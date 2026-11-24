#ifndef MESSAGE_HANDLER_HPP
# define MESSAGE_HANDLER_HPP

class   Command;

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

        static Result   validatePrivmsg(const Command &command);
        static Result   validateNotice(const Command &command);
};

#endif