
#include <string>
#include <iostream>
#include <cctype>
#include <cstdlib>
#include <cerrno>
#include <exception>
#include "Server.hpp"

bool isDigit(const char *arg)
{
	if (arg[0] == '\0')
		return false;
	for (int i = 0; arg[i] != '\0'; i++)
	{
		if (!std::isdigit(static_cast<unsigned char>(arg[i])))
			return false;
	}
	return true;
}

int main(int argc, char **argv)
{
	if (argc != 3 || argv[2][0] == '\0')
	{
		std::cerr << "Please add Port and Password" << std::endl;
        return 1;
	}

	if (isDigit(argv[1]) == false)
	{
		std::cerr << "Port must contain only digits" << std::endl;
    	return 1;
	}

	char *end = NULL;
	errno = 0;
	long parsedPort = std::strtol(argv[1], &end, 10);

	if (errno == ERANGE || parsedPort > 65535 || parsedPort < 1)
	{
		std::cerr << "Port must be in range 1 to 65535" << std::endl;
    	return 1;
	}

	int port = static_cast<int>(parsedPort);
	std::string password = argv[2];

	try
	{
		Server server(port, password);
		server.initListeningSocket();
		std::cout << "Listening on port " << server.getPort() << std::endl;
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
