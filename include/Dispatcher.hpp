#ifndef DISPATCHER_HPP
# define DISPATCHER_HPP

# include <string>

class Server;
class Client;
class Command;

class Dispatcher
{
    private:
        // Runs a command handler from the table, if one exists.
        // Returns true if a handler ran; accepted receives its result.
        static bool dispatchHandler(Server &server, Client &client, const Command &command, const std::string &serverName, bool &accepted);
        // Rejects known commands that require registration for an unregistered client.
        // Queues 451 and returns true when the command is rejected.
        static bool rejectBeforeRegistration(Server &server, Client &client, const Command &command, const std::string &serverName);

    public:
        // Applies registration restrictions, routes handlers and handles unknown commands.
        // Returns true if handled; accepted receives the handler result when one runs.
        static bool dispatch(Server &server, Client &client, const Command &command, const std::string &serverName, const std::string &version, const std::string &creationDate, bool &accepted);
};

#endif
