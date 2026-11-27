#ifndef SERVER_HPP
# define SERVER_HPP

# include <string>
# include <vector>
# include <map>
# include <poll.h>

class Client;
class Channel;

// Max IRC message length, "\r\n" included (RFC 2812, section 2.3).
static const size_t	MAX_LINE_LEN = 512;

class Server
{
	private:
		int						_port;
		std::string				_password;
		int						_listenFd;	// -1 while no socket is open

		// The single poll set: index 0 is the listening socket,
		// every other entry is a client socket.
		std::vector<pollfd>		_pollFds;
		// Every connected client, by socket fd. The Server owns them.
		std::map<int, Client *>	_clients;

		// The server owns _listenFd: a copy would close the same fd twice.
		// Declared private and never implemented to forbid copying (C++98).
		Server(const Server &other);
		Server &operator=(const Server &other);

		void	closeListeningSocket();
		void	failListeningSocket(const std::string &step);

		void	acceptClients();
		bool	readFromClient(size_t index);
		void	disconnectClient(size_t index);
		void	handleLine(Client &client, const std::string &line);

		void	setPollOut(int fd, bool enable);
		bool	flushClient(size_t index);

	public:
		Server(int port, const std::string &password);
		~Server();

		// socket -> setsockopt -> fcntl -> bind -> listen.
		// Throws std::runtime_error on failure (the fd is closed first).
		void				initListeningSocket();

		// Runs the poll() loop until SIGINT/SIGTERM.
		// Throws std::runtime_error if poll() itself fails.
		void				run();

		void				sendToClient(Client &client, const std::string &msg);

		int					getPort() const;
		const std::string	&getPassword() const;
		int					getListenFd() const;

		// Used for nickname collision checks and message recipient lookup.
		// Returns NULL if not found; the caller must exclude itself when needed.
		Client				*findClientByNick(const std::string &nick) const;

		Channel				*findChannelByName(const std::string &name) const;
};

#endif
