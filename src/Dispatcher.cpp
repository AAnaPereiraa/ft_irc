#include "Dispatcher.hpp"
#include "Command.hpp"
#include "Registration.hpp"
#include "Client.hpp"
#include "Server.hpp"
#include "Reply.hpp"
#include <vector>

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

bool Dispatcher::rejectBeforeRegistration(Server &server, Client &client, const Command &command, const std::string &serverName)
{
    if (client.isRegistered())
        return (false);

    const std::string &name = command.getName();

    if (name != "JOIN" && name != "PART" && name != "PRIVMSG" && name != "KICK" && name != "INVITE" && name != "TOPIC" && name != "MODE")
        return (false);

    server.sendToClient(client, Reply::numeric(serverName, client, "451", "", "You have not registered"));
    return (true);
}

bool Dispatcher::dispatch(Server &server, Client &client, const Command &command, const std::string &serverName, const std::string &hostname, const std::string &version, const std::string &creationDate, bool &accepted)
{
    accepted = false;

    if (rejectBeforeRegistration(server, client, command, serverName))
        return (true);

    // Reject usernames that would make the 001 welcome exceed 512 bytes,
    // including CRLF. Use the maximum nick length (9) so this also works
    // when USER arrives before NICK; this is a conservative limit.
    // Error 400 is project-specific, not defined by the RFC for USER.
    const std::vector<std::string> &params = command.getParams();

    if (command.getName() == "USER" && !client.isRegistered()
        && client.hasAcceptedPassword() && params.size() >= 4)
    {
        std::string longestNick(9, 'a');
        std::string welcome = Reply::welcome(serverName, longestNick,
                                             params[0], hostname);

        if (welcome.size() > MAX_LINE_LEN - 2)
        {
            server.sendToClient(client, Reply::numeric(
                serverName, client, "400", "USER",
                "Username too long for welcome reply"));
            return (true);
        }
    }

    if (!dispatchRegistration(server, client, command, serverName, accepted))
        return (false);

    if (accepted && client.tryCompleteRegistration())
    {
        server.sendToClient(client, Reply::welcome(serverName, client, hostname));
        server.sendToClient(client, Reply::yourHost(serverName, client, version));
        server.sendToClient(client, Reply::created(serverName, client, creationDate));
        server.sendToClient(client, Reply::myInfo(serverName, client, version));
    }

    return (true);
}
