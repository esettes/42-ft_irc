#ifndef COMMAND_DISPATCHER_HPP
#define COMMAND_DISPATCHER_HPP

#include <cstddef>
#include <map>
#include <string>

class Server;
class Client;
struct IrcMessage;

class CommandDispatcher
{
public:
    CommandDispatcher(Server &server);

    void execute(Client &client, const IrcMessage &message);

private:
    /* Type that points to member functions of CommandDispatcher */
    typedef void (CommandDispatcher::*CommandHandler)(
        Client &client,
        const IrcMessage &message
    );

    struct CommandDefinition
    {
        CommandHandler handler;
        std::size_t minParams;
        bool requiresRegistration;

        CommandDefinition(
            CommandHandler handler,
            std::size_t minParams,
            bool requiresRegistration
        );
    };

    typedef std::map<std::string, CommandDefinition> CommandMap;

    Server &server;
    CommandMap cmmds;

    CommandDispatcher(const CommandDispatcher &other);
    CommandDispatcher &operator=(const CommandDispatcher &other);
    ~CommandDispatcher();

    void registerCommands();

    void handlePass(Client &client, const IrcMessage &message);
    void handleNick(Client &client, const IrcMessage &message);
    void handleUser(Client &client, const IrcMessage &message);
    void handleJoin(Client &client, const IrcMessage &message);
    void handlePrivateMessage(Client &client, const IrcMessage &message);
};

#endif

