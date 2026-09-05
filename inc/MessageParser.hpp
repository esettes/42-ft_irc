// Copyright 2026 @esettes, @danielfdez17
#ifndef INC_MESSAGEPARSER_HPP_
#define INC_MESSAGEPARSER_HPP_

#include <string>

#include "IrcMessage.hpp"

/**
 * @file MessageParser.hpp
 * @brief Declares helpers for parsing raw IRC protocol lines into structured messages.
 */
class MessageParser {
 private:
        MessageParser();

 public:
        static IrcMessage parse(const std::string &line);
};

#endif  // INC_MESSAGEPARSER_HPP_
