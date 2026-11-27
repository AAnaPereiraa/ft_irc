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
