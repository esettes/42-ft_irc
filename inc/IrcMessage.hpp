#ifndef IRC_MESSAGE_HPP
#define IRC_MESSAGE_HPP

#include <string>
#include <vector>

struct IrcMessage
{
    std::string prefix;
    std::string cmd;
    std::vector<std::string> params;

    IrcMessage();
    
    IrcMessage(
        const std::string &msgCmd,
        const std::vector<std::string> &msgParams,
        const std::string &msgPrefix = ""
    );

    const std::string &getCommand() const;
    std::string serialize() const;
};

#endif
