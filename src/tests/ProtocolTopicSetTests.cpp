// Copyright 2026 @esettes, @danielfdez17
#include "ProtocolTestHarness.hpp"


/**
 * @brief Phase 14 — setting, replacing and clearing a topic broadcasts the
 * TOPIC message with the sender's full prefix to every member, including the
 * client that requested the change.
 */
static void testTopicSetModifyAndClear()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 14 TOPIC set tests"
        ))
    {
        return;
    }

    const int aliceSocketFd = connectToServer(server.getPort());
    const int bobSocketFd = connectToServer(server.getPort());

    if (aliceSocketFd == -1 || bobSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 14 TOPIC set tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(aliceSocketFd);
        closeSocket(bobSocketFd);
        return;
    }

    const std::string aliceWelcome =
        registerClient(aliceSocketFd, "alice");
    registerClient(bobSocketFd, "bob");

    std::string alicePrefix;

    if (!extractClientPrefixFromWelcome(
            aliceWelcome,
            "alice",
            alicePrefix
        ))
    {
        reportFailure(
            "Welcome should expose a prefix for phase 14 TOPIC set tests",
            "welcome containing alice!alice@host",
            aliceWelcome
        );
        closeSocket(aliceSocketFd);
        closeSocket(bobSocketFd);
        return;
    }

    sendAll(aliceSocketFd, "JOIN #general\r\n");
    discardPendingData(aliceSocketFd);

    sendAll(bobSocketFd, "JOIN #general\r\n");
    discardPendingData(aliceSocketFd);
    discardPendingData(bobSocketFd);

    sendAll(
        aliceSocketFd,
        "TOPIC #general :Nuevo tema del canal\r\n"
    );

    const std::string expectedTopicBroadcast =
        ":" + alicePrefix + " TOPIC #general :Nuevo tema del canal\r\n";

    expectEqual(
        receiveAvailableData(aliceSocketFd, 500),
        expectedTopicBroadcast,
        "Phase 14: setting a topic should notify the sender"
    );
    expectEqual(
        receiveAvailableData(bobSocketFd, 500),
        expectedTopicBroadcast,
        "Phase 14: setting a topic should notify every channel member"
    );

    sendAll(aliceSocketFd, "TOPIC #general :Tema actualizado\r\n");

    const std::string expectedUpdatedBroadcast =
        ":" + alicePrefix + " TOPIC #general :Tema actualizado\r\n";

    expectEqual(
        receiveAvailableData(aliceSocketFd, 500),
        expectedUpdatedBroadcast,
        "Phase 14: replacing a topic should notify the sender"
    );
    expectEqual(
        receiveAvailableData(bobSocketFd, 500),
        expectedUpdatedBroadcast,
        "Phase 14: replacing a topic should notify every channel member"
    );

    sendAll(aliceSocketFd, "TOPIC #general :\r\n");

    const std::string expectedClearedBroadcast =
        ":" + alicePrefix + " TOPIC #general :\r\n";

    expectEqual(
        receiveAvailableData(aliceSocketFd, 500),
        expectedClearedBroadcast,
        "Phase 14: clearing a topic should notify the sender with an empty trailing parameter"
    );
    expectEqual(
        receiveAvailableData(bobSocketFd, 500),
        expectedClearedBroadcast,
        "Phase 14: clearing a topic should notify every channel member"
    );

    expectEqual(
        sendLineAndReceive(bobSocketFd, "TOPIC #general\r\n"),
        ":irc.42.local 331 bob #general :No topic is set\r\n",
        "Phase 14: querying after clearing the topic should return 331"
    );

    closeSocket(aliceSocketFd);
    closeSocket(bobSocketFd);
}

/**
 * @brief Phase 14 — with +t disabled any member may change the topic; with +t
 * enabled only a channel operator may, and a rejected change must leave the
 * stored topic unchanged.
 */
static void testTopicRestrictionMode()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 14 TOPIC +t tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1 || memberSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 14 TOPIC +t tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        return;
    }

    registerClient(operatorSocketFd, "operador");

    const std::string memberWelcome =
        registerClient(memberSocketFd, "roxana");

    std::string memberPrefix;

    if (!extractClientPrefixFromWelcome(
            memberWelcome,
            "roxana",
            memberPrefix
        ))
    {
        reportFailure(
            "Welcome should expose a prefix for phase 14 TOPIC +t tests",
            "welcome containing roxana!roxana@host",
            memberWelcome
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

    sendAll(memberSocketFd, "TOPIC #general :Cualquier miembro\r\n");

    const std::string unrestrictedBroadcast =
        ":" + memberPrefix + " TOPIC #general :Cualquier miembro\r\n";

    expectEqual(
        receiveAvailableData(memberSocketFd, 500),
        unrestrictedBroadcast,
        "Phase 14: a regular member should be able to set the topic when +t is off"
    );
    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        unrestrictedBroadcast,
        "Phase 14: an unrestricted topic change should reach the operator"
    );

    sendAll(operatorSocketFd, "MODE #general +t\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    expectEqual(
        sendLineAndReceive(
            memberSocketFd,
            "TOPIC #general :Intento denegado\r\n"
        ),
        ":irc.42.local 482 roxana #general :You're not channel operator\r\n",
        "Phase 14: a regular member should receive 482 when +t is on"
    );

    expectEqual(
        receiveAvailableData(operatorSocketFd, 200),
        "",
        "Phase 14: a rejected topic change should not be broadcast"
    );

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "TOPIC #general\r\n"),
        ":irc.42.local 332 operador #general :Cualquier miembro\r\n",
        "Phase 14: a rejected topic change should leave the stored topic unchanged"
    );

    sendAll(
        operatorSocketFd,
        "TOPIC #general :Solo el operador\r\n"
    );

    expectContains(
        receiveAvailableData(operatorSocketFd, 500),
        " TOPIC #general :Solo el operador\r\n",
        "Phase 14: a channel operator should still be able to set the topic with +t"
    );
    expectContains(
        receiveAvailableData(memberSocketFd, 500),
        " TOPIC #general :Solo el operador\r\n",
        "Phase 14: an operator topic change should reach regular members"
    );

    sendAll(operatorSocketFd, "MODE #general -t\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    sendAll(memberSocketFd, "TOPIC #general :Otra vez permitido\r\n");

    const std::string restoredBroadcast =
        ":" + memberPrefix + " TOPIC #general :Otra vez permitido\r\n";

    expectEqual(
        receiveAvailableData(memberSocketFd, 500),
        restoredBroadcast,
        "Phase 14: disabling +t should restore unrestricted topic changes"
    );

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
}

int main()
{
    testTopicSetModifyAndClear();
    testTopicRestrictionMode();

    return finishSuite("phase 14 TOPIC set");
}
