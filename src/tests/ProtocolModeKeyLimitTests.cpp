#include "ProtocolTestHarness.hpp"

/**
 * @brief Phase 17 — +k stores the channel key, JOIN must supply that exact
 * password, and -k removes the restriction without consuming an argument.
 */
static void testChannelKeyModeAndJoin()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 17 MODE +k tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int guestSocketFd = connectToServer(server.getPort());
    const int laterSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1
        || guestSocketFd == -1
        || laterSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 17 MODE +k tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(guestSocketFd);
        closeSocket(laterSocketFd);
        return;
    }

    const std::string operatorWelcome =
        registerClient(operatorSocketFd, "operador");
    registerClient(guestSocketFd, "roxana");
    registerClient(laterSocketFd, "alice");

    std::string operatorPrefix;

    if (!extractClientPrefixFromWelcome(
            operatorWelcome,
            "operador",
            operatorPrefix
        ))
    {
        reportFailure(
            "Welcome should expose a prefix for phase 17 MODE +k tests",
            "welcome containing operador!operador@host",
            operatorWelcome
        );
        closeSocket(operatorSocketFd);
        closeSocket(guestSocketFd);
        closeSocket(laterSocketFd);
        return;
    }

    sendAll(operatorSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(operatorSocketFd, "MODE #general +k secret\r\n");

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        ":" + operatorPrefix + " MODE #general +k secret\r\n",
        "Phase 17: setting +k should broadcast the key"
    );

    expectEqual(
        sendLineAndReceive(guestSocketFd, "JOIN #general incorrecta\r\n"),
        ":irc.42.local 475 roxana #general :Cannot join channel (+k)\r\n",
        "Phase 17: JOIN with the wrong key should return 475"
    );

    sendAll(guestSocketFd, "JOIN #general secret\r\n");

    expectContains(
        receiveAvailableData(guestSocketFd, 500),
        " JOIN :#general\r\n",
        "Phase 17: JOIN with the correct key should succeed"
    );

    discardPendingData(operatorSocketFd);

    sendAll(operatorSocketFd, "MODE #general -k\r\n");

    const std::string expectedKeyRemoved =
        ":" + operatorPrefix + " MODE #general -k\r\n";

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        expectedKeyRemoved,
        "Phase 17: -k should notify the operator without consuming an argument"
    );
    expectEqual(
        receiveAvailableData(guestSocketFd, 500),
        expectedKeyRemoved,
        "Phase 17: -k should notify every channel member"
    );

    sendAll(laterSocketFd, "JOIN #general\r\n");

    expectContains(
        receiveAvailableData(laterSocketFd, 500),
        " JOIN :#general\r\n",
        "Phase 17: JOIN without a key should succeed after -k"
    );

    closeSocket(operatorSocketFd);
    closeSocket(guestSocketFd);
    closeSocket(laterSocketFd);
}

/**
 * @brief Phase 17 — +l stores a positive member limit, JOIN is rejected with
 * 471 once the channel is full, and -l removes the limit without an argument.
 */
static void testUserLimitModeAndJoin()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 17 MODE +l tests"
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
            "Clients should connect for phase 17 MODE +l tests",
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
            "Welcome should expose a prefix for phase 17 MODE +l tests",
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

    sendAll(operatorSocketFd, "MODE #general +l 2\r\n");

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        ":" + operatorPrefix + " MODE #general +l 2\r\n",
        "Phase 17: setting +l should broadcast the user limit"
    );

    sendAll(memberSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    expectEqual(
        sendLineAndReceive(guestSocketFd, "JOIN #general\r\n"),
        ":irc.42.local 471 roxana #general :Cannot join channel (+l)\r\n",
        "Phase 17: JOIN should return 471 when the channel has reached +l"
    );

    sendAll(operatorSocketFd, "MODE #general -l\r\n");

    const std::string expectedLimitRemoved =
        ":" + operatorPrefix + " MODE #general -l\r\n";

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        expectedLimitRemoved,
        "Phase 17: -l should notify the operator without consuming an argument"
    );
    expectEqual(
        receiveAvailableData(memberSocketFd, 500),
        expectedLimitRemoved,
        "Phase 17: -l should notify every channel member"
    );

    sendAll(guestSocketFd, "JOIN #general\r\n");

    expectContains(
        receiveAvailableData(guestSocketFd, 500),
        " JOIN :#general\r\n",
        "Phase 17: JOIN should succeed after the user limit is removed"
    );

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
    closeSocket(guestSocketFd);
}

int main()
{
    testChannelKeyModeAndJoin();
    testUserLimitModeAndJoin();

    return finishSuite("phase 17 MODE key and limit");
}
