#ifndef MESSAGE_HANDLER_HPP
# define MESSAGE_HANDLER_HPP

# include <string>

class   Client;
class   Command;
class   Server;

class   MessageHandler
{
    public:
        enum    Result
        {
            MESSAGE_VALID,
            MESSAGE_WRONG_COMMAND,
            MESSAGE_MISSING_TARGET,
            MESSAGE_MISSING_TEXT,
            MESSAGE_INVALID_PARAMETER_COUNT
        };

        static Result       validatePrivmsg(const Command &command);
        static Result       validateNotice(const Command &command);
        // Builds a PRIVMSG line without CRLF; does not validate or send it.
        static std::string  formatPrivmsg(const Client &sender, const std::string &hostname, const std::string &target, const std::string &text);
        // Validates PRIVMSG and queues an error if its parameters are invalid.
        static bool         validatePrivmsgOrReply(Server &server, Client &sender, const Command &command, const std::string &serverName);

        // Delivers a validated PRIVMSG to a nickname, or queues an error.
        static void deliverToNick(Server &server, Client &sender, const std::string &serverName, const std::string &hostname, const std::string &target, const std::string &text);
        // Handles a PRIVMSG to a nick. Returns false for a channel target.
        static bool handlePrivmsgToNick(Server &server, Client &sender, const Command &command, const std::string &serverName, const std::string &hostname);
};

#endif
