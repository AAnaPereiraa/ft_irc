#include "Dispatcher.hpp"
#include "Command.hpp"
#include "Registration.hpp"

bool Dispatcher::dispatchRegistration(Server &server, Client &client, const Command &command, const std::string &serverName, bool &accepted)
{
    accepted = false;

    if (command.getName() == "PASS")
        accepted = Registration::handlePass(server, client, command, serverName);
    else if (command.getName() == "NICK")
        accepted = Registration::handleNick(server, client, command, serverName);
    else if (command.getName() == "USER")
        accepted = Registration::handleUser(server, client, command, serverName);
    else
        return (false);

    return (true);
}
