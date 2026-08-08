#include "IrcMessage.hpp"

#include <stdexcept>

/**
 * 
 */
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

    if (!prefix.empty())
    {
        if (prefix.find('\r') != std::string::npos
            || prefix.find('\n') != std::string::npos
            || prefix.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid prefix");
        result += ':';
        result += prefix;
        result += ' ';
    }

    result += cmd;

    for (std::size_t i = 0; i < params.size(); ++i)
    {
        const std::string &param = params[i];

        if (param.find('\r') != std::string::npos
            || param.find('\n') != std::string::npos
            || param.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid parameter");

        if (i + 1 == params.size()
            && (param.empty()
                || param.find(' ') != std::string::npos
                || param.find('\t') != std::string::npos))
        {
            result += " :";
            result += param;
        }
        else
        {
            result += ' ';
            result += param;
        }
    }

    result += "\r\n";
    return result;
}

const std::string &IrcMessage::getCommand() const
{
    return cmd;
}
