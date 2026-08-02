#ifndef COMMAND_DISPATCHER_HPP
#define COMMAND_DISPATCHER_HPP

#include <cstddef>
#include <map>
#include <string>

class Server;
class Client;
struct IrcMessage;

/**
 * @file CommandDispatcher.hpp
 * @brief Declares the command router responsible for dispatching IRC commands to handlers.
 * 
 * @param server A reference to the main Server instance, used to access clients and channels.
 * @param cmmds A mapping of command strings to their corresponding handler functions, minimum parameter requirements, and registration requirements.
 * @param CommandHandler A type alias for member function pointers that handle specific IRC commands.
 * @param CommandDefinition A struct that encapsulates a command handler, its minimum
 */
class CommandDispatcher
{
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
        

        void registerCommands();

        void handlePass(Client &client, const IrcMessage &message);
        void handleNick(Client &client, const IrcMessage &message);
        void handleUser(Client &client, const IrcMessage &message);
        void handleJoin(Client &client, const IrcMessage &message);
        void handlePrivateMessage(Client &client, const IrcMessage &message);

    public:
        CommandDispatcher(Server &server);

        void execute(Client &client, const IrcMessage &message);
        ~CommandDispatcher();

};

#endif

