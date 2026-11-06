#ifndef SERVER_HPP
# define SERVER_HPP

# include <string>

class Server
{
	private:
		int			_port;
		std::string	_password;

	public:
		Server(int port, const std::string &password);
		int getPort() const;
		const std::string &getPassword() const;
		~Server();
};

#endif
