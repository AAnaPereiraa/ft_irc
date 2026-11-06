#include "Server.hpp"

Server::Server(int port, const std::string &password): _port(port), _password(password){}

Server::~Server(){}

int Server::getPort() const
{
	return _port;
}

const std::string &Server::getPassword() const
{
	return _password;
}
