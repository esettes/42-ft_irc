#include "ProtocolTestHarness.hpp"

/**
 * @brief Channel PRIVMSG sent before another client JOINs is replayed to
 * that client after JOIN, topic and NAMES. The outsider must not receive
 * the live copy, and later messages still fan out normally.
 */
static void testChannelHistoryIsReplayedOnJoin()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for channel history JOIN tests"
        ))
    {
        return;
    }

    const int aliceSocketFd = connectToServer(server.getPort());
    const int bobSocketFd = connectToServer(server.getPort());
    const int carolSocketFd = connectToServer(server.getPort());

    if (aliceSocketFd == -1 || bobSocketFd == -1 || carolSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for channel history JOIN tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(aliceSocketFd);
        closeSocket(bobSocketFd);
        closeSocket(carolSocketFd);
        return;
    }

    const std::string aliceWelcome =
        registerClient(aliceSocketFd, "alice");
    registerClient(bobSocketFd, "bob");
    registerClient(carolSocketFd, "carol");

    std::string alicePrefix;

    if (!extractClientPrefixFromWelcome(
            aliceWelcome,
            "alice",
            alicePrefix
        ))
    {
        reportFailure(
            "Welcome should expose a prefix for channel history JOIN tests",
            "welcome containing alice!alice@host",
            aliceWelcome
        );
        closeSocket(aliceSocketFd);
        closeSocket(bobSocketFd);
        closeSocket(carolSocketFd);
        return;
    }

    sendAll(aliceSocketFd, "JOIN #general\r\n");
    discardPendingData(aliceSocketFd);

    sendAll(aliceSocketFd, "PRIVMSG #general :antes de que entre b\r\n");
    sendAll(aliceSocketFd, "PRIVMSG #general :otro anterior\r\n");

    expectEqual(
        receiveAvailableData(aliceSocketFd, 200),
        "",
        "Channel history: the sender should not receive an echo"
    );
    expectEqual(
        receiveAvailableData(bobSocketFd, 200),
        "",
        "Channel history: a client outside the channel should not receive live PRIVMSG"
    );

    sendAll(bobSocketFd, "JOIN #general\r\n");

    const std::string bobJoinResponse =
        receiveAvailableData(bobSocketFd, 500);

    expectContains(
        bobJoinResponse,
        " JOIN :#general\r\n",
        "Channel history: the joining client should receive its own JOIN"
    );
    expectContains(
        bobJoinResponse,
        ":irc.42.local 366 bob #general :End of /NAMES list\r\n",
        "Channel history: history should follow the NAMES sequence"
    );

    const std::string firstHistoryMessage =
        ":" + alicePrefix + " PRIVMSG #general :antes de que entre b\r\n";
    const std::string secondHistoryMessage =
        ":" + alicePrefix + " PRIVMSG #general :otro anterior\r\n";

    expectContains(
        bobJoinResponse,
        firstHistoryMessage,
        "Channel history: JOIN should replay messages sent before the client entered"
    );
    expectContains(
        bobJoinResponse,
        secondHistoryMessage,
        "Channel history: JOIN should replay every stored channel message"
    );

    const std::string::size_type endNamesPosition =
        bobJoinResponse.find(" 366 bob #general ");
    const std::string::size_type firstHistoryPosition =
        bobJoinResponse.find(firstHistoryMessage);
    const std::string::size_type secondHistoryPosition =
        bobJoinResponse.find(secondHistoryMessage);

    expectTrue(
        endNamesPosition != std::string::npos
            && firstHistoryPosition != std::string::npos
            && secondHistoryPosition != std::string::npos
            && endNamesPosition < firstHistoryPosition
            && firstHistoryPosition < secondHistoryPosition,
        "Channel history: stored PRIVMSG should arrive after 366 and keep order",
        "366 then first history then second history",
        bobJoinResponse
    );

    expectEqual(
        receiveAvailableData(carolSocketFd, 200),
        "",
        "Channel history: an unrelated client should not receive replayed PRIVMSG"
    );

    sendAll(aliceSocketFd, "PRIVMSG #general :despues de que entre b\r\n");

    expectEqual(
        receiveAvailableData(bobSocketFd, 500),
        ":" + alicePrefix + " PRIVMSG #general :despues de que entre b\r\n",
        "Channel history: later PRIVMSG should still reach current members"
    );

    closeSocket(aliceSocketFd);
    closeSocket(bobSocketFd);
    closeSocket(carolSocketFd);
}

int main()
{
    testChannelHistoryIsReplayedOnJoin();

    return finishSuite("channel PRIVMSG history on JOIN");
}
