#include "Client.hpp"
#include "IrcCasemap.hpp"
#include "IrcMessage.hpp"

Client::Client(int socketFd, const std::string &host)
    : socketFd(socketFd), 
    inputBuffer(""), 
    outputBuffer(""), 
    nickname(""), 
    username(""), 
    realname(""), 
    host(host),
    passwordAccepted(false), 
    nicknameReceived(false), 
    usernameReceived(false), 
    registered(false),
    joinedChannels(),
    isOperator(false),
    disconnectRequested(false),
    disconnectReason("")
{
}

Client::~Client()
{
}

Client::Client(const Client &other)
    : socketFd(other.socketFd), 
    inputBuffer(other.inputBuffer), 
    outputBuffer(other.outputBuffer), 
    nickname(other.nickname), 
    username(other.username), 
    realname(other.realname), 
    host(other.host),
    passwordAccepted(other.passwordAccepted), 
    nicknameReceived(other.nicknameReceived), 
    usernameReceived(other.usernameReceived), 
    registered(other.registered),
    joinedChannels(other.joinedChannels),
    isOperator(other.isOperator),
    disconnectRequested(other.disconnectRequested),
    disconnectReason(other.disconnectReason)
{
}

Client &Client::operator=(const Client &other)
{
    if (this != &other)
    {
        socketFd = other.socketFd;
        inputBuffer = other.inputBuffer;
        outputBuffer = other.outputBuffer;
        nickname = other.nickname;
        username = other.username;
        realname = other.realname;
        host = other.host;
        passwordAccepted = other.passwordAccepted;
        nicknameReceived = other.nicknameReceived;
        usernameReceived = other.usernameReceived;
        registered = other.registered;
        joinedChannels = other.joinedChannels;
        isOperator = other.isOperator;
        disconnectRequested = other.disconnectRequested;
        disconnectReason = other.disconnectReason;
    }
    return *this;
}

int Client::getSocketFd() const
{
    return socketFd;
}

/**
 * @brief Reports whether this client exists only as an IRC identity.
 * Virtual users, such as the built-in bot, have no socket and are not
 * registered in poll().
 */
bool Client::isVirtual() const
{
    return socketFd < 0;
}

void Client::appendToInputBuffer(const std::string &data)
{
    inputBuffer += data;
}

const std::string &Client::getInputBuffer() const
{
    return inputBuffer;
}

void Client::appendToOutputBuffer(const std::string &data)
{
    outputBuffer += data;
}

const std::string &Client::getOutputBuffer() const
{
    return outputBuffer;
}

bool Client::isReadyToRegister() const
{
    return passwordAccepted && nicknameReceived && usernameReceived;
}

bool Client::isRegistered() const
{
    return registered;
}

bool Client::isPasswordAccepted() const
{
    return passwordAccepted;
}

bool Client::isNicknameReceived() const
{
    return nicknameReceived;
}

bool Client::isUsernameReceived() const
{
    return usernameReceived;
}

void Client::setPasswordAccepted(bool accepted)
{
    passwordAccepted = accepted;
}

void Client::setNicknameReceived(bool received)
{
    nicknameReceived = received;
}

void Client::setUsernameReceived(bool received)
{
    usernameReceived = received;
}

void Client::setRegistered(bool registered)
{
    this->registered = registered;
}

void Client::setNickname(const std::string &nickname)
{
    this->nickname = nickname;
}

const std::string &Client::getNickname() const
{
    return nickname;
}

void Client::setUsername(const std::string &username)
{
    this->username = username;
}

const std::string &Client::getUsername() const
{
    return username;
}

void Client::setRealname(const std::string &realname)
{
    this->realname = realname;
}

const std::string &Client::getRealname() const
{
    return realname;
}

const std::string &Client::getHost() const
{
    return host;
}

void Client::setIsOperator(bool isOperator)
{
    this->isOperator = isOperator;
}

bool Client::getIsOperator() const
{
    return isOperator;
}

/**
 * @brief Marks the client for deferred disconnection using a generic reason.
 * It does not close the socket or destroy the client.
 */
void Client::requestDisconnect()
{
    requestDisconnect("Client disconnected");
}

/**
 * @brief Marks the client for deferred disconnection and stores its cause.
 * The first request wins so later errors cannot overwrite the original
 * disconnection reason. An empty reason is replaced with a generic one.
 *
 * @param reason The reason why the client must be disconnected.
 */
void Client::requestDisconnect(const std::string &reason)
{
    if (disconnectRequested)
        return;

    disconnectRequested = true;

    if (reason.empty())
    {
        disconnectReason = "Client disconnected";
        return;
    }

    disconnectReason = reason;
}

bool Client::isDisconnectRequested() const
{
    return disconnectRequested;
}

/**
 * @brief Returns the cause stored by the first disconnection request.
 *
 * @return A read-only reference to the stored disconnection reason.
 */
const std::string &Client::getDisconnectReason() const
{
    return disconnectReason;
}

void Client::removeSentOutput(std::size_t sentByteCount)
{
    if (sentByteCount >= outputBuffer.size())
    {
        outputBuffer.clear();
        return;
    }

    outputBuffer.erase(0, sentByteCount);
}

void Client::joinChannel(const std::string &channelName)
{
    joinedChannels.insert(IrcCasemap::normalize(channelName));
}

void Client::leaveChannel(const std::string &channelName)
{
    joinedChannels.erase(IrcCasemap::normalize(channelName));
}

bool Client::isInChannel(const std::string &channelName) const
{
    return joinedChannels.find(IrcCasemap::normalize(channelName))
        != joinedChannels.end();
}

const std::set<std::string> &Client::getJoinedChannels() const
{
    return joinedChannels;
}

/**
 * @brief Extracts the next complete IRC line from the input buffer.
 * A complete line ends with LF, optionally preceded by CR.
 * The extracted content excludes the terminator.
 * Returns LINE_TOO_LONG when the pending or complete line would exceed
 * IRC_MAX_MESSAGE_LENGTH bytes including the terminator. Oversized data is
 * discarded so a later extract cannot loop on the same invalid fragment.
 */
Client::LineReadStatus Client::extractNextLine(std::string &completeLine)
{
    const std::string::size_type newlinePos = inputBuffer.find('\n');

    if (newlinePos == std::string::npos)
    {
        if (inputBuffer.size() > IRC_MAX_MESSAGE_LENGTH - 1)
        {
            inputBuffer.clear();
            return LINE_TOO_LONG;
        }
        return LINE_INCOMPLETE;
    }

    const std::size_t totalLength = newlinePos + 1;
    if (totalLength > IRC_MAX_MESSAGE_LENGTH)
    {
        inputBuffer.erase(0, totalLength);
        return LINE_TOO_LONG;
    }

    std::string::size_type contentLength = newlinePos;
    if (contentLength > 0 && inputBuffer[contentLength - 1] == '\r')
        --contentLength;

    completeLine.assign(inputBuffer, 0, contentLength);
    inputBuffer.erase(0, totalLength);

    return LINE_COMPLETE;
}
