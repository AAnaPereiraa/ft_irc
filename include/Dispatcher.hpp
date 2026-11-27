#ifndef DISPATCHER_HPP
# define DISPATCHER_HPP

# include <string>

class Server;
class Client;
class Command;

class Dispatcher
{
    public:
        // Routes PASS, NICK and USER to their registration handlers.
        // Returns true if routed; accepted receives the handler result.
        static bool dispatchRegistration(Server &server, Client &client, const Command &command, const std::string &serverName, bool &accepted);
        // Blocks listed channel and messaging commands before registration.
        // Queues 451 and returns true if the command was blocked.
        static bool rejectBeforeRegistration(Server &server, Client &client, const Command &command, const std::string &serverName);
        // Checks the registration restriction, then routes registration commands.
        // Returns true if handled; accepted is true only if a handler accepts it.
        static bool dispatch(Server &server, Client &client, const Command &command, const std::string &serverName, const std::string &hostname, const std::string &version, const std::string &creationDate, bool &accepted);
};

#endif
