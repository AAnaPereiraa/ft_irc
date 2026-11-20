#include "Server.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <stdexcept>

Server::Server(int port, const std::string &password)
	: _port(port), _password(password), _listenFd(-1)
{}

Server::~Server()
{
	closeListeningSocket();
}

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
}

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
