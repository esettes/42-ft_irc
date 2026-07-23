#include "Client.hpp"

Client::Client(int socketFd)
    : socketFd(socketFd), 
    inputBuffer(""), 
    outputBuffer(""), 
    nickname(""), 
    username(""), 
    realname(""), 
    passwordAccepted(false), 
    registered(false)
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
    registered(other.registered)
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
        registered = other.registered;
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
    return !nickname.empty() && !username.empty() && !realname.empty() && passwordAccepted;
}

bool Client::isRegistered() const
{
    return registered;
}

bool Client::isPasswordAccepted() const
{
    return passwordAccepted;
}

void Client::setPasswordAccepted(bool accepted)
{
    passwordAccepted = accepted;
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

void Client::removeSentOutput(std::size_t bytesSendt)
{
    if (bytesSendt >= outputBuffer.size())
    {
        outputBuffer.clear();
        return;
    }

    outputBuffer.erase(0, bytesSendt);
}
