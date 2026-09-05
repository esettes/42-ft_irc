// Copyright 2026 @esettes, @danielfdez17
#include "ProtocolTestHarness.hpp"

/**
 * @brief Phase 14 — querying a channel without a topic returns RPL_NOTOPIC,
 * querying after a topic is stored returns RPL_TOPIC, and casemapped names
 * resolve to the same channel. JOIN of a later member also reports 332.
 */
static void testTopicQueryReplies()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 14 TOPIC query tests"
        ))
    {
        return;
    }

    const int ownerSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());

    if (ownerSocketFd == -1 || memberSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 14 TOPIC query tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(ownerSocketFd);
        closeSocket(memberSocketFd);
        return;
    }

    registerClient(ownerSocketFd, "roxana");
    registerClient(memberSocketFd, "alice");

    sendAll(ownerSocketFd, "JOIN #general\r\n");
    discardPendingData(ownerSocketFd);

    expectEqual(
        sendLineAndReceive(ownerSocketFd, "TOPIC #general\r\n"),
        ":irc.42.local 331 roxana #general :No topic is set\r\n",
        "Phase 14: querying a channel without a topic should return 331"
    );

    sendAll(ownerSocketFd, "TOPIC #general :Tema actual del canal\r\n");
    discardPendingData(ownerSocketFd);

    expectEqual(
        sendLineAndReceive(ownerSocketFd, "TOPIC #general\r\n"),
        ":irc.42.local 332 roxana #general :Tema actual del canal\r\n",
        "Phase 14: querying a channel with a topic should return 332"
    );

    expectEqual(
        sendLineAndReceive(ownerSocketFd, "TOPIC #GENERAL\r\n"),
        ":irc.42.local 332 roxana #general :Tema actual del canal\r\n",
        "Phase 14: TOPIC should find channels with IRC casemapping"
    );

    sendAll(memberSocketFd, "JOIN #general\r\n");
    discardPendingData(ownerSocketFd);

    const std::string memberJoinResponse =
        receiveAvailableData(memberSocketFd, 500);

    expectContains(
        memberJoinResponse,
        ":irc.42.local 332 alice #general :Tema actual del canal\r\n",
        "Phase 14: JOIN should report the stored topic with 332"
    );

    expectTrue(
        memberJoinResponse.find(" 331 alice #general ") == std::string::npos,
        "Phase 14: JOIN should not send 331 when a topic is already set",
        "JOIN without 331",
        memberJoinResponse
    );

    closeSocket(ownerSocketFd);
    closeSocket(memberSocketFd);
}

/**
 * @brief Phase 14 — mode +t does not prevent a regular member from querying
 * the current topic.
 */
static void testTopicQueryIgnoresTopicRestriction()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 14 TOPIC query privilege tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1 || memberSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 14 TOPIC query privilege tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        return;
    }

    registerClient(operatorSocketFd, "operador");
    registerClient(memberSocketFd, "roxana");

    sendAll(operatorSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(memberSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    sendAll(operatorSocketFd, "MODE #general +t\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    sendAll(
        operatorSocketFd,
        "TOPIC #general :Solo operadores pueden cambiarlo\r\n"
    );
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    expectEqual(
        sendLineAndReceive(memberSocketFd, "TOPIC #general\r\n"),
        ":irc.42.local 332 roxana #general :Solo operadores pueden cambiarlo\r\n",
        "Phase 14: a regular member should still be able to query the topic with +t"
    );

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
}

int main()
{
    testTopicQueryReplies();
    testTopicQueryIgnoresTopicRestriction();

    return finishSuite("phase 14 TOPIC query");
}
