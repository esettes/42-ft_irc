#include "CommandDispatcher.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "IrcMessage.hpp"

#include <cctype>
#include <sstream>

namespace
{
    bool isSupportedChannelMode(char mode)
    {
        return mode == 'i'
            || mode == 't'
            || mode == 'k'
            || mode == 'o'
            || mode == 'l';
    }

    bool modeRequiresArgument(char mode, bool adding)
    {
        if (mode == 'o')
            return true;

        if (mode == 'k' && adding)
            return true;

        if (mode == 'l' && adding)
            return true;

        return false;
    }

    bool parsePositiveUserLimit(const std::string &text, std::size_t &limit)
    {
        if (text.empty())
            return false;

        const std::size_t maximumValue = static_cast<std::size_t>(-1);
        std::size_t value = 0;

        for (std::size_t index = 0; index < text.size(); ++index)
        {
            const unsigned char character =
                static_cast<unsigned char>(text[index]);

            if (!std::isdigit(character))
                return false;

            const std::size_t digit = static_cast<std::size_t>(character - '0');

            if (value > maximumValue / 10
                || (value == maximumValue / 10 && digit > maximumValue % 10))
            {
                return false;
            }

            value = value * 10 + digit;
        }

        if (value == 0)
            return false;

        limit = value;
        return true;
    }

    std::string formatUserLimit(std::size_t limit)
    {
        std::ostringstream stream;

        stream << limit;
        return stream.str();
    }
}


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
            "NOTICE",
            CommandDefinition(&CommandDispatcher::handleNotice, 0, true))
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
 * @brief Processes an explicit IRC QUIT command.
 *
 * Stores the supplied reason, or a default one when none was provided, and
 * requests deferred disconnection. QUIT notification, state cleanup, socket
 * closure and object destruction are performed once by
 * Server::disconnectClient().
 *
 * @param client The client requesting disconnection.
 * @param message The parsed QUIT command and its optional reason.
 */
void CommandDispatcher::handleQuit(Client &client,const IrcMessage &message)
{
    std::string quitReason = "Client Quit";

    if (!message.params.empty() && !message.params[0].empty())
        quitReason = message.params[0];

    client.requestDisconnect(quitReason);
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
 * creates the channel, adds the client, broadcasts JOIN, and sends the topic,
 * member list and stored channel messages to the joining client.
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
    server.sendChannelHistory(client, *channel);
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

    if (server.isBotClient(*targetClient))
        server.notifyBotInvite(*channel);
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

    const std::string kickedFromChannel = channel->getName();

    server.removeClientFromChannel(*targetClient, *channel);
    server.notifyBotKick(*targetClient, kickedFromChannel);
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

CommandDispatcher::ModeOperation::ModeOperation()
    : action(MODE_ADD),
      mode('\0'),
      argument(),
      numericArgument(0)
{
}

/**
 * @brief Interprets a MODE parameter list into discrete add/remove operations
 * without modifying channel state. Unknown letters and missing arguments are
 * reported so the handler can reject the whole command.
 */
bool CommandDispatcher::parseChannelModeOperations(
    const std::vector<std::string> &params,
    std::vector<ModeOperation> &operations,
    char &unknownMode
) const
{
    const std::string &modeString = params[1];
    ModeAction currentAction = MODE_ADD;
    std::size_t argumentIndex = 2;

    unknownMode = '\0';
    operations.clear();

    for (std::size_t index = 0; index < modeString.size(); ++index)
    {
        const char character = modeString[index];

        if (character == '+')
        {
            currentAction = MODE_ADD;
            continue;
        }

        if (character == '-')
        {
            currentAction = MODE_REMOVE;
            continue;
        }

        if (!isSupportedChannelMode(character))
        {
            unknownMode = character;
            operations.clear();
            return false;
        }

        ModeOperation operation;

        operation.action = currentAction;
        operation.mode = character;

        if (modeRequiresArgument(character, currentAction == MODE_ADD))
        {
            if (argumentIndex >= params.size() || params[argumentIndex].empty())
            {
                operations.clear();
                return false;
            }

            operation.argument = params[argumentIndex];
            ++argumentIndex;
        }

        operations.push_back(operation);
    }

    return true;
}

/**
 * @brief Checks that every parsed MODE operation is semantically valid before
 * any channel state changes. Operator and membership checks for +o/-o, a
 * non-empty key for +k, and a strictly positive limit for +l are applied in
 * command order. The first error is queued and later operations are not
 * examined so a rejected command never applies a prefix of the change list.
 */
bool CommandDispatcher::validateChannelModeOperations(
    Client &client,
    Channel &channel,
    std::vector<ModeOperation> &operations
)
{
    for (std::size_t index = 0; index < operations.size(); ++index)
    {
        ModeOperation &operation = operations[index];

        if (operation.mode == 'o')
        {
            Client *targetClient =
                server.findClientByNickname(operation.argument);

            if (targetClient == NULL)
            {
                server.queueNumericReply(
                    client,
                    NumericReply::ERR_NOSUCHNICK,
                    operation.argument,
                    NumericReply::MSG_NOSUCHNICK
                );
                return false;
            }

            if (!channel.hasMember(targetClient))
            {
                std::vector<std::string> notInChannelParameters;

                notInChannelParameters.push_back(targetClient->getNickname());
                notInChannelParameters.push_back(channel.getName());

                server.queueNumericReply(
                    client,
                    NumericReply::ERR_USERNOTINCHANNEL,
                    notInChannelParameters,
                    NumericReply::MSG_USERNOTINCHANNEL
                );
                return false;
            }

            operation.argument = targetClient->getNickname();
            continue;
        }

        if (operation.mode == 'k' && operation.action == MODE_ADD)
        {
            if (operation.argument.empty())
            {
                server.queueNumericReply(
                    client,
                    NumericReply::ERR_NEEDMOREPARAMS,
                    "MODE",
                    NumericReply::MSG_NEEDMOREPARAMS
                );
                return false;
            }

            continue;
        }

        if (operation.mode == 'l' && operation.action == MODE_ADD)
        {
            if (!parsePositiveUserLimit(
                    operation.argument,
                    operation.numericArgument
                ))
            {
                server.queueNumericReply(
                    client,
                    NumericReply::ERR_NEEDMOREPARAMS,
                    "MODE",
                    NumericReply::MSG_NEEDMOREPARAMS
                );
                return false;
            }

            operation.argument = formatUserLimit(operation.numericArgument);
        }
    }

    return true;
}

/**
 * @brief Applies already validated MODE operations to the channel. Channel
 * accessors keep invite, topic, key, limit and operator state consistent.
 */
void CommandDispatcher::applyChannelModeOperations(
    Channel &channel,
    const std::vector<ModeOperation> &operations
)
{
    for (std::size_t index = 0; index < operations.size(); ++index)
    {
        const ModeOperation &operation = operations[index];
        const bool adding = operation.action == MODE_ADD;

        if (operation.mode == 'i')
        {
            channel.setInviteOnly(adding);
            continue;
        }

        if (operation.mode == 't')
        {
            channel.setTopicRestricted(adding);
            continue;
        }

        if (operation.mode == 'k')
        {
            if (adding)
                channel.setKey(operation.argument);
            else
                channel.removeKey();
            continue;
        }

        if (operation.mode == 'l')
        {
            if (adding)
                channel.setUserLimit(operation.numericArgument);
            else
                channel.removeUserLimit();
            continue;
        }

        if (operation.mode == 'o')
        {
            Client *targetClient =
                server.findClientByNickname(operation.argument);

            if (adding)
                channel.addOperator(targetClient);
            else
                channel.removeOperator(targetClient);
        }
    }
}

/**
 * @brief Replies with RPL_CHANNELMODEIS. Active flags are always listed in
 * itkl order; +o is omitted because it is a per-user privilege. The key and
 * user limit follow the flag string when those modes are set.
 */
void CommandDispatcher::sendChannelModeIs(
    Client &client,
    const Channel &channel
)
{
    std::string modeFlags = "+";
    std::vector<std::string> parameters;

    parameters.push_back(channel.getName());

    if (channel.isInviteOnly())
        modeFlags += 'i';
    if (channel.isTopicRestricted())
        modeFlags += 't';
    if (channel.isKeyEnabled())
        modeFlags += 'k';
    if (channel.isLimitEnabled())
        modeFlags += 'l';

    parameters.push_back(modeFlags);

    if (channel.isKeyEnabled())
        parameters.push_back(channel.getKey());
    if (channel.isLimitEnabled())
        parameters.push_back(formatUserLimit(channel.getUserLimit()));

    server.queueNumericReply(
        client,
        NumericReply::RPL_CHANNELMODEIS,
        parameters
    );
}

/**
 * @brief Broadcasts the applied MODE operations to every channel member,
 * including the sender. The reconstructed mode string preserves sign changes
 * and only includes arguments that were actually consumed.
 */
void CommandDispatcher::notifyChannelModeChanges(
    Client &client,
    Channel &channel,
    const std::vector<ModeOperation> &operations
)
{
    if (operations.empty())
        return;

    std::string modeString;
    char currentSign = '\0';
    std::vector<std::string> modeParameters;

    modeParameters.push_back(channel.getName());

    for (std::size_t index = 0; index < operations.size(); ++index)
    {
        const ModeOperation &operation = operations[index];
        const char sign = operation.action == MODE_ADD ? '+' : '-';

        if (sign != currentSign)
        {
            modeString += sign;
            currentSign = sign;
        }

        modeString += operation.mode;
    }

    modeParameters.push_back(modeString);

    for (std::size_t index = 0; index < operations.size(); ++index)
    {
        const ModeOperation &operation = operations[index];

        if (modeRequiresArgument(operation.mode, operation.action == MODE_ADD))
            modeParameters.push_back(operation.argument);
    }

    const IrcMessage modeMessage(
        "MODE",
        modeParameters,
        server.getClientPrefix(client),
        false
    );

    server.queueMessageToChannel(channel, modeMessage.serialize());
}

/**
 * @brief Processes MODE for a registered client. With only a channel name it
 * reports the current flags through RPL_CHANNELMODEIS without requiring
 * operator privileges. With a mode string it parses every operation, validates
 * membership, operator status, known flags and arguments, then applies the
 * whole command and notifies the channel.
 *
 * Registration and a missing channel parameter are validated by execute()
 * before this handler is called. User-mode targets are ignored.
 */
void CommandDispatcher::handleMode(
    Client &client,
    const IrcMessage &message
)
{
    const std::string &target = message.params[0];

    if (target.empty())
    {
        server.queueNumericReply(
            client,
            NumericReply::ERR_NEEDMOREPARAMS,
            message.getCommand(),
            NumericReply::MSG_NEEDMOREPARAMS
        );
        return;
    }

    if (!isChannelTarget(target))
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

    if (message.params.size() < 2 || message.params[1].empty())
    {
        sendChannelModeIs(client, *channel);
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

    std::vector<ModeOperation> operations;
    char unknownMode = '\0';

    if (!parseChannelModeOperations(message.params, operations, unknownMode))
    {
        if (unknownMode != '\0')
        {
            server.queueNumericReply(
                client,
                NumericReply::ERR_UNKNOWNMODE,
                std::string(1, unknownMode),
                NumericReply::MSG_UNKNOWNMODE
            );
            return;
        }

        server.queueNumericReply(
            client,
            NumericReply::ERR_NEEDMOREPARAMS,
            message.getCommand(),
            NumericReply::MSG_NEEDMOREPARAMS
        );
        return;
    }

    if (!validateChannelModeOperations(client, *channel, operations))
        return;

    applyChannelModeOperations(*channel, operations);
    notifyChannelModeChanges(client, *channel, operations);
}

/**
 * @brief Delivers PRIVMSG to a nickname or channel after registration and
 * parameter checks performed by execute(). Empty message text is rejected
 * with 412; routing then depends on whether the target begins with '#'.
 *
 * CTCP payloads, including DCC SEND/CHAT, travel as a normal PRIVMSG
 * trailing parameter. The handler does not parse or rewrite the text, so
 * SOH-delimited DCC handshakes are forwarded byte-for-byte to the target.
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
        sendMessageToChannel(client, target, messageText, "PRIVMSG", true);
        return;
    }

    sendMessageToUser(client, target, messageText, "PRIVMSG", true);
}

/**
 * @brief Delivers NOTICE with the same nickname/channel routing as PRIVMSG,
 * including CTCP replies such as DCC REJECT. RFC 2812 forbids automatic
 * error replies, so missing targets, empty text and channel access failures
 * are dropped silently.
 */
void CommandDispatcher::handleNotice(
    Client &client,
    const IrcMessage &message
)
{
    if (message.params.size() < 2 || message.params[1].empty())
        return;

    const std::string &target = message.params[0];
    const std::string &messageText = message.params[1];

    if (isChannelTarget(target))
    {
        sendMessageToChannel(client, target, messageText, "NOTICE", false);
        return;
    }

    sendMessageToUser(client, target, messageText, "NOTICE", false);
}

bool CommandDispatcher::isChannelTarget(const std::string &target) const
{
    return !target.empty() && target[0] == '#';
}

/**
 * @brief Delivers a client message to a single nickname when it exists,
 * otherwise replies with ERR_NOSUCHNICK when reportErrors is true. Used by
 * PRIVMSG and NOTICE, including exact CTCP/DCC payloads. Delivery uses the
 * non-blocking output buffer. PRIVMSG to the built-in bot is then handed to
 * the bot handler so the virtual user can reply without a socket.
 */
void CommandDispatcher::sendMessageToUser(
    Client &sender,
    const std::string &nickname,
    const std::string &messageText,
    const std::string &command,
    bool reportErrors
)
{
    Client *recipient = server.findClientByNickname(nickname);

    if (recipient == NULL)
    {
        if (reportErrors)
        {
            server.queueNumericReply(
                sender,
                NumericReply::ERR_NOSUCHNICK,
                nickname,
                NumericReply::MSG_NOSUCHNICK
            );
        }
        return;
    }

    std::vector<std::string> privateMessageParameters;

    privateMessageParameters.push_back(nickname);
    privateMessageParameters.push_back(messageText);

    const IrcMessage privateMessage(
        command,
        privateMessageParameters,
        server.getClientPrefix(sender),
        true
    );

    server.queueMessage(*recipient, privateMessage.serialize());

    if (command == "PRIVMSG" && server.isBotClient(*recipient))
        server.notifyBotPrivateMessage(sender, messageText);
}

/**
 * @brief Delivers a client message to every channel member except the sender
 * when the channel exists and the sender belongs to it. PRIVMSG lines are
 * stored for later JOIN replay; NOTICE is not. Missing channels yield
 * ERR_NOSUCHCHANNEL and non-members yield ERR_CANNOTSENDTOCHAN when
 * reportErrors is true.
 */
void CommandDispatcher::sendMessageToChannel(
    Client &sender,
    const std::string &channelName,
    const std::string &messageText,
    const std::string &command,
    bool reportErrors
)
{
    Channel *channel = server.findChannel(channelName);

    if (channel == NULL)
    {
        if (reportErrors)
        {
            server.queueNumericReply(
                sender,
                NumericReply::ERR_NOSUCHCHANNEL,
                channelName,
                NumericReply::MSG_NOSUCHCHANNEL
            );
        }
        return;
    }

    if (!channel->hasMember(&sender))
    {
        if (reportErrors)
        {
            server.queueNumericReply(
                sender,
                NumericReply::ERR_CANNOTSENDTOCHAN,
                channelName,
                NumericReply::MSG_CANNOTSENDTOCHAN
            );
        }
        return;
    }

    std::vector<std::string> privateMessageParameters;

    privateMessageParameters.push_back(channelName);
    privateMessageParameters.push_back(messageText);

    const IrcMessage privateMessage(
        command,
        privateMessageParameters,
        server.getClientPrefix(sender),
        true
    );

    const std::string serializedMessage = privateMessage.serialize();

    if (command == "PRIVMSG")
        channel->addHistoryMessage(serializedMessage);

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

    if (command == "PRIVMSG")
        server.notifyBotChannelMessage(sender, *channel, messageText);
}
