#include "Server.hpp"
#include "Client.hpp"
#include "Command.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <csignal>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Signals
// ---------------------------------------------------------------------------

// Set to 0 by SIGINT/SIGTERM. A signal handler may only safely write to a
// volatile sig_atomic_t, so the handler just flips this flag and run()
// leaves its loop normally, letting destructors close every fd.
static volatile sig_atomic_t g_running = 1;

static void handleStopSignal(int signum)
{
	(void)signum;
	g_running = 0;
}

static void installSignalHandlers()
{
	struct sigaction sa;

	std::memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;	// no SA_RESTART: poll() must return EINTR so we notice

	sa.sa_handler = handleStopSignal;
	if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1)
		throw std::runtime_error(std::string("sigaction: ") + std::strerror(errno));

	// send() to a client that already disconnected raises SIGPIPE, which
	// would kill the server. Ignore it; send() then just returns -1.
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, NULL) == -1)
		throw std::runtime_error(std::string("sigaction: ") + std::strerror(errno));
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

Server::Server(int port, const std::string &password)
	: _port(port), _password(password), _listenFd(-1)
{}

Server::~Server()
{
	for (std::map<int, Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		close(it->first);
		delete it->second;
	}
	_clients.clear();
	_pollFds.clear();
	closeListeningSocket();
}

// ---------------------------------------------------------------------------
// Listening socket
// ---------------------------------------------------------------------------

void Server::closeListeningSocket()
{
	if (_listenFd != -1)
	{
		close(_listenFd);
		_listenFd = -1;
	}
}

// Read errno before close(), because close() may overwrite it.
void Server::failListeningSocket(const std::string &step)
{
	std::string reason = std::strerror(errno);

	closeListeningSocket();
	throw std::runtime_error(step + ": " + reason);
}

void Server::initListeningSocket()
{
	// 1. Create an IPv4 TCP socket.
	_listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_listenFd == -1)
		failListeningSocket("socket");

	// 2. Allow immediate restart on the same port (skip TIME_WAIT).
	//    Must be set before bind().
	int opt = 1;
	if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
		failListeningSocket("setsockopt");

	// 3. Non-blocking, as the subject requires for every fd.
	if (fcntl(_listenFd, F_SETFL, O_NONBLOCK) == -1)
		failListeningSocket("fcntl");

	// 4. Bind to every local interface on the requested port.
	//    htonl/htons convert to network (big-endian) byte order.
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(static_cast<uint16_t>(_port));

	if (bind(_listenFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == -1)
		failListeningSocket("bind");

	// 5. Start accepting connections into the kernel's backlog queue.
	if (listen(_listenFd, SOMAXCONN) == -1)
		failListeningSocket("listen");

	// 6. The listening socket is always entry 0 of the poll set.
	pollfd entry;
	entry.fd = _listenFd;
	entry.events = POLLIN;	// readable = a connection is waiting in accept()
	entry.revents = 0;
	_pollFds.push_back(entry);
}

// ---------------------------------------------------------------------------
// Event loop
// ---------------------------------------------------------------------------

void Server::run()
{
	if (_listenFd == -1)
		throw std::logic_error("run() called before initListeningSocket()");

	installSignalHandlers();

	while (g_running)
	{
		// The one and only poll(): sleep until some fd is ready (-1 = no timeout).
		if (poll(&_pollFds[0], _pollFds.size(), -1) == -1)
		{
			if (errno == EINTR)	// interrupted by a signal: re-check g_running
				continue;
			throw std::runtime_error(std::string("poll: ") + std::strerror(errno));
		}

		// Walk by index: acceptClients() may push_back (reallocating the
		// vector) and disconnectClient() erases the current entry, which
		// shifts the next one into slot i -- so we only ++i when nothing
		// was removed. New clients land at the end with revents == 0.
		size_t i = 0;
		while (i < _pollFds.size())
		{
			const short revents = _pollFds[i].revents;

			if (revents == 0)
			{
				++i;
				continue;
			}

			if (_pollFds[i].fd == _listenFd)
			{
				if (revents & POLLIN)
					acceptClients();
				++i;
				continue;
			}

			// Socket error or invalid fd: nothing useful left to do.
			if (revents & (POLLERR | POLLNVAL))
			{
				disconnectClient(i);
				continue;
			}

			// Read before handling POLLHUP: a client may send data and close
			// at once, and recv() returns 0 once that data is consumed.
			// Each handler returns false if it removed entry i, in which case
			// the next fd now sits in slot i and must not be skipped.
			if (revents & POLLIN)
			{
				if (!readFromClient(i))
					continue;
			}
			else if (revents & POLLHUP)
			{
				disconnectClient(i);
				continue;
			}

			// No `continue` after reading: the same round can also be
			// writable. POLLOUT is only set while output is pending.
			if (revents & POLLOUT)
			{
				if (!flushClient(i))
					continue;
			}

			++i;
		}
	}
	std::cout << "\nShutting down" << std::endl;
}

// ---------------------------------------------------------------------------
// Clients
// ---------------------------------------------------------------------------

// Called when the listening socket is readable. Accept every pending
// connection: the socket is non-blocking, so accept() returns -1 once
// the backlog is empty. Only the return value is checked, never errno.
void Server::acceptClients()
{
	while (true)
	{
		struct sockaddr_in addr;
		socklen_t addrLen = sizeof(addr);

		int fd = accept(_listenFd, reinterpret_cast<struct sockaddr *>(&addr), &addrLen);
		if (fd == -1)
			return;

		if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
		{
			std::cerr << "fcntl: " << std::strerror(errno) << std::endl;
			close(fd);
			continue;
		}

		_clients[fd] = new Client(fd);

		pollfd entry;
		entry.fd = fd;
		entry.events = POLLIN;
		entry.revents = 0;
		_pollFds.push_back(entry);

		std::cout << "Client connected: fd " << fd
				  << " from " << inet_ntoa(addr.sin_addr) << std::endl;
	}
}

// Called only after poll() reported POLLIN for this client.
// Returns false if the client was disconnected (its entry was erased).
bool Server::readFromClient(size_t index)
{
	const int	fd = _pollFds[index].fd;
	char		buffer[512];

	ssize_t bytes = recv(fd, buffer, sizeof(buffer), 0);
	if (bytes <= 0)	// 0 = client closed the connection, -1 = error
	{
		disconnectClient(index);
		return false;
	}

	// 1. Append: never process the raw recv() chunk directly, it may be
	//    half a message or several messages glued together.
	Client *client = _clients[fd];
	client->appendToBuffer(buffer, static_cast<size_t>(bytes));

	// 2. Extract every complete line currently in the buffer.
	std::string line;
	while (client->extractLine(line))
		handleLine(*client, line);
	// TODO(command dispatch issue): once commands can disconnect a client
	// (QUIT, failed PASS...), stop this loop and return false when that
	// happens -- `client` would point to deleted memory.

	// 3. Whatever is left is an incomplete line, kept for the next recv().
	//    A valid line is at most 512 bytes including "\r\n", so a leftover
	//    of 512+ bytes can never become valid. Drop the client instead of
	//    letting it grow the buffer forever.
	if (client->bufferSize() >= MAX_LINE_LEN)
	{
		std::cerr << "[fd " << fd << "] line too long, disconnecting" << std::endl;
		disconnectClient(index);
		return false;
	}
	return true;
}

// Handles one complete IRC line (terminator already stripped).
void Server::handleLine(Client &client, const std::string &line)
{
	// RFC 2812: empty messages are silently ignored.
	if (line.empty())
		return;

	Command					cmd;
	Command::ParseStatus	status = Command::parse(line, cmd);

	// TODO(command dispatch issue): send the matching numeric error
	// (e.g. 417 ERR_INPUTTOOLONG) and execute the command.
	if (status != Command::PARSE_SUCCESS)
	{
		std::cout << "[fd " << client.getFd() << "] parse error (" << status
				  << "): " << line << std::endl;
		return;
	}
	std::cout << "[fd " << client.getFd() << "] command: " << cmd.getName()
			  << " (" << cmd.getParams().size() << " params)" << std::endl;
}

// Queues a message for a client and asks poll() to report when its socket
// is writable. Nothing is sent here: every send() happens in flushClient(),
// after poll() has said the socket can take data.
void Server::sendToClient(Client &client, const std::string &msg)
{
	client.queueMessage(msg);
	setPollOut(client.getFd(), true);
}

// Turns POLLOUT on or off in a client's poll entry. It is only on while
// output is pending: a socket is writable almost all the time, so leaving
// it on would wake poll() constantly with nothing to send (busy loop).
void Server::setPollOut(int fd, bool enable)
{
	for (size_t i = 0; i < _pollFds.size(); ++i)
	{
		if (_pollFds[i].fd != fd)
			continue;
		if (enable)
			_pollFds[i].events |= POLLOUT;
		else
			_pollFds[i].events &= ~POLLOUT;
		return;
	}
}

// Called only after poll() reported POLLOUT for this client.
// Returns false if the client was disconnected (its entry was erased).
bool Server::flushClient(size_t index)
{
	const int	fd = _pollFds[index].fd;
	Client		*client = _clients[fd];

	const std::string &out = client->getOutBuffer();
	if (out.empty())	// safety net: nothing to send, stop asking
	{
		setPollOut(fd, false);
		return true;
	}

	// send() returns how many bytes the kernel accepted, which can be
	// fewer than out.size(). Only the return value is checked, never errno.
	ssize_t sent = send(fd, out.data(), out.size(), 0);
	if (sent == -1)
	{
		disconnectClient(index);
		return false;
	}

	client->consumeOutput(static_cast<size_t>(sent));
	if (!client->hasPendingOutput())
		setPollOut(fd, false);	// all sent: back to POLLIN only
	return true;
}

// Closes the client's socket, frees the Client and removes its poll entry.
// TODO(disconnection issue): remove the client from channels and notify them.
void Server::disconnectClient(size_t index)
{
	const int fd = _pollFds[index].fd;

	std::map<int, Client *>::iterator it = _clients.find(fd);
	if (it != _clients.end())
	{
		delete it->second;
		_clients.erase(it);
	}
	close(fd);
	_pollFds.erase(_pollFds.begin() + index);

	std::cout << "Client disconnected: fd " << fd << std::endl;
}

// ---------------------------------------------------------------------------
// Getters
// ---------------------------------------------------------------------------

int Server::getPort() const
{
	return _port;
}

const std::string &Server::getPassword() const
{
	return _password;
}

int Server::getListenFd() const
{
	return _listenFd;
}
