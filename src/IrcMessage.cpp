#include "IrcMessage.hpp"

#include <stdexcept>

namespace
{
    void validateCommand(const std::string &cmd)
    {
        if (cmd.empty()
            || cmd.find(' ') != std::string::npos
            || cmd.find('\t') != std::string::npos
            || cmd.find('\r') != std::string::npos
            || cmd.find('\n') != std::string::npos
            || cmd.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid command");
    }

    void validateField(const std::string &field, const std::string &fieldName)
    {
        if (field.find('\r') != std::string::npos
            || field.find('\n') != std::string::npos
            || field.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid " + fieldName);
    }
}

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

    validateCommand(cmd);

    if (!prefix.empty())
    {
        validateField(prefix, "prefix");
        result += ':';
        result += prefix;
        result += ' ';
    }

    result += cmd;

    for (std::size_t i = 0; i < params.size(); ++i)
    {
        const std::string &param = params[i];

        validateField(param, "parameter");

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
