#include "CommandDispatcher.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "IrcMessage.hpp"

#include <cctype>


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
            "CAP",
            CommandDefinition(&CommandDispatcher::handleCap, 1, false))
    );
    cmmds.insert(
        std::make_pair(
            "JOIN",
            CommandDefinition(&CommandDispatcher::handleJoin, 1, true))
    );
    cmmds.insert(
        std::make_pair(
            "PART",
            CommandDefinition(&CommandDispatcher::handlePart, 1, true))
    );
    cmmds.insert(
        std::make_pair(
            "PRIVMSG",
            CommandDefinition(&CommandDispatcher::handlePrivateMessage, 2, true))
    );
    cmmds.insert(
        std::make_pair(
            "TOPIC",
            CommandDefinition(&CommandDispatcher::handleTopic, 1, true))
    );
    cmmds.insert(
        std::make_pair(
            "INVITE",
            CommandDefinition(&CommandDispatcher::handleInvite, 2, true))
    );
    cmmds.insert(
        std::make_pair(
            "KICK",
            CommandDefinition(&CommandDispatcher::handleKick, 2, true))
    );
    cmmds.insert(
        std::make_pair(
            "MODE",
            CommandDefinition(&CommandDispatcher::handleMode, 1, true))
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

        if (message.params.size() == 1
            || message.params[1].empty())
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

/**
 * @brief Processes CAP LS, CAP LIST, CAP REQ and CAP ENDbefore or after client
 * registration, returning empty capability lists for LS and LIST and rejecting
 * requested capabilities through NAK.
 */
void CommandDispatcher::handleCap(Client &client, const IrcMessage &message)
{
    std::string capabilitySubcommand = message.params[0];

    for (std::size_t characterIndex = 0;
        characterIndex < capabilitySubcommand.size();
        ++characterIndex)
    {
        capabilitySubcommand[characterIndex] = static_cast<char>(
            std::toupper(
                static_cast<unsigned char>(
                    capabilitySubcommand[characterIndex]
                )
            )
        );
    }
    if (capabilitySubcommand == "END")
        return;
    
    std::string responseSubcommand;
    std::string responseCapabilityList;

    if (capabilitySubcommand == "LS" || capabilitySubcommand == "LIST")
    {
        responseSubcommand = capabilitySubcommand;
    }
    else if (capabilitySubcommand == "REQ")
    {
        if (message.params.size() < 2 || message.params[1].empty())
        {
            server.queueNumericReply(
                client,
                NumericReply::ERR_NEEDMOREPARAMS,
                message.getCommand(),
                NumericReply::MSG_NEEDMOREPARAMS
            );
            return;
        }

        responseSubcommand = "NAK";
        responseCapabilityList = message.params[1];
    }
    else
    {
        return;
    }

    std::string clientIdentifier = client.getNickname();

    if (clientIdentifier.empty())
        clientIdentifier = "*";

    std::vector<std::string> capabilityParameters;

    capabilityParameters.push_back(clientIdentifier);
    capabilityParameters.push_back(responseSubcommand);
    capabilityParameters.push_back(responseCapabilityList);

    const IrcMessage capabilityMessage(
        "CAP",
        capabilityParameters,
        server.getServerName(),
        true
    );

    server.queueMessage(
        client,
        capabilityMessage.serialize()
    );
}

/**
 * @brief Splits a comma-separated IRC parameter into individual values while
 * preserving empty entries so positional relationships, such as channels and
 * their corresponding keys, remain intact.
 *
 * @param valueList The comma-separated parameter to split.
 * @return A vector containing each value in its original order.
 */
std::vector<std::string>
CommandDispatcher::splitCommaSeparatedValues(const std::string &valueList)
{
    std::vector<std::string> values;
    std::string::size_type valueStart = 0;

    while (true)
    {
        const std::string::size_type commaPosition =
            valueList.find(',', valueStart);

        if (commaPosition == std::string::npos)
        {
            values.push_back(valueList.substr(valueStart));
            break;
        }

        values.push_back(
            valueList.substr(
                valueStart,
                commaPosition - valueStart
            )
        );

        valueStart = commaPosition + 1;
    }

    return values;
}

/**
 * @brief Processes a JOIN attempt for one channel. It validates the channel
 * name and access restrictions, ignores duplicate membership, obtains or
 * creates the channel, adds the client, broadcasts JOIN, and sends the topic
 * and member list to the joining client.
 *
 * @param client The registered client requesting to join.
 * @param channelName The individual channel name to process.
 * @param providedKey The corresponding channel key, or an empty string when
 * no key was supplied for this channel.
 */
void CommandDispatcher::joinClientToSingleChannel(
    Client &client,
    const std::string &channelName,
    const std::string &providedKey
)
{
    Channel *channel = server.findOrCreateChannel(channelName);

    if (channel == NULL)
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NOSUCHCHANNEL,
            channelName,
            NumericReply::MSG_NOSUCHCHANNEL
        );
        return;
    }

    if (channel->hasMember(&client))
        return;

    if (!server.validateChannelJoinAccess(
            client,
            *channel,
            providedKey
        ))
    {
        return;
    }

    server.addClientToChannel(client, *channel);

    std::vector<std::string> joinParameters;

    joinParameters.push_back(channel->getName());

    const IrcMessage joinMessage(
        "JOIN",
        joinParameters,
        server.getClientPrefix(client),
        true
    );

    server.queueMessageToChannel(
        *channel,
        joinMessage.serialize()
    );

    server.sendChannelTopic(client, *channel);
    server.sendChannelNames(client, *channel);
}

/**
 * @brief Processes JOIN for a registered client. It separates the requested
 * channel and key lists, associates keys with channels by position, and
 * processes every channel independently.
 *
 * An empty channel parameter produces ERR_NEEDMOREPARAMS. Failure to join one
 * channel does not prevent later channels in the same command from being
 * processed.
 *
 * Registration and missing parameter counts are validated by execute() before
 * this handler is called.
 *
 * @param client The registered client requesting to join.
 * @param message The parsed JOIN message containing a comma-separated channel
 * list and an optional comma-separated key list.
 */
void CommandDispatcher::handleJoin(Client &client, const IrcMessage &message)
{
   const std::string &channelList = message.params[0];

    if (channelList.empty())
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NEEDMOREPARAMS,
            message.getCommand(),
            NumericReply::MSG_NEEDMOREPARAMS
        );
        return;
    }

    const std::vector<std::string> requestedChannels =
        splitCommaSeparatedValues(channelList);

    std::vector<std::string> providedKeys;

    if (message.params.size() > 1)
    {
        providedKeys =
            splitCommaSeparatedValues(message.params[1]);
    }

    for (std::size_t channelIndex = 0;
        channelIndex < requestedChannels.size();
        ++channelIndex)
    {
        std::string providedKey;

        if (channelIndex < providedKeys.size())
            providedKey = providedKeys[channelIndex];

        joinClientToSingleChannel(
            client,
            requestedChannels[channelIndex],
            providedKey
        );
    }
}

/**
 * @brief Processes a PART attempt for one channel. It validates that the
 * channel exists and that the client belongs to it, broadcasts the PART
 * message before changing membership, and then removes the client while
 * allowing the server to delete an empty channel.
 *
 * @param client The client requesting to leave the channel.
 * @param channelName The individual channel name to leave.
 * @param partReason The optional reason included in the PART message.
 * @param hasPartReason Whether the original command contained a reason,
 * including an explicitly empty trailing reason.
 */
void CommandDispatcher::partClientFromSingleChannel(
    Client &client,
    const std::string &channelName,
    const std::string &partReason,
    bool hasPartReason)
{
    Channel *channel = server.findChannel(channelName);

    if (channel == NULL)
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NOSUCHCHANNEL,
            channelName,
            NumericReply::MSG_NOSUCHCHANNEL
        );
        return;
    }

    if (!channel->hasMember(&client))
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NOTONCHANNEL,
            channel->getName(),
            NumericReply::MSG_NOTONCHANNEL
        );
        return;
    }

    std::vector<std::string> partParameters;

    partParameters.push_back(channel->getName());

    if (hasPartReason)
        partParameters.push_back(partReason);

    const IrcMessage partMessage(
        "PART",
        partParameters,
        server.getClientPrefix(client),
        hasPartReason
    );

    server.queueMessageToChannel(
        *channel,
        partMessage.serialize()
    );

    server.removeClientFromChannel(client, *channel);
}

/**
 * @brief Processes PART for a registered client. It separates the requested
 * channel list, preserves the optional shared reason, and processes each
 * channel independently.
 *
 * An empty channel parameter produces ERR_NEEDMOREPARAMS. Failure to leave one
 * channel does not stop the remaining channels from being processed.
 *
 * @param client The registered client requesting to leave channels.
 * @param message The parsed PART message containing a comma-separated channel
 * list and an optional reason.
 */
void CommandDispatcher::handlePart(Client &client, const IrcMessage &message)
{
    const std::string &channelList = message.params[0];

    if (channelList.empty())
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NEEDMOREPARAMS,
            message.getCommand(),
            NumericReply::MSG_NEEDMOREPARAMS
        );
        return;
    }

    const std::vector<std::string> requestedChannels =
        splitCommaSeparatedValues(channelList);

    const bool hasPartReason = message.params.size() > 1;

    std::string partReason;

    if (hasPartReason)
        partReason = message.params[1];

    for (std::size_t channelIndex = 0;
        channelIndex < requestedChannels.size();
        ++channelIndex)
    {
        partClientFromSingleChannel(
            client,
            requestedChannels[channelIndex],
            partReason,
            hasPartReason
        );
    }
}

/**
 * @brief Validates invite-only, channel-key and user-limit restrictions for
 * JOIN. A pending invitation only bypasses +i; +k and +l still apply.
 * Failed checks do not consume invitations.
 */
bool CommandDispatcher::canJoinChannel(
    Client &client,
    Channel &channel,
    const std::string &providedKey
)
{
    if (channel.isInviteOnly() && !channel.hasInvitation(&client))
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_INVITEONLYCHAN,
            channel.getName(),
            NumericReply::MSG_INVITEONLYCHAN
        );
        return false;
    }

    if (channel.isKeyEnabled()
        && (providedKey.empty() || providedKey != channel.getKey()))
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_BADCHANNELKEY,
            channel.getName(),
            NumericReply::MSG_BADCHANNELKEY
        );
        return false;
    }

    if (channel.isLimitEnabled()
        && channel.getMemberCount() >= channel.getUserLimit())
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_CHANNELISFULL,
            channel.getName(),
            NumericReply::MSG_CHANNELISFULL
        );
        return false;
    }

    return true;
}

/**
 * @brief Processes INVITE for a registered channel operator. Validates the
 * target nickname and channel, stores a pending invitation, confirms with
 * RPL_INVITING and notifies only the invited client.
 */
void CommandDispatcher::handleInvite(
    Client &client,
    const IrcMessage &message
)
{
    const std::string &targetNickname = message.params[0];
    const std::string &channelName = message.params[1];

    if (targetNickname.empty() || channelName.empty())
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NEEDMOREPARAMS,
            message.getCommand(),
            NumericReply::MSG_NEEDMOREPARAMS
        );
        return;
    }

    Channel *channel = server.findChannel(channelName);

    if (channel == NULL)
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NOSUCHCHANNEL,
            channelName,
            NumericReply::MSG_NOSUCHCHANNEL
        );
        return;
    }

    Client *targetClient = server.findClientByNickname(targetNickname);

    if (targetClient == NULL)
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NOSUCHNICK,
            targetNickname,
            NumericReply::MSG_NOSUCHNICK
        );
        return;
    }

    if (!channel->hasMember(&client))
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NOTONCHANNEL,
            channel->getName(),
            NumericReply::MSG_NOTONCHANNEL
        );
        return;
    }

    if (channel->hasMember(targetClient))
    {
        std::vector<std::string> alreadyOnChannelParameters;

        alreadyOnChannelParameters.push_back(targetClient->getNickname());
        alreadyOnChannelParameters.push_back(channel->getName());

        server.queueNumericReply(
            client,
            NumericReply::ERR_USERONCHANNEL,
            alreadyOnChannelParameters,
            NumericReply::MSG_USERONCHANNEL
        );
        return;
    }

    if (!channel->hasOperator(&client))
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_CHANOPRIVSNEEDED,
            channel->getName(),
            NumericReply::MSG_CHANOPRIVSNEEDED
        );
        return;
    }

    channel->inviteClient(targetClient);

    std::vector<std::string> invitingParameters;

    invitingParameters.push_back(targetClient->getNickname());
    invitingParameters.push_back(channel->getName());

    server.queueNumericReply(
        client,
        NumericReply::RPL_INVITING,
        invitingParameters
    );

    std::vector<std::string> inviteParameters;

    inviteParameters.push_back(targetClient->getNickname());
    inviteParameters.push_back(channel->getName());

    const IrcMessage inviteMessage(
        "INVITE",
        inviteParameters,
        server.getClientPrefix(client),
        true
    );

    server.queueMessage(*targetClient, inviteMessage.serialize());
}

/**
 * @brief Processes KICK for a registered channel operator. It validates the
 * channel, the target nickname, membership and operator privileges before
 * changing any state, broadcasts the KICK to every current member including
 * the target, then removes the target from the channel without disconnecting
 * them. An omitted or empty reason defaults to the operator's nickname.
 */
void CommandDispatcher::handleKick(
    Client &client,
    const IrcMessage &message
)
{
    const std::string &channelName = message.params[0];
    const std::string &targetNickname = message.params[1];

    if (channelName.empty() || targetNickname.empty())
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NEEDMOREPARAMS,
            message.getCommand(),
            NumericReply::MSG_NEEDMOREPARAMS
        );
        return;
    }

    Channel *channel = server.findChannel(channelName);

    if (channel == NULL)
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NOSUCHCHANNEL,
            channelName,
            NumericReply::MSG_NOSUCHCHANNEL
        );
        return;
    }

    Client *targetClient = server.findClientByNickname(targetNickname);

    if (targetClient == NULL)
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NOSUCHNICK,
            targetNickname,
            NumericReply::MSG_NOSUCHNICK
        );
        return;
    }

    if (!channel->hasMember(&client))
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NOTONCHANNEL,
            channel->getName(),
            NumericReply::MSG_NOTONCHANNEL
        );
        return;
    }

    if (!channel->hasOperator(&client))
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_CHANOPRIVSNEEDED,
            channel->getName(),
            NumericReply::MSG_CHANOPRIVSNEEDED
        );
        return;
    }

    if (!channel->hasMember(targetClient))
    {
        std::vector<std::string> notInChannelParameters;

        notInChannelParameters.push_back(targetClient->getNickname());
        notInChannelParameters.push_back(channel->getName());

        server.queueNumericReply(
            client,
            NumericReply::ERR_USERNOTINCHANNEL,
            notInChannelParameters,
            NumericReply::MSG_USERNOTINCHANNEL
        );
        return;
    }

    std::string kickReason = client.getNickname();

    if (message.params.size() >= 3 && !message.params[2].empty())
        kickReason = message.params[2];

    std::vector<std::string> kickParameters;

    kickParameters.push_back(channel->getName());
    kickParameters.push_back(targetClient->getNickname());
    kickParameters.push_back(kickReason);

    const IrcMessage kickMessage(
        "KICK",
        kickParameters,
        server.getClientPrefix(client),
        true
    );

    server.queueMessageToChannel(
        *channel,
        kickMessage.serialize()
    );

    server.removeClientFromChannel(*targetClient, *channel);
}

/**
 * @brief Processes TOPIC for a registered client. With only a channel name
 * it reports the current topic through RPL_TOPIC or RPL_NOTOPIC. With a
 * topic parameter, including an empty trailing parameter, it updates the
 * stored topic when the client is allowed to do so and broadcasts the
 * change to every channel member, including the sender.
 *
 * Registration and a missing channel name are validated by execute() before
 * this handler is called.
 */
void CommandDispatcher::handleTopic(
    Client &client,
    const IrcMessage &message
)
{
    const std::string &channelName = message.params[0];

    if (channelName.empty())
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NEEDMOREPARAMS,
            message.getCommand(),
            NumericReply::MSG_NEEDMOREPARAMS
        );
        return;
    }

    Channel *channel = server.findChannel(channelName);

    if (channel == NULL)
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NOSUCHCHANNEL,
            channelName,
            NumericReply::MSG_NOSUCHCHANNEL
        );
        return;
    }

    if (!channel->hasMember(&client))
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NOTONCHANNEL,
            channel->getName(),
            NumericReply::MSG_NOTONCHANNEL
        );
        return;
    }

    const bool topicProvided =
        message.hasTrailingParameter || message.params.size() >= 2;

    if (!topicProvided)
    {
        sendTopicReply(client, *channel);
        return;
    }

    if (channel->isTopicRestricted() && !channel->hasOperator(&client))
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_CHANOPRIVSNEEDED,
            channel->getName(),
            NumericReply::MSG_CHANOPRIVSNEEDED
        );
        return;
    }

    const std::string newTopic =
        message.params.size() >= 2 ? message.params[1] : "";

    applyTopicChange(client, *channel, newTopic);
}

/**
 * @brief Replies with the current channel topic. An empty stored topic
 * produces RPL_NOTOPIC; otherwise the stored text is sent as RPL_TOPIC.
 */
void CommandDispatcher::sendTopicReply(
    Client &client,
    const Channel &channel
)
{
    if (channel.getTopic().empty())
    {
        server.queueNumericReply(
            client,
            NumericReply::RPL_NOTOPIC,
            channel.getName(),
            NumericReply::MSG_NOTOPIC
        );
        return;
    }

    server.queueNumericReply(
        client,
        NumericReply::RPL_TOPIC,
        channel.getName(),
        channel.getTopic()
    );
}

/**
 * @brief Stores a new channel topic and notifies every member, including
 * the client that requested the change. An empty topic clears the current
 * one and is still broadcast with an explicit trailing parameter.
 */
void CommandDispatcher::applyTopicChange(
    Client &client,
    Channel &channel,
    const std::string &newTopic
)
{
    channel.setTopic(newTopic);

    std::vector<std::string> topicParameters;

    topicParameters.push_back(channel.getName());
    topicParameters.push_back(newTopic);

    const IrcMessage topicMessage(
        "TOPIC",
        topicParameters,
        server.getClientPrefix(client),
        true
    );

    server.queueMessageToChannel(channel, topicMessage.serialize());
}

/**
 * @brief Applies channel mode changes required by TOPIC and INVITE. Full MODE
 * handling belongs to a later phase; this handler understands +t/-t, +i/-i
 * and +k/-k so those restrictions can be enabled and tested. Mode queries and
 * other flags are ignored until the complete MODE command is implemented.
 */
void CommandDispatcher::handleMode(
    Client &client,
    const IrcMessage &message
)
{
    const std::string &target = message.params[0];

    if (target.empty() || !isChannelTarget(target))
        return;

    Channel *channel = server.findChannel(target);

    if (channel == NULL)
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NOSUCHCHANNEL,
            target,
            NumericReply::MSG_NOSUCHCHANNEL
        );
        return;
    }

    if (!channel->hasMember(&client))
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NOTONCHANNEL,
            channel->getName(),
            NumericReply::MSG_NOTONCHANNEL
        );
        return;
    }

    if (message.params.size() < 2)
        return;

    if (!channel->hasOperator(&client))
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_CHANOPRIVSNEEDED,
            channel->getName(),
            NumericReply::MSG_CHANOPRIVSNEEDED
        );
        return;
    }

    const std::string &modeString = message.params[1];
    std::vector<std::string> modeParameters;

    modeParameters.push_back(channel->getName());
    modeParameters.push_back(modeString);

    if (modeString == "+t")
    {
        channel->setTopicRestricted(true);
    }
    else if (modeString == "-t")
    {
        channel->setTopicRestricted(false);
    }
    else if (modeString == "+i")
    {
        channel->setInviteOnly(true);
    }
    else if (modeString == "-i")
    {
        channel->setInviteOnly(false);
    }
    else if (modeString == "+k")
    {
        if (message.params.size() < 3 || message.params[2].empty())
        {
            server.queueNumericReply(
                client,
                NumericReply::ERR_NEEDMOREPARAMS,
                message.getCommand(),
                NumericReply::MSG_NEEDMOREPARAMS
            );
            return;
        }

        channel->setKey(message.params[2]);
        modeParameters.push_back(message.params[2]);
    }
    else if (modeString == "-k")
    {
        channel->removeKey();
    }
    else
    {
        return;
    }

    const IrcMessage modeMessage(
        "MODE",
        modeParameters,
        server.getClientPrefix(client),
        false
    );

    server.queueMessageToChannel(*channel, modeMessage.serialize());
}

/**
 * @brief Delivers PRIVMSG to a nickname or channel after registration and
 * parameter checks performed by execute(). Empty message text is rejected
 * with 412; routing then depends on whether the target begins with '#'.
 */
void CommandDispatcher::handlePrivateMessage(
    Client &client,
    const IrcMessage &message
)
{
    const std::string &target = message.params[0];
    const std::string &messageText = message.params[1];

    if (messageText.empty())
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NOTEXTTOSEND,
            NumericReply::MSG_NOTEXTTOSEND
        );
        return;
    }

    if (isChannelTarget(target))
    {
        sendMessageToChannel(client, target, messageText);
        return;
    }

    sendMessageToUser(client, target, messageText);
}

bool CommandDispatcher::isChannelTarget(const std::string &target) const
{
    return !target.empty() && target[0] == '#';
}

/**
 * @brief Delivers a private message to a single nickname when it exists,
 * otherwise replies with ERR_NOSUCHNICK. The sender's full prefix is preserved
 * and delivery uses the non-blocking output buffer.
 */
void CommandDispatcher::sendMessageToUser(
    Client &sender,
    const std::string &nickname,
    const std::string &messageText
)
{
    Client *recipient = server.findClientByNickname(nickname);

    if (recipient == NULL)
    {
        server.queueNumericReply(
            sender,
            NumericReply::ERR_NOSUCHNICK,
            nickname,
            NumericReply::MSG_NOSUCHNICK
        );
        return;
    }

    std::vector<std::string> privateMessageParameters;

    privateMessageParameters.push_back(nickname);
    privateMessageParameters.push_back(messageText);

    const IrcMessage privateMessage(
        "PRIVMSG",
        privateMessageParameters,
        server.getClientPrefix(sender),
        true
    );

    server.queueMessage(*recipient, privateMessage.serialize());
}

/**
 * @brief Delivers a channel message to every member except the sender when
 * the channel exists and the sender belongs to it. Missing channels yield
 * ERR_NOSUCHCHANNEL; non-members yield ERR_CANNOTSENDTOCHAN.
 */
void CommandDispatcher::sendMessageToChannel(
    Client &sender,
    const std::string &channelName,
    const std::string &messageText
)
{
    Channel *channel = server.findChannel(channelName);

    if (channel == NULL)
    {
        server.queueNumericReply(
            sender,
            NumericReply::ERR_NOSUCHCHANNEL,
            channelName,
            NumericReply::MSG_NOSUCHCHANNEL
        );
        return;
    }

    if (!channel->hasMember(&sender))
    {
        server.queueNumericReply(
            sender,
            NumericReply::ERR_CANNOTSENDTOCHAN,
            channelName,
            NumericReply::MSG_CANNOTSENDTOCHAN
        );
        return;
    }

    std::vector<std::string> privateMessageParameters;

    privateMessageParameters.push_back(channelName);
    privateMessageParameters.push_back(messageText);

    const IrcMessage privateMessage(
        "PRIVMSG",
        privateMessageParameters,
        server.getClientPrefix(sender),
        true
    );

    const std::string serializedMessage = privateMessage.serialize();
    const std::set<Client *> &channelMembers = channel->getMembers();

    std::set<Client *>::const_iterator memberIterator =
        channelMembers.begin();

    while (memberIterator != channelMembers.end())
    {
        Client *channelMember = *memberIterator;

        if (channelMember != NULL && channelMember != &sender)
            server.queueMessage(*channelMember, serializedMessage);

        ++memberIterator;
    }
}
