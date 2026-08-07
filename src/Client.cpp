#include "Client.hpp"

Client::Client(int socketFd)
    : socketFd(socketFd), 
    inputBuffer(""), 
    outputBuffer(""), 
    nickname(""), 
    username(""), 
    realname(""), 
    passwordAccepted(false), 
    nicknameReceived(false), 
    usernameReceived(false), 
    registered(false)
    // isOperator(false)
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
    passwordAccepted(other.passwordAccepted), 
    nicknameReceived(other.nicknameReceived), 
    usernameReceived(other.usernameReceived), 
    registered(other.registered)
    // isOperator(other.isOperator)
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
        passwordAccepted = other.passwordAccepted;
        nicknameReceived = other.nicknameReceived;
        usernameReceived = other.usernameReceived;
        registered = other.registered;
        // isOperator = other.isOperator;
    }
    return *this;
}

int Client::getSocketFd() const
{
    return socketFd;
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

void Client::removeSentOutput(std::size_t sentByteCount)
{
    if (sentByteCount >= outputBuffer.size())
    {
        outputBuffer.clear();
        return;
    }

    outputBuffer.erase(0, sentByteCount);
}

/**
 * @brief Extracts the next complete line from the input buffer.
 * A complete line is a sequence of characters ending with a newline character ('\n').
 * If a complete line is found, it is assigned to the provided string reference and removed from
 * the input buffer.
 */
bool Client::extractNextLine(std::string &completeLine)
{
    const std::string::size_type pos = inputBuffer.find('\n');

    if (pos == std::string::npos)
        return false;

    std::string::size_type len = pos;

    if (len > 0 && inputBuffer[len - 1] == '\r')
        --len;

    completeLine.assign(inputBuffer, 0, len);

    inputBuffer.erase(0, pos + 1);

    return true;
}
