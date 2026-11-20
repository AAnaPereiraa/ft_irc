#include "Client.hpp"

Client::Client(int fd) : _fd(fd)
{}

// The Server closes the fd, so the Client does not.
Client::~Client()
{}

int Client::getFd() const
{
	return _fd;
}
