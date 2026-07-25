#include "IrcMessage.hpp"

#include <stdexcept>

IrcMessage::IrcMessage(
        const std::string &msgCmd,
        const std::vector<std::string> &msgParams,
        const std::string &msgPrefix
        )
        :   prefix(msgPrefix),
            cmd(msgCmd),
            params(msgParams)
{
}

IrcMessage::IrcMessage() : prefix(""), cmd(""), params(std::vector<std::string>()) {}

std::string IrcMessage::serialize() const
{
    std::string result;

    if (cmd.empty() || cmd.find(' ') != std::string::npos)
        throw std::runtime_error("Invalid command");
    
    return result;
}

const std::string &IrcMessage::getCommand() const
{
    return cmd;
}
