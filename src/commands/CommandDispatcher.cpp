#include "CommandDispatcher.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "IrcMessage.hpp"

CommandDispatcher::CommandDefinition::CommandDefinition(
    CommandHandler handler,
    std::size_t minParams,
    bool requiresRegistration
) : handler(handler), minParams(minParams), requiresRegistration(requiresRegistration)
{
}

CommandDispatcher::CommandDispatcher(Server &server) : server(server)
{
    registerCommands();
}

void CommandDispatcher::registerCommands()
{
    cmmds.insert(
        std::make_pair(
            "PASS",
            CommandDefinition(&CommandDispatcher::handlePass, 1, false))
    );
    cmmds.insert(
        std::make_pair(
            "NICK",
            CommandDefinition(&CommandDispatcher::handleNick, 1, false))
    );
    cmmds.insert(
        std::make_pair(
            "USER",
            CommandDefinition(&CommandDispatcher::handleUser, 4, false))
    );
    cmmds.insert(
        std::make_pair(
            "PING",
            CommandDefinition(&CommandDispatcher::handlePing, 0, false))
    );
    cmmds.insert(
        std::make_pair(
            "JOIN",
            CommandDefinition(&CommandDispatcher::handleJoin, 1, true))
    );
    cmmds.insert(
        std::make_pair(
            "PRIVMSG",
            CommandDefinition(&CommandDispatcher::handlePrivateMessage, 2, true))
    );
}

void CommandDispatcher::execute(Client &client, const IrcMessage &message)
{
    CommandMap::iterator it;

    it = cmmds.find(message.getCommand());
    if (it == cmmds.end())
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_UNKNOWNCOMMAND,
            message.getCommand(),
            NumericReply::MSG_UNKNOWNCOMMAND
        );
        return;
    }

    const CommandDefinition &definition = it->second;

    if (definition.requiresRegistration && !client.isRegistered())
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NOTREGISTERED,
            NumericReply::MSG_NOTREGISTERED
        );
        return;
    }

    if (message.getCommand() == "NICK" && message.params.empty())
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NONICKNAMEGIVEN,
            NumericReply::MSG_NONICKNAMEGIVEN
        );
        return;
    }

    if (message.getCommand() == "PING" && message.params.empty())
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NOORIGIN,
            NumericReply::MSG_NOORIGIN
        );
        return;
    }

    if (message.getCommand() == "PRIVMSG")
    {
        if (message.params.empty())
        {
            server.queueNumericReply(
                client,
                NumericReply::ERR_NORECIPIENT,
                NumericReply::noRecipientMessage(message.getCommand())
            );
            return;
        }

        if (message.params.size() == 1)
        {
            server.queueNumericReply(
                client,
                NumericReply::ERR_NOTEXTTOSEND,
                NumericReply::MSG_NOTEXTTOSEND
            );
            return;
        }
    }

    if (message.params.size() < definition.minParams)
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NEEDMOREPARAMS,
            message.getCommand(),
            NumericReply::MSG_NEEDMOREPARAMS
        );
        return;
    }

    (this->*(definition.handler))(client, message);
}

CommandDispatcher::~CommandDispatcher()
{
}

void CommandDispatcher::handlePass(Client &client, const IrcMessage &message)
{
    // do something with client
    client.setPasswordAccepted(true);
    message.params[0]; // Access the password parameter
}

void CommandDispatcher::handleNick(Client &client, const IrcMessage &message)
{
    // do something with client and message
    client.setNickname(message.params[0]);
    client.setNicknameReceived(true);
}

void CommandDispatcher::handleUser(Client &client, const IrcMessage &message)
{
    // do something with client and message
    client.setUsername(message.params[0]);
    client.setRealname(message.params[3]);
    client.setUsernameReceived(true);
}

void CommandDispatcher::handlePing(Client &client, const IrcMessage &message)
{
    (void)client;
    (void)message;
}

void CommandDispatcher::handleJoin(Client &client, const IrcMessage &message)
{
    // do something with client and message
    (void)client;
    std::string channelName = message.params[0];
    (void)channelName;
}

void CommandDispatcher::handlePrivateMessage(Client &client, const IrcMessage &message)
{
    const std::string &target = message.params[0];
    const std::string &msg = message.params[1];

    std::vector<std::string> privateMessageParameters;

    privateMessageParameters.push_back(target);
    privateMessageParameters.push_back(msg);

    const IrcMessage privateMessage(
        "PRIVMSG",
        privateMessageParameters,
        server.getClientPrefix(client).substr(1),
        true
    );

    server.queueMessage(client, privateMessage.serialize());
}
