#ifndef DISPATCHER_HPP
# define DISPATCHER_HPP

# include <string>

class Server;
class Client;
class Command;

class Dispatcher
{
    public:
        static bool dispatchRegistration(Server &server, Client &client, const Command &command, const std::string &serverName, bool &accepted);
};

#endif