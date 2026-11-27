#ifndef CHANNEL_HPP
# define CHANNEL_HPP

# include <string>

class Client;
class Server;

class Channel
{
        public:
            bool hasMember(const Client &client) const;
            void sendToOtherMembers(Server &server, const Client &sender, const std::string &line) const;
};

#endif
