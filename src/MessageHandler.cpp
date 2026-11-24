#include "MessageHandler.hpp"
#include "Command.hpp"
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
    {
        if (command.hasTrailingParam())
            return (MessageHandler::MESSAGE_MISSING_TARGET);
        return (MessageHandler::MESSAGE_MISSING_TEXT);
    }

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