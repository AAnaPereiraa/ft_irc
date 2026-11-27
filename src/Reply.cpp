#include "Reply.hpp"
#include "Client.hpp"

std::string Reply::numeric(const std::string &serverName, const Client &client, const std::string &code, const std::string &params, const std::string &text)
{
    std::string target = client.hasNickname() ? client.getNickname() : "*";

    std::string line = ":" + serverName + " " + code + " " + target;

    if (!params.empty())
        line += " " + params;

    line += " :" + text;
    return (line);
}

std::string Reply::welcome(const std::string &serverName, const std::string &nickname, const std::string &username, const std::string &hostname)
{
    std::string target = nickname.empty() ? "*" : nickname;
    std::string identity = nickname + "!" + username + "@" + hostname;

    return (":" + serverName + " 001 " + target
            + " :Welcome to the Internet Relay Network " + identity);
}

std::string Reply::welcome(const std::string &serverName, const Client &client, const std::string &hostname)
{
    return (Reply::welcome(serverName, client.getNickname(), client.getUsername(), hostname));
}

std::string Reply::yourHost(const std::string &serverName, const Client &client, const std::string &version)
{
    return (Reply::numeric(serverName, client, "002", "", "Your host is " + serverName + ", running version " + version));
}

std::string Reply::created(const std::string &serverName, const Client &client, const std::string &creationDate)
{
    return (Reply::numeric(serverName, client, "003", "", "This server was created " + creationDate));
}

std::string Reply::myInfo(const std::string &serverName, const Client &client, const std::string &version)
{
    // Temporary placeholder: replace "*" with available user modes.
    std::string params = serverName + " " + version + " *";

    // Temporary placeholder: replace "*" with available channel modes.
    // Client compatibility with these placeholders still needs testing.
    return (Reply::numeric(serverName, client, "004", params, "*"));
}
