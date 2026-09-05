// Copyright 2026 @esettes, @danielfdez17
#include "ProtocolTestHarness.hpp"

/**
 * @brief Phase 17 — +i and -i are broadcast to every member, including the
 * operator. While +i is set, JOIN without an invitation is rejected; -i
 * restores unrestricted entry.
 */
static void testInviteOnlyModeToggle()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 17 MODE +i tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());
    const int guestSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1
        || memberSocketFd == -1
        || guestSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 17 MODE +i tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        closeSocket(guestSocketFd);
        return;
    }

    const std::string operatorWelcome =
        registerClient(operatorSocketFd, "operador");
    registerClient(memberSocketFd, "alice");
    registerClient(guestSocketFd, "roxana");

    std::string operatorPrefix;

    if (!extractClientPrefixFromWelcome(
            operatorWelcome,
            "operador",
            operatorPrefix
        ))
    {
        reportFailure(
            "Welcome should expose a prefix for phase 17 MODE +i tests",
            "welcome containing operador!operador@host",
            operatorWelcome
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        closeSocket(guestSocketFd);
        return;
    }

    sendAll(operatorSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(memberSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    sendAll(operatorSocketFd, "MODE #general +i\r\n");

    const std::string expectedInviteOnly =
        ":" + operatorPrefix + " MODE #general +i\r\n";

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        expectedInviteOnly,
        "Phase 17: enabling +i should notify the operator"
    );
    expectEqual(
        receiveAvailableData(memberSocketFd, 500),
        expectedInviteOnly,
        "Phase 17: enabling +i should notify every channel member"
    );

    expectEqual(
        sendLineAndReceive(guestSocketFd, "JOIN #general\r\n"),
        ":irc.42.local 473 roxana #general :Cannot join channel (+i)\r\n",
        "Phase 17: JOIN without an invitation should fail while +i is set"
    );

    sendAll(operatorSocketFd, "MODE #general -i\r\n");

    const std::string expectedInviteOpen =
        ":" + operatorPrefix + " MODE #general -i\r\n";

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        expectedInviteOpen,
        "Phase 17: disabling +i should notify the operator"
    );
    expectEqual(
        receiveAvailableData(memberSocketFd, 500),
        expectedInviteOpen,
        "Phase 17: disabling +i should notify every channel member"
    );

    sendAll(guestSocketFd, "JOIN #general\r\n");

    expectContains(
        receiveAvailableData(guestSocketFd, 500),
        " JOIN :#general\r\n",
        "Phase 17: JOIN should succeed after +i is removed"
    );

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
    closeSocket(guestSocketFd);
}

/**
 * @brief Phase 17 — +t restricts TOPIC to operators and -t restores the
 * default where any member may change the topic. Both toggles are broadcast.
 */
static void testTopicRestrictedModeToggle()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 17 MODE +t tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1 || memberSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 17 MODE +t tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        return;
    }

    const std::string operatorWelcome =
        registerClient(operatorSocketFd, "operador");
    registerClient(memberSocketFd, "roxana");

    std::string operatorPrefix;

    if (!extractClientPrefixFromWelcome(
            operatorWelcome,
            "operador",
            operatorPrefix
        ))
    {
        reportFailure(
            "Welcome should expose a prefix for phase 17 MODE +t tests",
            "welcome containing operador!operador@host",
            operatorWelcome
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

    sendAll(operatorSocketFd, "MODE #general +t\r\n");

    const std::string expectedTopicRestricted =
        ":" + operatorPrefix + " MODE #general +t\r\n";

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        expectedTopicRestricted,
        "Phase 17: enabling +t should notify the operator"
    );
    expectEqual(
        receiveAvailableData(memberSocketFd, 500),
        expectedTopicRestricted,
        "Phase 17: enabling +t should notify every channel member"
    );

    expectEqual(
        sendLineAndReceive(
            memberSocketFd,
            "TOPIC #general :Intento denegado\r\n"
        ),
        ":irc.42.local 482 roxana #general :You're not channel operator\r\n",
        "Phase 17: a regular member should not change the topic while +t is set"
    );

    sendAll(operatorSocketFd, "MODE #general -t\r\n");

    const std::string expectedTopicOpen =
        ":" + operatorPrefix + " MODE #general -t\r\n";

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        expectedTopicOpen,
        "Phase 17: disabling +t should notify the operator"
    );
    expectEqual(
        receiveAvailableData(memberSocketFd, 500),
        expectedTopicOpen,
        "Phase 17: disabling +t should notify every channel member"
    );

    sendAll(memberSocketFd, "TOPIC #general :Ahora permitido\r\n");

    expectContains(
        receiveAvailableData(memberSocketFd, 500),
        " TOPIC #general :Ahora permitido\r\n",
        "Phase 17: a regular member should be able to set the topic after -t"
    );

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
}

int main()
{
    testInviteOnlyModeToggle();
    testTopicRestrictedModeToggle();

    return finishSuite("phase 17 MODE flags");
}
