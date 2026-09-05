// Copyright 2026 @esettes, @danielfdez17
#include <cstdlib>
#include <iostream>

#include "Irc.hpp"
#include "MessageParser.hpp"

namespace {
int fail(const std::string &message) {
    std::cerr << "MessageParser test failed: " << message << std::endl;
    return EXIT_FAILURE;
}
}  // namespace

int main() {
    const IrcMessage message = MessageParser::parse(
        "privmsg #general :Hola a todo el mundo");

    if (message.cmd != Constants::PRIVMSG_CMD) {
        return fail("command must be normalized to uppercase");
    }
    if (message.params.size() != 2) {
        return fail("command must have exactly two parameters");
    }
    if (message.params[0] != "#general") {
        return fail("first parameter must keep channel name");
    }
    if (message.params[1] != "Hola a todo el mundo") {
        return fail("trailing parameter must preserve spaces");
    }

    const IrcMessage explicitTrailingMessage =
        MessageParser::parse("QUIT :bye");

    if (!explicitTrailingMessage.hasTrailingParameter) {
        return fail("parser must remember an explicit trailing parameter");
    }

    if (explicitTrailingMessage.serialize() != "QUIT :bye\r\n") {
        return fail("serialization must preserve an explicit trailing marker");
    }

    const IrcMessage regularParameterMessage =
        MessageParser::parse("QUIT bye");

    if (regularParameterMessage.hasTrailingParameter) {
        return fail("regular parameter must not be marked as trailing");
    }

    if (regularParameterMessage.serialize() != "QUIT bye\r\n") {
        return fail("regular parameter serialization must not add a colon");
    }

    const IrcMessage emptyTopicMessage =
        MessageParser::parse("TOPIC #general :");

    if (emptyTopicMessage.cmd != Constants::TOPIC_CMD) {
        return fail("TOPIC command must be recognized");
    }
    if (emptyTopicMessage.params.size() != 2) {
        return fail("TOPIC with an empty"
            " trailing parameter must keep two parameters");
    }
    if (emptyTopicMessage.params[0] != "#general") {
        return fail("TOPIC must keep the channel name");
    }
    if (!emptyTopicMessage.params[1].empty()) {
        return fail("an empty trailing parameter"
            " must be preserved as an empty string");
    }
    if (!emptyTopicMessage.hasTrailingParameter) {
        return fail("TOPIC #general : must be"
            " marked as having a trailing parameter");
    }

    const std::string dccLine =
        "PRIVMSG Roxana :"
        "\x01"
        "DCC SEND example.txt 2130706433 5000 1200"
        "\x01";
    const IrcMessage dccMessage = MessageParser::parse(dccLine);

    if (dccMessage.cmd != Constants::PRIVMSG_CMD) {
        return fail("DCC CTCP must parse as PRIVMSG");
    }
    if (dccMessage.params.size() != 2) {
        return fail("DCC CTCP must keep target and trailing payload");
    }
    if (dccMessage.params[0] != "Roxana") {
        return fail("DCC CTCP must keep the recipient nickname");
    }
    if (dccMessage.params[1]
        != std::string("\x01") + "DCC SEND example.txt"
        " 2130706433 5000 1200" + "\x01") {
        return fail("DCC CTCP must preserve SOH markers and spaces");
    }
    if (!dccMessage.hasTrailingParameter) {
        return fail("DCC CTCP must stay a trailing parameter");
    }
    if (dccMessage.serialize() != dccLine + "\r\n") {
        return fail("DCC CTCP must serialize back to the original payload");
    }

    std::cout << "MessageParser test passed" << std::endl;
    return EXIT_SUCCESS;
}
