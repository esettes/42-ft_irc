#include "MessageParser.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
    int fail(const std::string &message)
    {
        std::cerr << "MessageParser test failed: " << message << std::endl;
        return EXIT_FAILURE;
    }
}

int main()
{
    const IrcMessage message = MessageParser::parse(
        "privmsg #general :Hola a todo el mundo");

    if (message.cmd != "PRIVMSG")
    {
        return fail("command must be normalized to uppercase");
    }
    if (message.params.size() != 2)
    {
        return fail("command must have exactly two parameters");
    }
    if (message.params[0] != "#general")
    {
        return fail("first parameter must keep channel name");
    }
    if (message.params[1] != "Hola a todo el mundo")
    {
        return fail("trailing parameter must preserve spaces");
    }

    const IrcMessage explicitTrailingMessage =
    MessageParser::parse("QUIT :bye");

    if (!explicitTrailingMessage.hasTrailingParameter)
    {
        return fail("parser must remember an explicit trailing parameter");
    }

    if (explicitTrailingMessage.serialize() != "QUIT :bye\r\n")
    {
        return fail("serialization must preserve an explicit trailing marker");
    }

    const IrcMessage regularParameterMessage =
        MessageParser::parse("QUIT bye");

    if (regularParameterMessage.hasTrailingParameter)
    {
        return fail("regular parameter must not be marked as trailing");
    }

    if (regularParameterMessage.serialize() != "QUIT bye\r\n")
    {
        return fail("regular parameter serialization must not add a colon");
    }

    std::cout << "MessageParser test passed" << std::endl;
    return EXIT_SUCCESS;
}
