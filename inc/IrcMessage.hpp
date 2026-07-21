#ifndef IRC_MESSAGE_HPP
#define IRC_MESSAGE_HPP

#include <string>
#include <vector>

struct IrcMessage
{
    std::string prefix;
    std::string cmd;
    std::vector<std::string> params;
};

#endif