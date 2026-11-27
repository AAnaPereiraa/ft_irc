#include "Dispatcher.hpp"
#include "Command.hpp"
#include "Registration.hpp"
#include "Client.hpp"
#include "Server.hpp"
#include "Reply.hpp"

typedef bool (*CommandHandler)(Server &, Client &, const Command &, const std::string &);

struct CommandEntry
{
    const char *name;
    CommandHandler handler;
};

static const CommandEntry commands[] = {
    {"PASS", Registration::handlePass},
    {"NICK", Registration::handleNick},
    {"USER", Registration::handleUser},
    {"PING", 0},
    {"PONG", 0},
    {"QUIT", 0},
    {"NOTICE", 0},
    {"JOIN", 0},
    {"PART", 0},
    {"PRIVMSG", 0},
    {"KICK", 0},
    {"INVITE", 0},
    {"TOPIC", 0},
    {"MODE", 0}
};

static const CommandEntry *findCommand(const std::string &name)
{
    for (std::size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i)
    {
        if (name == commands[i].name)
            return (&commands[i]);
    }
    return (0);
}

static bool isKnownCommand(const std::string &name)
{
    return (findCommand(name) != 0);
}

bool Dispatcher::dispatchHandler(Server &server, Client &client, const Command &command, const std::string &serverName, bool &accepted)
{
    accepted = false;

    const CommandEntry *entry = findCommand(command.getName());

    if (entry == 0 || entry->handler == 0)
        return (false);

    accepted = entry->handler(server, client, command, serverName);
    return (true);
}

bool Dispatcher::rejectBeforeRegistration(Server &server, Client &client, const Command &command, const std::string &serverName)
{
    const std::string &name = command.getName();

    if (client.isRegistered() || !isKnownCommand(name))
        return (false);

    if (name == "NOTICE")
        return (true);

    if (name == "PASS" || name == "NICK" || name == "USER" || name == "PING" || name == "PONG" || name == "QUIT")
        return (false);

    server.sendToClient(client, Reply::numeric(serverName, client, "451", "", "You have not registered"));
    return (true);
}

bool Dispatcher::dispatch(Server &server, Client &client, const Command &command, const std::string &serverName, const std::string &version, const std::string &creationDate, bool &accepted)
{
    accepted = false;

    if (rejectBeforeRegistration(server, client, command, serverName))
        return (true);

    if (!dispatchHandler(server, client, command, serverName, accepted))
    {
        if (command.getName() == "NOTICE")
            return (true);

        if (isKnownCommand(command.getName()))
            return (false);

        std::string reply = Reply::numeric(serverName, client, "421", command.getName(), "Unknown command");

        // A very long command name may not fit in the numeric reply.
        if (reply.size() > MAX_LINE_LEN - 2)
            reply = Reply::numeric(serverName, client, "421", "*", "Unknown command");

        server.sendToClient(client, reply);
        return (true);
    }

    if (accepted && client.tryCompleteRegistration())
    {
        server.sendToClient(client, Reply::welcome(serverName, client, client.getHostname()));
        server.sendToClient(client, Reply::yourHost(serverName, client, version));
        server.sendToClient(client, Reply::created(serverName, client, creationDate));
        server.sendToClient(client, Reply::myInfo(serverName, client, version));
    }

    return (true);
}
