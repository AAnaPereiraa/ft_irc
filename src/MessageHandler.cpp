#include "MessageHandler.hpp"
#include "Command.hpp"
#include "Client.hpp"
#include "Server.hpp"
#include "Reply.hpp"
#include <stdexcept>
#include <string>
#include <vector>

static MessageHandler::Result validateMessage(const Command &command, const std::string &expectedCommand)
{
    if (command.getName() != expectedCommand)
        return (MessageHandler::MESSAGE_WRONG_COMMAND);

    const std::vector<std::string> &params = command.getParams();

    if (params.empty() || params[0].empty())
        return (MessageHandler::MESSAGE_MISSING_TARGET);

    if (params.size() == 1)
        return (MessageHandler::MESSAGE_MISSING_TEXT);

    if (params.size() > 2)
        return (MessageHandler::MESSAGE_INVALID_PARAMETER_COUNT);

    if (params[1].empty())
        return (MessageHandler::MESSAGE_MISSING_TEXT);

    return (MessageHandler::MESSAGE_VALID);
}

MessageHandler::Result MessageHandler::validatePrivmsg(const Command &command)
{
    return (validateMessage(command, "PRIVMSG"));
}

MessageHandler::Result MessageHandler::validateNotice(const Command &command)
{
    return (validateMessage(command, "NOTICE"));
}

std::string MessageHandler::formatPrivmsg(const Client &sender, const std::string &hostname, const std::string &target, const std::string &text)
{
    std::string prefix = sender.getNickname() + "!" + sender.getUsername() + "@" + hostname;

    return (":" + prefix + " PRIVMSG " + target + " :" + text);
}

bool MessageHandler::validatePrivmsgOrReply(Server &server, Client &sender, const Command &command, const std::string &serverName)
{
    Result result = validatePrivmsg(command);

    if (result == MESSAGE_VALID)
        return (true);

    if (result == MESSAGE_WRONG_COMMAND)
        throw std::logic_error("validatePrivmsgOrReply: expected PRIVMSG");

    if (result == MESSAGE_MISSING_TARGET)
        server.sendToClient(sender, Reply::numeric(serverName, sender, "411", "", "No recipient given (PRIVMSG)"));
    else if (result == MESSAGE_MISSING_TEXT)
        server.sendToClient(sender, Reply::numeric(serverName, sender, "412", "", "No text to send"));
    else
        server.sendToClient(sender, Reply::numeric(serverName, sender, "400", "PRIVMSG", "Too many parameters"));

    return (false);
}

void MessageHandler::deliverToNick(Server &server, Client &sender, const std::string &serverName, const std::string &hostname, const std::string &target, const std::string &text)
{
    Client *recipient = server.findClientByNick(target);

    if (recipient == NULL || !recipient->isRegistered())
    {
        std::string error = Reply::numeric(serverName, sender, "401", target, "No such nick");

        if (error.size() > MAX_LINE_LEN - 2)
            error = Reply::numeric(serverName, sender, "401", "*", "No such nick");

        server.sendToClient(sender, error);
        return;
    }

    std::string message = formatPrivmsg(sender, hostname, target, text);

    if (message.size() > MAX_LINE_LEN - 2)
    {
        server.sendToClient(sender, Reply::numeric(serverName, sender, "400", "PRIVMSG", "Message too long"));
        return;
    }

    server.sendToClient(*recipient, message);
}

bool MessageHandler::handlePrivmsgToNick(Server &server, Client &sender, const Command &command, const std::string &serverName, const std::string &hostname)
{
    if (!sender.isRegistered())
    {
        server.sendToClient(sender, Reply::numeric(serverName, sender, "451", "", "You have not registered"));
        return (true);
    }

    if (!validatePrivmsgOrReply(server, sender, command, serverName))
        return (true);

    const std::vector<std::string> &params = command.getParams();
    const std::string &target = params[0];

    if (target[0] == '#' || target[0] == '&' || target[0] == '+' || target[0] == '!')
        return (false);

    deliverToNick(server, sender, serverName, hostname, target, params[1]);
    return (true);
}
