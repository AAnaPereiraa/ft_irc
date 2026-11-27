#include "Registration.hpp"
#include "Client.hpp"
#include "Command.hpp"
#include "Server.hpp"
#include "Reply.hpp"
#include <vector>

// RFC 2812, section 2.3.1: validates the USER <user> field.
static bool isValidUsername(const std::string &username)
{
    if (username.empty() || username.size() > 10)
        return (false);

    for (std::string::size_type i = 0; i < username.size(); ++i)
    {
        unsigned char   c = username[i];
        if (c == '\0' || c == '\r' || c == '\n' || c == ' ' || c == '@')
            return (false);
    }
    return (true);
}

bool Registration::handlePass(Server &server, Client &client, const Command &command, const std::string &serverName)
{
    if (client.isRegistered())
    {
        server.sendToClient(client, Reply::numeric(serverName, client, "462", "", "Unauthorized command (already registered)"));
        return (false);
    }

    client.clearPasswordAccepted();

    const std::vector<std::string> &params = command.getParams();

    if (params.empty())
    {
        server.sendToClient(client, Reply::numeric(serverName, client, "461", "PASS", "Not enough parameters"));
        return (false);
    }

    if (params[0] != server.getPassword())
    {
        server.sendToClient(client, Reply::numeric(serverName, client, "464", "", "Password incorrect"));
        return (false);
    }

    client.markPasswordAccepted();
    return (true);
}

bool Registration::handleUser(Server &server, Client &client, const Command &command, const std::string &serverName)
{
    if (client.isRegistered())
    {
        server.sendToClient(client, Reply::numeric(
            serverName, client, "462", "",
            "Unauthorized command (already registered)"));
        return (false);
    }

    if (client.hasUserInfo())
    {
        server.sendToClient(client, Reply::numeric(
            serverName, client, "462", "", "Unauthorized command (already registered)"));
        return (false);
    }

    if (!client.hasAcceptedPassword())
    {
        server.sendToClient(client, Reply::numeric(
            serverName, client, "464", "", "Password required"));
        return (false);
    }

    const std::vector<std::string> &params = command.getParams();

    if (params.size() < 4)
    {
        server.sendToClient(client, Reply::numeric(
            serverName, client, "461", "USER", "Not enough parameters"));
        return (false);
    }

    if (!isValidUsername(params[0]))
    {
        server.sendToClient(client, Reply::numeric(
            serverName, client, "400", "USER", "Invalid username"));
        return (false);
    }

    client.setUserInfo(params[0], params[3]);
    return (true);
}

bool Registration::handleNick(Server &server, Client &client, const Command &command, const std::string &serverName)
{
    if (!client.isRegistered() && !client.hasAcceptedPassword())
    {
        server.sendToClient(client, Reply::numeric(serverName, client, "464", "", "Password required"));
        return (false);
    }

    const std::vector<std::string> &params = command.getParams();

    if (params.empty() || params[0].empty())
    {
        server.sendToClient(client, Reply::numeric(serverName, client, "431", "", "No nickname given"));
        return (false);
    }

    const std::string &nickname = params[0];

    // RFC 2812, sections 1.2.1 and 2.3.1.
    bool validNick = nickname.size() <= 9;

    for (std::string::size_type i = 0; validNick && i < nickname.size(); ++i)
    {
        unsigned char c = nickname[i];

        bool letter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        bool digit = c >= '0' && c <= '9';
        bool special = (c >= '[' && c <= '`') || (c >= '{' && c <= '}');

        if (i == 0)
            validNick = letter || special;
        else
            validNick = letter || digit || special || c == '-';
    }

    if (!validNick)
    {
        std::string replyNick = nickname;

        if (replyNick.size() > 9 || replyNick[0] == ':')
            replyNick = "*";
        else
        {
            for (std::string::size_type i = 0; i < replyNick.size(); ++i)
            {
                unsigned char c = replyNick[i];
                if (c < 33 || c > 126)
                {
                    replyNick = "*";
                    break;
                }
            }
        }

        server.sendToClient(client, Reply::numeric(serverName, client, "432", replyNick, "Erroneous nickname"));
        return (false);
    }

    Client *existing = server.findClientByNick(nickname);

    if (existing != NULL && existing != &client)
    {
        server.sendToClient(client, Reply::numeric(serverName, client, "433", nickname, "Nickname is already in use"));
        return (false);
    }

    client.setNickname(nickname);
    return (true);
}
