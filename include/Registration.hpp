#ifndef REGISTRATION_HPP
# define REGISTRATION_HPP

# include <string>

class   Client;
class   Command;

class   Registration
{
    public:
        enum    PassResult
        {
            PASS_ACCEPTED,
            PASS_NOT_PASS_COMMAND,
            PASS_MISSING_PARAMETER,
            PASS_INVALID_PARAMETER_COUNT,
            PASS_ALREADY_REGISTERED,
            PASS_OUT_OF_ORDER,
            PASS_PASSWORD_MISMATCH
        };
        enum UserResult
        {
            USER_ACCEPTED,
            USER_NOT_USER_COMMAND,
            USER_MISSING_PARAMETERS,
            USER_INVALID_PARAMETER_COUNT,
            USER_ALREADY_REGISTERED,
            USER_INVALID_USERNAME,
            USER_INVALID_MODE,
            USER_PASS_REQUIRED
        };
/*
 * Integration TODO with Ana:
 * - Agree who parses and dispatches each Command with its Client.
 * - Agree how the dispatcher queries its live Client collection for
 *   nickname collisions using IRC case-mapping, excluding this Client.
 * - Handlers return results; the dispatcher maps them to IRC replies
 *   and queues those replies through the transport layer.
 * - Decide whether disconnecting removes a nickname automatically
 *   with its Client, or needs a hook such as onClientDisconnected().
 */
        enum NickResult
        {
            NICK_ACCEPTED,
            NICK_NOT_NICK_COMMAND,
            NICK_MISSING_PARAMETER,
            NICK_INVALID_PARAMETER_COUNT,
            NICK_INVALID_NICKNAME,
            NICK_ALREADY_IN_USE,
            NICK_PASS_REQUIRED
        };

// Compares the PASS parameter with the configured server password;
// returns an internal result and performs no network I/O.
        static PassResult   handlePass(Client &client, const Command &command, const std::string &serverPassword);

        static UserResult   handleUser(Client &client, const Command &command);

// Temporary integration contract: the caller supplies the result of
// checking the live client collection; replace this input if needed
// once the shared nickname-lookup API is agreed (bool nicknameInUse).
        static NickResult handleNick(Client &client, const Command &command, bool nicknameInUse);
};

#endif
