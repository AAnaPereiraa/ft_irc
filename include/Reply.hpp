#ifndef REPLY_HPP
# define REPLY_HPP

# include <string>

class Client;

// Builds IRC reply lines without CRLF; it does not queue or send them.
class Reply
{
    public:
        // Builds a numeric reply. Uses "*" as target if the client has no nick.
        static std::string numeric(const std::string &serverName, const Client &client, const std::string &code, const std::string &params, const std::string &text);
        // Builds 001 using the nickname and username stored in the client.
        static std::string welcome(const std::string &serverName, const Client &client, const std::string &hostname);
        // Builds 001 from explicit identity fields, also used to check its size.
        static std::string welcome(const std::string &serverName, const std::string &nickname, const std::string &username, const std::string &hostname);
        // Builds 002 with the server name and version.
        static std::string yourHost(const std::string &serverName, const Client &client, const std::string &version);
        // Builds 003 with the server creation date.
        static std::string created(const std::string &serverName, const Client &client, const std::string &creationDate);
        // Builds 004; the mode fields currently use temporary "*" placeholders.
        static std::string myInfo(const std::string &serverName, const Client &client, const std::string &version);
};

#endif
