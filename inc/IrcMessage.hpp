#ifndef IRC_MESSAGE_HPP
#define IRC_MESSAGE_HPP

#include <string>
#include <vector>

/**
 * @file IrcMessage.hpp
 * @brief Declares the parsed IRC message representation shared across the server.
 * 
 * @param prefix The optional prefix of the message, typically indicating the sender.
 * @param cmd The command of the message, which can be a standard IRC command or a numeric reply.
 * @param params The parameters of the message, which can include channel names, usernames, and message text.
 */
struct IrcMessage
{
    std::string prefix;
    std::string cmd;
    std::vector<std::string> params;
    bool hasTrailingParameter;

    IrcMessage();
    
    IrcMessage(
        const std::string &msgCmd,
        const std::vector<std::string> &msgParams,
        const std::string &msgPrefix = "",
        bool messageHasTrailingParameter = false
    );

    const std::string &getCommand() const;
    std::string serialize() const;
};

#endif
