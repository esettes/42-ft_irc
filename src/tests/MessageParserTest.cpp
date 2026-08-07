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
        "privmsg #general :Hola a todo el mundo"
    );

    if (message.cmd != "PRIVMSG")
        return fail("command must be normalized to uppercase");
    if (message.params.size() != 2)
        return fail("command must have exactly two parameters");
    if (message.params[0] != "#general")
        return fail("first parameter must keep channel name");
    if (message.params[1] != "Hola a todo el mundo")
        return fail("trailing parameter must preserve spaces");

    std::cout << "MessageParser test passed" << std::endl;
    return EXIT_SUCCESS;
}
