#ifndef SERVER_HPP
# define SERVER_HPP

# include <string>

class Server
{
	private:
		int			_port;
		std::string	_password;
		int			_listenFd;	// -1 while no socket is open

		// The server owns _listenFd: a copy would close the same fd twice.
		// Declared private and never implemented to forbid copying (C++98).
		Server(const Server &other);
		Server &operator=(const Server &other);

		void	closeListeningSocket();
		void	failListeningSocket(const std::string &step);

	public:
		Server(int port, const std::string &password);
		~Server();

		// socket -> setsockopt -> fcntl -> bind -> listen.
		// Throws std::runtime_error on failure (the fd is closed first).
		void				initListeningSocket();

		int					getPort() const;
		const std::string	&getPassword() const;
		int					getListenFd() const;
};

#endif
