#ifndef REPLY_HPP
# define REPLY_HPP

# include <string>

class Client;

class Reply
{
    public:
        static std::string numeric(const std::string &serverName, const Client &client, const std::string &code, const std::string &params, const std::string &text);
};

#endif
