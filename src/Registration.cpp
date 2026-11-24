#include "Registration.hpp"
#include "Client.hpp"
#include "Command.hpp"
#include <vector>

// RFC 2812, section 2.3.1: validates the USER <user> field.
static bool isValidUsername(const std::string &username)
{
    if (username.empty())
        return (false);

    for (std::string::size_type i = 0; i < username.size(); ++i)
    {
        unsigned char   c = username[i];
        if (c == '\0' || c == '\r' || c == '\n' || c == ' ' || c == '@')
            return (false);
    }
    return (true);
}

Registration::PassResult    Registration::handlePass(Client &client, const Command &command, const std::string &serverPassword)
{
    if (command.getName() != "PASS")
        return (PASS_NOT_PASS_COMMAND);

    if (client.isRegistered())
        return (PASS_ALREADY_REGISTERED);

    const std::vector<std::string>  &params = command.getParams();
    if (params.empty())
        return (PASS_MISSING_PARAMETER);
    if (params.size() != 1)
        return (PASS_INVALID_PARAMETER_COUNT);

// Current policy: reject PASS once both NICK and USER information are present.
    if (client.hasNickname() && client.hasUserInfo())
        return (PASS_OUT_OF_ORDER);

    if (params[0] != serverPassword)
        return (PASS_PASSWORD_MISMATCH);

    client.markPasswordAccepted();
    return (PASS_ACCEPTED);
}

Registration::UserResult    Registration::handleUser(Client &client, const Command &command)
{
    if (command.getName() != "USER")
        return (USER_NOT_USER_COMMAND);

    if (client.isRegistered())
        return (USER_ALREADY_REGISTERED);

    if (!client.hasAcceptedPassword())
        return (USER_PASS_REQUIRED);

// RFC 2812 USER has four fields: <user> <mode> <unused> <realname>.
    const std::vector<std::string> &params = command.getParams();
    if (params.size() < 4)
        return (USER_MISSING_PARAMETERS);
    if (params.size() > 4)
        return (USER_INVALID_PARAMETER_COUNT);

    if (!isValidUsername(params[0]))
        return (USER_INVALID_USERNAME);

// RFC 2812 requires <mode> to be numeric.
    bool numericMode = !params[1].empty();
    for (std::string::size_type i = 0; i < params[1].size(); ++i)
    {
        if (params[1][i] < '0' || params[1][i] > '9')
            numericMode = false;
    }
    if (!numericMode)
        return (USER_INVALID_MODE);

    client.setUserInfo(params[0], params[3]);
    return (USER_ACCEPTED);
}

Registration::NickResult    Registration::handleNick(Client &client, const Command &command, bool nicknameInUse)
{
    if (command.getName() != "NICK")
        return (NICK_NOT_NICK_COMMAND);

    if (!client.isRegistered() && !client.hasAcceptedPassword())
        return (NICK_PASS_REQUIRED);

    const std::vector<std::string> &params = command.getParams();
    if (params.empty())
        return (NICK_MISSING_PARAMETER);
    if (params.size() != 1)
        return (NICK_INVALID_PARAMETER_COUNT);

// RFC 2812, section 1.2.1: nickname length is limited to nine characters.
    const std::string &nickname = params[0];
    if (nickname.empty() || nickname.size() > 9)
        return (NICK_INVALID_NICKNAME);

// RFC 2812, section 2.3.1: validates the nickname character grammar.
    for (std::string::size_type i = 0; i < nickname.size(); ++i)
    {
        unsigned char c = nickname[i];

        bool letter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        bool digit = c >= '0' && c <= '9';
        bool special = (c >= '[' && c <= '`') || (c >= '{' && c <= '}');

        if (i == 0)
        {
            if (!letter && !special)
                return (NICK_INVALID_NICKNAME);
        }
        else if (!letter && !digit && !special && c != '-')
            return (NICK_INVALID_NICKNAME);
    }

// The caller checks the live client collection for a nickname collision.
    if (nicknameInUse)
        return (NICK_ALREADY_IN_USE);

    client.setNickname(nickname);
    return (NICK_ACCEPTED);
}
