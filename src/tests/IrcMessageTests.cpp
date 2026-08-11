#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "IrcMessage.hpp"

static int g_failures = 0;

static void expectTrue(bool condition, const std::string &message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << std::endl;
        ++g_failures;
    }
}

static void expectEqual(const std::string &actual, const std::string &expected, const std::string &message)
{
    if (actual != expected)
    {
        std::cerr << "FAIL: " << message << std::endl;
        std::cerr << "  expected: " << expected << std::endl;
        std::cerr << "  actual  : " << actual << std::endl;
        ++g_failures;
    }
}

static void testSerializeWithTrailingParameter()
{
    std::vector<std::string> params;
    params.push_back("#general");
    params.push_back("Hola a todo el mundo");
    IrcMessage message("PRIVMSG", params);

    expectEqual(message.serialize(), "PRIVMSG #general :Hola a todo el mundo\r\n",
        "serialize should use a trailing parameter for a message with spaces");
}

static void testSerializeWithPrefixAndMultipleParameters()
{
    std::vector<std::string> params;
    params.push_back("roxana");
    params.push_back("0");
    params.push_back("*");
    params.push_back("Roxana Example");
    IrcMessage message("USER", params, "nick!user@host");

    expectEqual(message.serialize(), ":nick!user@host USER roxana 0 * :Roxana Example\r\n",
        "serialize should include prefix and use trailing parameter for the last value");
}

static void testSerializeEmptyTrailingParameter()
{
    std::vector<std::string> params;
    params.push_back("");
    IrcMessage message("PING", params, "", true);

    expectEqual(message.serialize(), "PING :\r\n",
        "serialize should preserve an explicit empty trailing parameter");
}

static void testRejectsInvalidCommand()
{
    std::vector<std::string> params;
    params.push_back("value");
    IrcMessage message("BAD COMMAND", params);

    try
    {
        message.serialize();
        expectTrue(false, "serialize should reject commands containing whitespace");
    }
    catch (const std::runtime_error &)
    {
        expectTrue(true, "serialize rejected invalid command");
    }
}

static void testRejectsCommandWithControlCharacters()
{
    std::vector<std::string> params;
    params.push_back("value");
    IrcMessage message("PING\r", params);

    try
    {
        message.serialize();
        expectTrue(false, "serialize should reject commands containing carriage return");
    }
    catch (const std::runtime_error &)
    {
        expectTrue(true, "serialize rejected invalid command with carriage return");
    }
}

static void testRejectsInvalidPrefix()
{
    std::vector<std::string> params;
    params.push_back("value");
    IrcMessage message("PING", params, "bad\nprefix");

    try
    {
        message.serialize();
        expectTrue(false, "serialize should reject prefixes containing newlines");
    }
    catch (const std::runtime_error &)
    {
        expectTrue(true, "serialize rejected invalid prefix");
    }
}

static void testRejectsInvalidParameter()
{
    std::vector<std::string> params;
    params.push_back(std::string("bad\0value", 9));
    IrcMessage message("PING", params);

    try
    {
        message.serialize();
        expectTrue(false, "serialize should reject parameters containing NUL bytes");
    }
    catch (const std::runtime_error &)
    {
        expectTrue(true, "serialize rejected invalid parameter");
    }
}

static void expectSerializationFailure(
    const IrcMessage &message,
    const std::string &failureMessage
)
{
    try
    {
        message.serialize();
        expectTrue(false, failureMessage);
    }
    catch (const std::runtime_error &)
    {
    }
}

static void testRejectsInvalidMiddleParameters()
{
    std::vector<std::string> parameters;

    parameters.push_back("invalid parameter");
    parameters.push_back("last");

    expectSerializationFailure(
        IrcMessage("COMMAND", parameters),
        "serialize should reject middle parameters containing spaces"
    );

    parameters.clear();
    parameters.push_back("");
    parameters.push_back("last");

    expectSerializationFailure(
        IrcMessage("COMMAND", parameters),
        "serialize should reject empty middle parameters"
    );

    parameters.clear();
    parameters.push_back(":invalid");
    parameters.push_back("last");

    expectSerializationFailure(
        IrcMessage("COMMAND", parameters),
        "serialize should reject middle parameters starting with a colon"
    );
}

static void testPreservesColonAtStartOfTrailingParameter()
{
    std::vector<std::string> parameters;
    parameters.push_back(":literal");

    const IrcMessage message("NOTICE", parameters);

    expectEqual(
        message.serialize(),
        "NOTICE ::literal\r\n",
        "serialize should preserve a colon belonging to trailing content"
    );
}

static void testRejectsTrailingWithoutParameter()
{
    const std::vector<std::string> parameters;
    const IrcMessage message("QUIT", parameters, "", true);

    expectSerializationFailure(
        message,
        "serialize should reject trailing state without a parameter"
    );
}

int main()
{
    testSerializeWithTrailingParameter();
    testSerializeWithPrefixAndMultipleParameters();
    testSerializeEmptyTrailingParameter();
    testRejectsInvalidCommand();
    testRejectsCommandWithControlCharacters();
    testRejectsInvalidPrefix();
    testRejectsInvalidParameter();
    testRejectsInvalidMiddleParameters();
    testPreservesColonAtStartOfTrailingParameter();
    testRejectsTrailingWithoutParameter();

    if (g_failures != 0)
    {
        std::cerr << g_failures << " test(s) failed" << std::endl;
        return 1;
    }

    std::cout << "All IrcMessage serialization tests passed" << std::endl;
    return 0;
}
