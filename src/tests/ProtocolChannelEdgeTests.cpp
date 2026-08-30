#include "ProtocolTestHarness.hpp"

/**
 * @brief Phase 19 — the first JOIN creates the channel and grants operator
 * status; the last unexpected disconnect deletes it so a later JOIN recreates
 * it with a new operator.
 */
static void testFirstMemberIsOperatorAndLastDisconnectDeletesChannel()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 19 last-member tests"
        ))
    {
        return;
    }

    const int firstSocketFd = connectToServer(server.getPort());

    if (firstSocketFd == -1)
    {
        reportFailure(
            "Client should connect for phase 19 last-member tests",
            "successful connection",
            "connection failed"
        );
        return;
    }

    const std::string firstWelcome =
        registerClient(firstSocketFd, "alice");

    std::string firstPrefix;

    if (!extractClientPrefixFromWelcome(
            firstWelcome,
            "alice",
            firstPrefix
        ))
    {
        reportFailure(
            "Welcome should expose a prefix for phase 19 last-member tests",
            "welcome containing alice!alice@host",
            firstWelcome
        );
        closeSocket(firstSocketFd);
        return;
    }

    sendAll(firstSocketFd, "JOIN #solo\r\n");

    const std::string firstJoin =
        receiveAvailableData(firstSocketFd, 500);

    expectContains(
        firstJoin,
        ":irc.42.local 353 alice = #solo :@alice\r\n",
        "Phase 19: the first member of a channel should become an operator"
    );

    closeSocket(firstSocketFd);
    ::usleep(150000);

    const int secondSocketFd = connectToServer(server.getPort());

    if (secondSocketFd == -1)
    {
        reportFailure(
            "A later client should connect after the last member disconnects",
            "successful connection",
            "connection failed"
        );
        return;
    }

    registerClient(secondSocketFd, "bob");
    sendAll(secondSocketFd, "JOIN #solo\r\n");

    const std::string secondJoin =
        receiveAvailableData(secondSocketFd, 500);

    expectContains(
        secondJoin,
        ":irc.42.local 353 bob = #solo :@bob\r\n",
        "Phase 19: joining a channel emptied by disconnect should recreate it"
    );
    expectTrue(
        secondJoin.find("alice") == std::string::npos,
        "Phase 19: a disconnected last member should not remain in NAMES",
        "NAMES without alice",
        secondJoin
    );

    closeSocket(secondSocketFd);
}

/**
 * @brief Phase 19 — disconnecting the only operator leaves remaining members
 * in the channel without inventing a replacement operator.
 */
static void testSoleOperatorDisconnectLeavesMembers()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 19 sole-operator tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1 || memberSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 19 sole-operator tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        return;
    }

    const std::string operatorWelcome =
        registerClient(operatorSocketFd, "operador");
    const std::string memberWelcome =
        registerClient(memberSocketFd, "roxana");

    std::string operatorPrefix;
    std::string memberPrefix;

    if (!extractClientPrefixFromWelcome(
            operatorWelcome,
            "operador",
            operatorPrefix
        )
        || !extractClientPrefixFromWelcome(
            memberWelcome,
            "roxana",
            memberPrefix
        ))
    {
        reportFailure(
            "Welcome should expose prefixes for phase 19 sole-operator tests",
            "welcome containing operador and roxana prefixes",
            operatorWelcome + memberWelcome
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        return;
    }

    sendAll(operatorSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(memberSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    closeSocket(operatorSocketFd);

    const std::string memberNotification =
        receiveAvailableData(memberSocketFd, 1000);

    expectContains(
        memberNotification,
        ":" + operatorPrefix + " QUIT :",
        "Phase 19: disconnecting the sole operator should notify remaining members"
    );

    expectEqual(
        sendLineAndReceive(memberSocketFd, "MODE #general +i\r\n"),
        ":irc.42.local 482 roxana #general :You're not channel operator\r\n",
        "Phase 19: remaining members should not inherit operator status"
    );

    sendAll(memberSocketFd, "PRIVMSG #general :sigo dentro\r\n");
    expectEqual(
        receiveAvailableData(memberSocketFd, 200),
        "",
        "Phase 19: a remaining member should still belong to the channel"
    );

    const int observerSocketFd = connectToServer(server.getPort());

    if (observerSocketFd == -1)
    {
        reportFailure(
            "Observer should connect after the sole operator disconnects",
            "successful connection",
            "connection failed"
        );
        closeSocket(memberSocketFd);
        return;
    }

    registerClient(observerSocketFd, "observer");
    sendAll(observerSocketFd, "JOIN #general\r\n");

    const std::string observerJoin =
        receiveAvailableData(observerSocketFd, 500);

    expectContains(
        observerJoin,
        "roxana",
        "Phase 19: NAMES after the operator disconnects should still list remaining members"
    );
    expectTrue(
        observerJoin.find("@roxana") == std::string::npos,
        "Phase 19: the remaining member should not become an operator automatically",
        "NAMES without @roxana",
        observerJoin
    );
    expectTrue(
        observerJoin.find("operador") == std::string::npos,
        "Phase 19: a disconnected operator should not remain in NAMES",
        "NAMES without operador",
        observerJoin
    );

    closeSocket(memberSocketFd);
    closeSocket(observerSocketFd);
}

/**
 * @brief Phase 19 — KICK, PART, QUIT and an unexpected close all leave the
 * target out of the channel while keeping other members connected.
 */
static void testKickPartQuitAndCloseLeaveConsistentMembership()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 19 membership-consistency tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());
    const int targetSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1
        || memberSocketFd == -1
        || targetSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 19 membership-consistency tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        closeSocket(targetSocketFd);
        return;
    }

    const std::string operatorWelcome =
        registerClient(operatorSocketFd, "admin");
    registerClient(memberSocketFd, "usuario");
    registerClient(targetSocketFd, "objetivo");

    std::string operatorPrefix;

    if (!extractClientPrefixFromWelcome(
            operatorWelcome,
            "admin",
            operatorPrefix
        ))
    {
        reportFailure(
            "Welcome should expose a prefix for phase 19 membership-consistency tests",
            "welcome containing admin!admin@host",
            operatorWelcome
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        closeSocket(targetSocketFd);
        return;
    }

    sendAll(operatorSocketFd, "JOIN #kick\r\n");
    discardPendingData(operatorSocketFd);
    sendAll(memberSocketFd, "JOIN #kick\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);
    sendAll(targetSocketFd, "JOIN #kick\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);
    discardPendingData(targetSocketFd);

    sendAll(operatorSocketFd, "KICK #kick objetivo :fuera\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);
    discardPendingData(targetSocketFd);

    expectEqual(
        sendLineAndReceive(targetSocketFd, "PRIVMSG #kick :hola\r\n"),
        ":irc.42.local 404 objetivo #kick :Cannot send to channel\r\n",
        "Phase 19: a kicked user should no longer be able to speak in the channel"
    );

    sendAll(targetSocketFd, "JOIN #part\r\n");
    discardPendingData(targetSocketFd);
    sendAll(memberSocketFd, "JOIN #part\r\n");
    discardPendingData(targetSocketFd);
    discardPendingData(memberSocketFd);

    sendAll(targetSocketFd, "PART #part\r\n");
    discardPendingData(targetSocketFd);
    discardPendingData(memberSocketFd);

    expectEqual(
        sendLineAndReceive(targetSocketFd, "PRIVMSG #part :hola\r\n"),
        ":irc.42.local 404 objetivo #part :Cannot send to channel\r\n",
        "Phase 19: a user who PARTed should no longer be able to speak in the channel"
    );

    sendAll(targetSocketFd, "JOIN #quit\r\n");
    discardPendingData(targetSocketFd);
    sendAll(memberSocketFd, "JOIN #quit\r\n");
    discardPendingData(targetSocketFd);
    discardPendingData(memberSocketFd);

    sendAll(targetSocketFd, "QUIT :bye\r\n");
    waitForSocketClosure(targetSocketFd, 1000);
    closeSocket(targetSocketFd);

    const std::string memberQuit =
        receiveAvailableData(memberSocketFd, 500);

    expectContains(
        memberQuit,
        " QUIT :bye\r\n",
        "Phase 19: QUIT should notify remaining members of the channel"
    );

    const int replacementSocketFd = connectToServer(server.getPort());

    if (replacementSocketFd == -1)
    {
        reportFailure(
            "A replacement client should connect after QUIT",
            "successful connection",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        return;
    }

    registerClient(replacementSocketFd, "objetivo");
    sendAll(replacementSocketFd, "JOIN #close\r\n");
    discardPendingData(replacementSocketFd);
    sendAll(memberSocketFd, "JOIN #close\r\n");
    discardPendingData(replacementSocketFd);
    discardPendingData(memberSocketFd);

    closeSocket(replacementSocketFd);

    const std::string memberCloseNotification =
        receiveAvailableData(memberSocketFd, 1000);

    expectContains(
        memberCloseNotification,
        " QUIT :",
        "Phase 19: an unexpected close should notify remaining members with QUIT"
    );

    expectEqual(
        sendLineAndReceive(memberSocketFd, "PING :still-on-channels\r\n"),
        "PONG :still-on-channels\r\n",
        "Phase 19: remaining members should stay connected after KICK, PART, QUIT and close"
    );

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
}

int main()
{
    testFirstMemberIsOperatorAndLastDisconnectDeletesChannel();
    testSoleOperatorDisconnectLeavesMembers();
    testKickPartQuitAndCloseLeaveConsistentMembership();

    return finishSuite("phase 19 channel edge cases");
}
