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

/**
 * @brief Registers every supported IRC command with its handler, minimum
 * parameter count, and client registration requirement.
 */
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
            "PONG",
            CommandDefinition(&CommandDispatcher::handlePong, 0, false))
    );
    cmmds.insert(
        std::make_pair(
            "QUIT",
            CommandDefinition(&CommandDispatcher::handleQuit, 0, false))
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

/**
 * @brief Dispatches a parsed IRC command after verifying that it is
 * supported, that the client satisfies its registration requirement, and
 * that its required parameters are present. Queues the corresponding numeric
 * error when validation fails and invokes the registered handler otherwise.
 */
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

    if ((message.getCommand() == "PING"
            || message.getCommand() == "PONG")
        && message.params.empty())
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
    if (client.isRegistered())
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_ALREADYREGISTERED,
            NumericReply::MSG_ALREADYREGISTRED
        );
        return;
    }

    const std::string &providedPassword = message.params[0];

    if (!server.isPasswordCorrect(providedPassword))
    {
        client.setPasswordAccepted(false);
        server.queueNumericReply(
            client,
            NumericReply::ERR_PASSWDMISMATCH,
            NumericReply::MSG_PASSWDMISMATCH
        );
        return;
    }

    client.setPasswordAccepted(true);
}

/**
 * @brief Validates and assigns a requested nickname, rejects missing, invalid
 * or occupied names, and broadcasts registered nickname changes using the
 * client's previous prefix; initial and identical assignments are not announced.
 */
void CommandDispatcher::handleNick(Client &client, const IrcMessage &message)
{
    const std::string &requestedNickname = message.params[0];

    if (requestedNickname.empty())
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NONICKNAMEGIVEN,
            NumericReply::MSG_NONICKNAMEGIVEN
        );
        return;
    }

    if (!server.isValidNickname(requestedNickname))
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_ERRONEUSNICKNAME,
            requestedNickname,
            NumericReply::MSG_ERRONEUSNICKNAME
        );
        return;
    }

    const bool clientWasRegistered = client.isRegistered();
    const std::string previousNickname = client.getNickname();
    const std::string previousClientPrefix =
        server.getClientPrefix(client);

    if (!server.assignNickname(client, requestedNickname))
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NICKNAMEINUSE,
            requestedNickname,
            NumericReply::MSG_NICKNAMEINUSE
        );
        return;
    }

    if (!clientWasRegistered || previousNickname == requestedNickname)
        return;

    std::vector<std::string> nicknameParameters;
    nicknameParameters.push_back(requestedNickname);

    const IrcMessage nicknameMessage(
        "NICK",
        nicknameParameters,
        previousClientPrefix,
        true
    );

    server.queueMessageToRelatedClients(
        client,
        nicknameMessage.serialize()
    );
}

/**
 * @brief Processes USER during registration, rejecting registered clients or
 * missing required data, then stores the username and real name and marks the
 * USER registration step as completed.
 */
void CommandDispatcher::handleUser(Client &client, const IrcMessage &message)
{
    if (client.isRegistered())
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_ALREADYREGISTERED,
            NumericReply::MSG_ALREADYREGISTRED
        );
        return;
    }

    const std::string &requestedUsername = message.params[0];
    const std::string &requestedRealname = message.params[3];

    if (requestedUsername.empty() || requestedRealname.empty())
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NEEDMOREPARAMS,
            message.getCommand(),
            NumericReply::MSG_NEEDMOREPARAMS
        );
        return;
    }

    client.setUsername(requestedUsername);
    client.setRealname(requestedRealname);
    client.setUsernameReceived(true);
}

/**
 * @brief Responds to a valid PING command with a PONG message containing
 * the same token, queued through the server's non-blocking output system.
 */
void CommandDispatcher::handlePing(Client &client, const IrcMessage &message)
{
    std::vector<std::string> pongParameters;

    pongParameters.push_back(message.params[0]);

    const IrcMessage pongMessage(
        "PONG",
        pongParameters,
        "",
        true
    );

    server.queueMessage(client, pongMessage.serialize());
}

/**
 * @brief Accepts a valid PONG response from a client. No state is updated
 * because the server does not currently track keepalive tokens or deadlines.
 */
void CommandDispatcher::handlePong(Client &client, const IrcMessage &message)
{
    (void)client;
    (void)message;
}

/**
 * @brief Processes a intentioned client disconnection, using the supplied
 * reason or a default one, notifying related clients and requesting the
 * source client's removal from the server.
 */
void CommandDispatcher::handleQuit(
    Client &client,
    const IrcMessage &message
)
{
    std::string quitReason = "Client Quit";

    if (!message.params.empty() && !message.params[0].empty())
        quitReason = message.params[0];

    std::vector<std::string> quitParameters;

    quitParameters.push_back(quitReason);

    const IrcMessage quitMessage(
        "QUIT",
        quitParameters,
        server.getClientPrefix(client),
        true
    );

    const std::string serializedQuitMessage =
        quitMessage.serialize();

    client.requestDisconnect();

    server.queueMessageToRelatedClients(
        client,
        serializedQuitMessage
    );
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
        server.getClientPrefix(client),
        true
    );

    server.queueMessage(client, privateMessage.serialize());
}
