#ifndef MESSAGE_PARSER_HPP
#define MESSAGE_PARSER_HPP

#include "IrcMessage.hpp"
#include <string>

class MessageParser
{
    public:
        static IrcMessage parse(const std::string &line);

    private:
        MessageParser();
};

#endif
