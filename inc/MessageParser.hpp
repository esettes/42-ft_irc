#ifndef MESSAGE_PARSER_HPP
#define MESSAGE_PARSER_HPP

#include "IrcMessage.hpp"
#include <string>

/**
 * @file MessageParser.hpp
 * @brief Declares helpers for parsing raw IRC protocol lines into structured messages.
 */
class MessageParser
{
    private:
        MessageParser();

    public:
        static IrcMessage parse(const std::string &line);
};

#endif
