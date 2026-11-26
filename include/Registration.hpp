#ifndef REGISTRATION_HPP
# define REGISTRATION_HPP

# include <string>

class   Client;
class   Command;
class   Server;

class   Registration
{
    public:
        static bool handlePass(Server &server, Client &client, const Command &command, const std::string &serverName);

        static bool handleUser(Server &server, Client &client, const Command &command, const std::string &serverName);

        static bool handleNick(Server &server, Client &client, const Command &command, const std::string &serverName);
};

#endif
