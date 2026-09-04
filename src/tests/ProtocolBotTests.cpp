#include "ProtocolTestHarness.hpp"

namespace
{
    const char *BOT_PREFIX = "marvin!bot@bot.local";
    const char CTCP_MARKER = '\x01';

    std::string wrapCtcp(const std::string &payload)
    {
        return std::string(1, CTCP_MARKER) + payload + std::string(1, CTCP_MARKER);
    }

    bool connectRegisteredClient(
        TestServerProcess &server,
        int &socketFd,
        const std::string &nickname,
        const std::string &testName
    )
    {
        if (!startServerOrFail(server, testName))
            return false;

        socketFd = connectToServer(server.getPort());

        if (socketFd == -1)
        {
            reportFailure(
                testName,
                "successful connection",
                "connection failed"
            );
            return false;
        }

        const std::string welcome = registerClient(socketFd, nickname);

        if (welcome.find(" 001 ") == std::string::npos)
        {
            reportFailure(
                testName,
                "welcome 001 after registration",
                welcome
            );
            closeSocket(socketFd);
            socketFd = -1;
            return false;
        }

        return true;
    }
}

/**
 * @brief Bonus — the built-in bot reserves its nickname before any TCP
 * client can register, so NICK marvin must produce ERR_NICKNAMEINUSE.
 */
static void testBotNicknameIsReserved()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for bot nickname reservation tests"
        ))
    {
        return;
    }

    const int socketFd = connectToServer(server.getPort());

    if (socketFd == -1)
    {
        reportFailure(
            "A client should connect to test the reserved bot nick",
            "successful connection",
            "connection failed"
        );
        return;
    }

    sendAll(socketFd, "PASS secret\r\nNICK marvin\r\n");

    expectContains(
        receiveAvailableData(socketFd, 500),
        " 433 * marvin :Nickname is already in use\r\n",
        "Bot: NICK marvin should be rejected with 433"
    );

    sendAll(socketFd, "NICK MARVIN\r\n");

    expectContains(
        receiveAvailableData(socketFd, 500),
        " 433 * MARVIN :Nickname is already in use\r\n",
        "Bot: the reserved nickname must follow IRC casemapping"
    );

    closeSocket(socketFd);
}

/**
 * @brief Bonus — PRIVMSG to the bot is answered in a query, including the
 * bang-prefixed form used in channels.
 */
static void testDirectHelpAndPing()
{
    TestServerProcess server;
    int socketFd = -1;

    if (!connectRegisteredClient(
            server,
            socketFd,
            "alice",
            "Server should start for bot query tests"
        ))
    {
        return;
    }

    expectEqual(
        sendLineAndReceive(socketFd, "PRIVMSG marvin :ping\r\n"),
        std::string(":") + BOT_PREFIX + " PRIVMSG alice :pong\r\n",
        "Bot: PRIVMSG marvin :ping should reply with pong"
    );

    const std::string helpResponse =
        sendLineAndReceive(socketFd, "PRIVMSG marvin :help\r\n");

    expectContains(
        helpResponse,
        std::string(":") + BOT_PREFIX + " PRIVMSG alice :",
        "Bot: help should arrive as a private PRIVMSG from marvin"
    );
    expectContains(
        helpResponse,
        "ping",
        "Bot: help should list the ping command"
    );

    expectEqual(
        sendLineAndReceive(socketFd, "PRIVMSG marvin :!ping\r\n"),
        std::string(":") + BOT_PREFIX + " PRIVMSG alice :pong\r\n",
        "Bot: a leading '!' in a query should still run the command"
    );

    closeSocket(socketFd);
}

/**
 * @brief Bonus — nickname lookup is casemapped and unknown commands get a
 * hint instead of ERR_NOSUCHNICK.
 */
static void testCasemapEchoAndUnknownCommand()
{
    TestServerProcess server;
    int socketFd = -1;

    if (!connectRegisteredClient(
            server,
            socketFd,
            "alice",
            "Server should start for bot casemap tests"
        ))
    {
        return;
    }

    expectEqual(
        sendLineAndReceive(socketFd, "PRIVMSG MARVIN :echo hello world\r\n"),
        std::string(":") + BOT_PREFIX + " PRIVMSG alice :hello world\r\n",
        "Bot: MARVIN should resolve to the bot and echo the argument"
    );

    expectContains(
        sendLineAndReceive(socketFd, "PRIVMSG marvin :foobar\r\n"),
        "Unknown command \"foobar\"",
        "Bot: unknown query commands should return a hint"
    );

    const std::string timeResponse =
        sendLineAndReceive(socketFd, "PRIVMSG marvin :time\r\n");

    expectContains(
        timeResponse,
        std::string(":") + BOT_PREFIX + " PRIVMSG alice :Local time is ",
        "Bot: time should answer with the local timestamp prefix"
    );

    closeSocket(socketFd);
}

/**
 * @brief Bonus — the bot is a real member of #bot, appears in NAMES as an
 * operator, and answers '!command' and nickname mentions there.
 */
static void testHomeChannelMembershipAndCommands()
{
    TestServerProcess server;
    int socketFd = -1;

    if (!connectRegisteredClient(
            server,
            socketFd,
            "alice",
            "Server should start for bot channel tests"
        ))
    {
        return;
    }

    sendAll(socketFd, "JOIN #bot\r\n");

    const std::string joinResponse = receiveAvailableData(socketFd, 500);

    expectContains(
        joinResponse,
        " JOIN :#bot\r\n",
        "Bot: joining #bot should confirm the JOIN"
    );
    expectContains(
        joinResponse,
        " 332 alice #bot :Ask me with !help or /msg marvin help\r\n",
        "Bot: #bot should expose the helper topic"
    );
    expectContains(
        joinResponse,
        "@marvin",
        "Bot: NAMES for #bot should include @marvin"
    );
    expectContains(
        joinResponse,
        "alice",
        "Bot: NAMES for #bot should include the joining client"
    );

    expectEqual(
        sendLineAndReceive(socketFd, "PRIVMSG #bot :!ping\r\n"),
        std::string(":") + BOT_PREFIX + " PRIVMSG #bot :pong\r\n",
        "Bot: !ping in #bot should be answered in the channel"
    );

    expectEqual(
        sendLineAndReceive(socketFd, "PRIVMSG #bot :marvin: ping\r\n"),
        std::string(":") + BOT_PREFIX + " PRIVMSG #bot :pong\r\n",
        "Bot: a nickname mention in #bot should run the command"
    );

    expectEqual(
        sendLineAndReceive(socketFd, "PRIVMSG #bot :hello there\r\n"),
        "",
        "Bot: ordinary channel talk should not trigger a reply"
    );

    closeSocket(socketFd);
}

/**
 * @brief Bonus — INVITE makes the virtual user JOIN the target channel and
 * greet its members.
 */
static void testInviteJoinsBot()
{
    TestServerProcess server;
    int socketFd = -1;

    if (!connectRegisteredClient(
            server,
            socketFd,
            "alice",
            "Server should start for bot INVITE tests"
        ))
    {
        return;
    }

    sendAll(socketFd, "JOIN #lounge\r\n");
    discardPendingData(socketFd);

    sendAll(socketFd, "INVITE marvin #lounge\r\n");

    const std::string inviteResponse = receiveAvailableData(socketFd, 500);

    expectContains(
        inviteResponse,
        ":irc.42.local 341 alice marvin #lounge\r\n",
        "Bot: inviting marvin should confirm with 341"
    );
    expectContains(
        inviteResponse,
        std::string(":") + BOT_PREFIX + " JOIN :#lounge\r\n",
        "Bot: an invited bot should JOIN the channel"
    );
    expectContains(
        inviteResponse,
        std::string(":") + BOT_PREFIX + " PRIVMSG #lounge :Hello! Type !help to see my commands.\r\n",
        "Bot: the invited bot should greet the channel"
    );

    expectEqual(
        sendLineAndReceive(socketFd, "PRIVMSG #lounge :!ping\r\n"),
        std::string(":") + BOT_PREFIX + " PRIVMSG #lounge :pong\r\n",
        "Bot: !ping should work in a channel the bot was invited to"
    );

    closeSocket(socketFd);
}

/**
 * @brief Bonus — NOTICE must not produce an automatic bot reply, matching
 * RFC 2812. Kicking the bot from a non-home channel should leave it out.
 */
static void testNoticeAndKick()
{
    TestServerProcess server;
    int socketFd = -1;

    if (!connectRegisteredClient(
            server,
            socketFd,
            "alice",
            "Server should start for bot NOTICE and KICK tests"
        ))
    {
        return;
    }

    expectEqual(
        sendLineAndReceive(socketFd, "NOTICE marvin :ping\r\n"),
        "",
        "Bot: NOTICE must not trigger an automatic reply"
    );

    sendAll(socketFd, "JOIN #lounge\r\n");
    discardPendingData(socketFd);
    sendAll(socketFd, "INVITE marvin #lounge\r\n");
    discardPendingData(socketFd);

    sendAll(socketFd, "KICK #lounge marvin :go wait outside\r\n");

    const std::string kickResponse = receiveAvailableData(socketFd, 500);

    expectContains(
        kickResponse,
        " KICK #lounge marvin :go wait outside\r\n",
        "Bot: KICK should still notify the channel"
    );
    expectEqual(
        sendLineAndReceive(socketFd, "PRIVMSG #lounge :!ping\r\n"),
        "",
        "Bot: a kicked bot should not answer commands in that channel"
    );

    closeSocket(socketFd);
}

/**
 * @brief Bonus — CTCP VERSION is answered with NOTICE, while DCC CTCP is
 * ignored so file-transfer payloads are not turned into chat replies.
 */
static void testCtcpHandling()
{
    TestServerProcess server;
    int socketFd = -1;

    if (!connectRegisteredClient(
            server,
            socketFd,
            "alice",
            "Server should start for bot CTCP tests"
        ))
    {
        return;
    }

    expectEqual(
        sendLineAndReceive(
            socketFd,
            "PRIVMSG marvin :" + wrapCtcp("VERSION") + "\r\n"
        ),
        std::string(":") + BOT_PREFIX + " NOTICE alice :"
            + wrapCtcp("VERSION ft_irc bot 1.0") + "\r\n",
        "Bot: CTCP VERSION should be answered with NOTICE"
    );

    expectEqual(
        sendLineAndReceive(
            socketFd,
            "PRIVMSG marvin :" + wrapCtcp("DCC SEND file.txt 1 2 3") + "\r\n"
        ),
        "",
        "Bot: DCC CTCP sent to the bot should be ignored"
    );

    closeSocket(socketFd);
}

int main()
{
    testBotNicknameIsReserved();
    testDirectHelpAndPing();
    testCasemapEchoAndUnknownCommand();
    testHomeChannelMembershipAndCommands();
    testInviteJoinsBot();
    testNoticeAndKick();
    testCtcpHandling();

    return finishSuite("IRC bot bonus");
}
