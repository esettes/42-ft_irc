#ifndef COMMAND_DISPATCHER_HPP
#define COMMAND_DISPATCHER_HPP

#include <cstddef>
#include <map>
#include <string>
#include <vector>

class Server;
class Client;
class Channel;
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
        void handlePing(Client &client, const IrcMessage &message);
        void handlePong(Client &client, const IrcMessage &message);
        void handleQuit(Client &client, const IrcMessage &message);
        void handleCap(Client &client, const IrcMessage &message);
        static std::vector<std::string> splitCommaSeparatedValues(
            const std::string &valueList);
        void joinClientToSingleChannel(
            Client &client,
            const std::string &channelName,
            const std::string &providedKey);
        void handleJoin(Client &client, const IrcMessage &message);
        void partClientFromSingleChannel(
            Client &client,
            const std::string &channelName,
            const std::string &partReason,
            bool hasPartReason
        );
        void handlePart(Client &client, const IrcMessage &message);
        void handlePrivateMessage(Client &client, const IrcMessage &message);
        void handleNotice(Client &client, const IrcMessage &message);
        void handleTopic(Client &client, const IrcMessage &message);
        void handleInvite(Client &client, const IrcMessage &message);
        void handleKick(Client &client, const IrcMessage &message);
        void handleMode(Client &client, const IrcMessage &message);

        enum ModeAction
        {
            MODE_ADD,
            MODE_REMOVE
        };

        struct ModeOperation
        {
            ModeAction action;
            char mode;
            std::string argument;
            std::size_t numericArgument;

            ModeOperation();
        };

        bool parseChannelModeOperations(
            const std::vector<std::string> &params,
            std::vector<ModeOperation> &operations,
            char &unknownMode
        ) const;
        bool validateChannelModeOperations(
            Client &client,
            Channel &channel,
            std::vector<ModeOperation> &operations
        );
        void applyChannelModeOperations(
            Channel &channel,
            const std::vector<ModeOperation> &operations
        );
        void sendChannelModeIs(Client &client, const Channel &channel);
        void notifyChannelModeChanges(
            Client &client,
            Channel &channel,
            const std::vector<ModeOperation> &operations
        );

        bool isChannelTarget(const std::string &target) const;
        bool canJoinChannel(
            Client &client,
            Channel &channel,
            const std::string &providedKey
        );
        void sendTopicReply(Client &client, const Channel &channel);
        void applyTopicChange(
            Client &client,
            Channel &channel,
            const std::string &newTopic
        );
        void sendMessageToUser(
            Client &sender,
            const std::string &nickname,
            const std::string &messageText,
            const std::string &command,
            bool reportErrors
        );
        void sendMessageToChannel(
            Client &sender,
            const std::string &channelName,
            const std::string &messageText,
            const std::string &command,
            bool reportErrors
        );

    public:
        CommandDispatcher(Server &server);

        void execute(Client &client, const IrcMessage &message);
        ~CommandDispatcher();

};

#endif

