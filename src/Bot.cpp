// Copyright 2026 @esettes, @danielfdez17
#include <stdexcept>
#include <cstdlib>
#include <sstream>
#include <vector>
#include <set>
#include <iostream>

#include "Irc.hpp"
#include "Bot.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "IrcMessage.hpp"
#include "IrcCasemap.hpp"
#include "Console.hpp"

const char *Bot::NICKNAME = "marvin";
const char *Bot::USERNAME = "bot";
const char *Bot::REALNAME = "ft_irc helper bot";
const char *Bot::HOST = "bot.local";
const char *Bot::HOME_CHANNEL = "#bot";
const char *Bot::HOME_TOPIC = "Ask me with !help or /msg marvin help";

namespace {
const int VIRTUAL_CLIENT_FD = Constants::INVALID_FD;
const char CTCP_MARKER = '\x01';

std::string wrapCtcp(const std::string &payload) {
    return std::string(1, CTCP_MARKER) + payload + std::string(1, CTCP_MARKER);
}
}  // namespace

Bot::Bot(Server &server)
    : server(server),
      user(NULL),
      startedAt(std::time(NULL)) {
}

Bot::~Bot() {
    stop();
}

/**
 * @brief Registers the virtual user and joins the home channel. The nickname
 * is reserved before any TCP client can connect, so it cannot be stolen.
 */
void Bot::start() {
    if (user != NULL)
        return;

    if (startedAt == static_cast<std::time_t>(-1))
        startedAt = 0;

    std::srand(static_cast<unsigned int>(
        startedAt == 0 ? 1 : startedAt));

    user = new Client(VIRTUAL_CLIENT_FD, HOST);
    registerIdentity();
    joinHomeChannel();

    std::cout << Console::SERVER
        << " Bot registered as " << NICKNAME
        << " on " << HOME_CHANNEL << std::endl;
}

/**
 * @brief Leaves every channel, frees the nickname and destroys the virtual
 * client. Safe to call more than once.
 */
void Bot::stop() {
    if (user == NULL)
        return;

    const std::set<std::string> joinedChannels = user->getJoinedChannels();
    std::set<std::string>::const_iterator channelIterator =
        joinedChannels.begin();

    while (channelIterator != joinedChannels.end()) {
        Channel *channel = server.findChannel(*channelIterator);

        if (channel != NULL)
            server.removeClientFromChannel(*user, *channel);

        ++channelIterator;
    }

    server.releaseNickname(*user);
    delete user;
    user = NULL;
}

bool Bot::owns(const Client &client) const {
    return user != NULL && user == &client;
}

Client *Bot::getClient() const {
    return user;
}

void Bot::registerIdentity() {
    user->setUsername(USERNAME);
    user->setRealname(REALNAME);
    user->setPasswordAccepted(true);
    user->setUsernameReceived(true);
    user->setRegistered(true);

    if (!server.assignNickname(*user, NICKNAME)) {
        delete user;
        user = NULL;
        throw std::runtime_error("failed to reserve the bot nickname");
    }
}

void Bot::joinHomeChannel() {
    Channel *channel = server.findOrCreateChannel(HOME_CHANNEL);

    if (channel == NULL)
        return;

    channel->setTopic(HOME_TOPIC);
    joinChannel(*channel, false);
}

void Bot::joinChannel(Channel &channel, bool greet) {
    if (user == NULL || channel.hasMember(user))
        return;

    server.addClientToChannel(*user, channel);
    broadcastJoin(channel);

    if (IrcCasemap::equal(channel.getName(), HOME_CHANNEL)
        && !channel.hasOperator(user)) {
        channel.addOperator(user);
    }

    if (greet) {
        sendChannelReply(
            channel,
            Constants::BOT_HOME_GREETING);
    }
}

void Bot::broadcastJoin(Channel &channel) {
    std::vector<std::string> joinParameters;

    joinParameters.push_back(channel.getName());

    const IrcMessage joinMessage(
        Constants::JOIN_CMD,
        joinParameters,
        server.getClientPrefix(*user),
        true);

    server.queueMessageToChannel(channel, joinMessage.serialize());
}

void Bot::sendPrivateReply(Client &recipient, const std::string &text) {
    sendText(&recipient, NULL, Constants::PRIVMSG_CMD, text);
}

void Bot::sendChannelReply(Channel &channel, const std::string &text) {
    sendText(NULL, &channel, Constants::PRIVMSG_CMD, text);
}

void Bot::sendNotice(Client &recipient, const std::string &text) {
    sendText(&recipient, NULL, Constants::NOTICE_CMD, text);
}

/**
 * @brief Sends one IRC line from the bot. Virtual clients have no output
 * buffer, so the destination is always a real user or a channel of real
 * members. Oversized trailing text is clipped to stay within 512 bytes.
 */
void Bot::sendText(
    Client *recipient,
    Channel *channel,
    const std::string &command,
    const std::string &text
) {
    if (user == NULL || text.empty())
        return;

    std::string target;
    if (channel != NULL)
        target = channel->getName();
    else if (recipient != NULL)
        target = recipient->getNickname();
    else
        return;

    const std::string clippedText = clipTrailing(command, target, text);

    if (clippedText.empty())
        return;

    std::vector<std::string> parameters;

    parameters.push_back(target);
    parameters.push_back(clippedText);

    const IrcMessage message(
        command,
        parameters,
        server.getClientPrefix(*user),
        true);

    const std::string serialized = message.serialize();

    if (channel != NULL)
        server.queueMessageToChannel(*channel, serialized);
    else
        server.queueMessage(*recipient, serialized);
}

std::string Bot::trim(const std::string &text) const {
    std::string::size_type start = 0;

    while (start < text.size() && text[start] == ' ')
        ++start;

    std::string::size_type end = text.size();

    while (end > start && text[end - 1] == ' ')
        --end;

    return text.substr(start, end - start);
}

std::string Bot::toLowerAscii(const std::string &text) const {
    std::string lowered = text;

    for (std::size_t index = 0; index < lowered.size(); ++index) {
        const unsigned char character =
            static_cast<unsigned char>(lowered[index]);

        if (character >= 'A' && character <= 'Z')
            lowered[index] = static_cast<char>(character - 'A' + 'a');
    }

    return lowered;
}

bool Bot::consumeBotMention(std::string &text) const {
    if (user == NULL)
        return false;

    const std::string &nickname = user->getNickname();

    if (text.size() < nickname.size())
        return false;

    if (!IrcCasemap::equal(text.substr(0, nickname.size()), nickname))
        return false;

    std::string::size_type index = nickname.size();

    if (index == text.size()) {
        text.clear();
        return true;
    }

    if (text[index] == ':' || text[index] == ',')
        ++index;
    else if (text[index] != ' ')
        return false;

    while (index < text.size() && text[index] == ' ')
        ++index;

    text = text.substr(index);
    return true;
}

/**
 * @brief Turns a PRIVMSG body into a command name and optional argument.
 * Direct messages treat the whole line as a command. Channel messages only
 * fire on a leading '!' or a mention of the bot nickname.
 */
bool Bot::extractCommand(
    const std::string &text,
    bool fromChannel,
    std::string &command,
    std::string &argument
) const {
    std::string line = trim(text);

    if (line.empty())
        return false;

    if (!line.empty() && line[0] == CTCP_MARKER)
        return false;

    if (fromChannel) {
        if (line[0] == '!')
            line = trim(line.substr(1));
        else if (!consumeBotMention(line))
            return false;
    } else if (line[0] == '!') {
        line = trim(line.substr(1));
    }

    if (line.empty())
        return false;

    const std::string::size_type spacePosition = line.find(' ');

    if (spacePosition == std::string::npos) {
        command = toLowerAscii(line);
        argument.clear();
        return true;
    }

    command = toLowerAscii(line.substr(0, spacePosition));
    argument = trim(line.substr(spacePosition + 1));
    return true;
}

bool Bot::handleCtcp(Client &sender, const std::string &text) {
    if (text.empty() || text[0] != CTCP_MARKER)
        return false;

    std::string payload = text.substr(1);

    if (!payload.empty() && payload[payload.size() - 1] == CTCP_MARKER)
        payload.erase(payload.size() - 1);

    if (payload.empty())
        return true;

    std::string ctcpCommand;
    std::string ctcpArgument;
    const std::string::size_type spacePosition = payload.find(' ');

    if (spacePosition == std::string::npos) {
        ctcpCommand = toLowerAscii(payload);
    } else {
        ctcpCommand = toLowerAscii(payload.substr(0, spacePosition));
        ctcpArgument = payload.substr(spacePosition + 1);
    }

    if (ctcpCommand == BotConstants::VERSION) {
        sendNotice(sender, wrapCtcp(Constants::BOT_VERSION));
        return true;
    }

    if (ctcpCommand == BotConstants::PING) {
        sendNotice(
            sender,
            wrapCtcp(ctcpArgument.empty() ?
                Constants::PING_CMD :
                "PING " + ctcpArgument));
        return true;
    }

    if (ctcpCommand == BotConstants::TIME) {
        sendNotice(
            sender,
            wrapCtcp("TIME " + formatNow("%Y-%m-%d %H:%M:%S")));
        return true;
    }

    return true;
}

std::string Bot::runCommand(
    Client &sender,
    const std::string &command,
    const std::string &argument
) {
    if (command == BotConstants::HELP) {
        return Constants::BOT_HELP;
    }

    if (command == BotConstants::PING)
        return BotConstants::PONG;

    if (command == BotConstants::TIME)
        return "Local time is " + formatNow("%Y-%m-%d %H:%M:%S");

    if (command == BotConstants::DATE)
        return "Local date is " + formatNow("%Y-%m-%d");

    if (command == BotConstants::VERSION)
        return Constants::BOT_VERSION_CXX98;

    if (command == BotConstants::UPTIME)
        return "I have been online for " + formatUptime();

    if (command == BotConstants::USERS) {
        std::ostringstream stream;

        stream << server.getRegisteredNicknameCount()
            << " nickname(s) currently registered";
        return stream.str();
    }

    if (command == BotConstants::INFO) {
        std::ostringstream stream;

        stream << "I am " << NICKNAME
            << " on " << server.getServerName()
            << ". " << server.getRegisteredNicknameCount()
            << " nickname(s) online, up " << formatUptime()
            << ". Home channel: " << HOME_CHANNEL;
        return stream.str();
    }

    if (command == BotConstants::WHOAMI)
        return "You are " + server.getClientPrefix(sender);

    if (command == BotConstants::ECHO) {
        if (argument.empty())
            return Constants::BOT_ECHO_USAGE;
        return argument;
    }

    if (command == BotConstants::DICE || command == BotConstants::ROLL) {
        std::ostringstream stream;

        stream << "You rolled a " << (std::rand() % 6 + 1);
        return stream.str();
    }

    return "Unknown command \"" + command + "\". Try help.";
}

std::string Bot::formatNow(const char *format) const {
    const std::time_t now = std::time(NULL);

    if (now == static_cast<std::time_t>(-1))
        return BotConstants::UNKNOWN;

    const std::tm *localTime = std::localtime(&now);

    if (localTime == NULL)
        return BotConstants::UNKNOWN;

    char buffer[64];

    if (std::strftime(buffer, sizeof(buffer), format, localTime) == 0)
        return BotConstants::UNKNOWN;

    return buffer;
}

std::string Bot::formatUptime() const {
    const std::time_t now = std::time(NULL);

    if (now == static_cast<std::time_t>(-1) || startedAt <= 0)
        return BotConstants::UNKNOWN;

    int64_t elapsed = static_cast<int64_t>(now - startedAt);

    if (elapsed < 0)
        elapsed = 0;

    const int64_t hours = elapsed / 3600;
    const int64_t minutes = (elapsed % 3600) / 60;
    const int64_t seconds = elapsed % 60;
    std::ostringstream stream;

    stream << hours << "h " << minutes << "m " << seconds << "s";
    return stream.str();
}

std::string Bot::clipTrailing(
    const std::string &command,
    const std::string &target,
    const std::string &text
) const {
    const std::string prefix = server.getClientPrefix(*user);
    const std::size_t overhead =
        prefix.size() + command.size() + target.size() + 7;

    if (overhead >= IRC_MAX_MESSAGE_LENGTH)
        return "";

    const std::size_t maximumText = IRC_MAX_MESSAGE_LENGTH - overhead;

    if (text.size() <= maximumText)
        return text;

    return text.substr(0, maximumText);
}

void Bot::handleDirectMessage(Client &sender, const std::string &text) {
    if (user == NULL || owns(sender))
        return;

    if (handleCtcp(sender, text))
        return;

    std::string command;
    std::string argument;

    if (!extractCommand(text, false, command, argument)) {
        sendPrivateReply(sender, "I did not catch that. Try help.");
        return;
    }

    sendPrivateReply(sender, runCommand(sender, command, argument));
}

void Bot::handleChannelMessage(
    Client &sender,
    Channel &channel,
    const std::string &text
) {
    if (user == NULL || owns(sender) || !channel.hasMember(user))
        return;

    if (!text.empty() && text[0] == CTCP_MARKER)
        return;

    std::string command;
    std::string argument;

    if (!extractCommand(text, true, command, argument))
        return;

    sendChannelReply(channel, runCommand(sender, command, argument));
}

void Bot::handleInvite(Channel &channel) {
    if (user == NULL || channel.hasMember(user))
        return;

    joinChannel(channel, true);
}

void Bot::handleKick(const std::string &channelName) {
    if (user == NULL)
        return;

    if (!IrcCasemap::equal(channelName, HOME_CHANNEL))
        return;

    Channel *channel = server.findOrCreateChannel(HOME_CHANNEL);

    if (channel == NULL)
        return;

    joinChannel(*channel, false);
}
