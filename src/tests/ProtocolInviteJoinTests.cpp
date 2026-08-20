#include "ProtocolTestHarness.hpp"

/**
 * @brief Phase 15 — JOIN on an invite-only channel is rejected until INVITE
 * succeeds, the invitation is consumed after a successful JOIN, and a later
 * JOIN without a new invitation is rejected again.
 */
static void testInviteOnlyJoinAndConsumption()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 15 INVITE JOIN tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int guestSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1 || guestSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 15 INVITE JOIN tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(guestSocketFd);
        return;
    }

    registerClient(operatorSocketFd, "operador");

    const std::string invitedWelcome =
        registerClient(guestSocketFd, "roxana");

    std::string guestPrefix;

    if (!extractClientPrefixFromWelcome(
            invitedWelcome,
            "roxana",
            guestPrefix
        ))
    {
        reportFailure(
            "Welcome should expose a prefix for phase 15 INVITE JOIN tests",
            "welcome containing roxana!roxana@host",
            invitedWelcome
        );
        closeSocket(operatorSocketFd);
        closeSocket(guestSocketFd);
        return;
    }

    sendAll(operatorSocketFd, "JOIN #privado\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(operatorSocketFd, "MODE #privado +i\r\n");
    discardPendingData(operatorSocketFd);

    expectEqual(
        sendLineAndReceive(guestSocketFd, "JOIN #privado\r\n"),
        ":irc.42.local 473 roxana #privado :Cannot join channel (+i)\r\n",
        "Phase 15: JOIN without an invitation should return 473"
    );

    sendAll(operatorSocketFd, "INVITE roxana #privado\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(guestSocketFd);

    sendAll(guestSocketFd, "JOIN #privado\r\n");

    const std::string joinResponse =
        receiveAvailableData(guestSocketFd, 500);

    expectContains(
        joinResponse,
        ":" + guestPrefix + " JOIN :#privado\r\n",
        "Phase 15: an invited client should be able to JOIN a +i channel"
    );

    discardPendingData(operatorSocketFd);

    sendAll(guestSocketFd, "PART #privado\r\n");
    discardPendingData(guestSocketFd);
    discardPendingData(operatorSocketFd);

    expectEqual(
        sendLineAndReceive(guestSocketFd, "JOIN #privado\r\n"),
        ":irc.42.local 473 roxana #privado :Cannot join channel (+i)\r\n",
        "Phase 15: a consumed invitation should not allow a later JOIN"
    );

    closeSocket(operatorSocketFd);
    closeSocket(guestSocketFd);
}

/**
 * @brief Phase 15 — an invitation only bypasses +i. A wrong or missing key
 * still rejects JOIN without consuming the invitation; the correct key then
 * succeeds and consumes it.
 */
static void testInviteDoesNotBypassChannelKey()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 15 INVITE +k tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int guestSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1 || guestSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 15 INVITE +k tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(guestSocketFd);
        return;
    }

    registerClient(operatorSocketFd, "operador");
    registerClient(guestSocketFd, "roxana");

    sendAll(operatorSocketFd, "JOIN #privado\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(operatorSocketFd, "MODE #privado +i\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(operatorSocketFd, "MODE #privado +k secreto\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(operatorSocketFd, "INVITE roxana #privado\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(guestSocketFd);

    expectEqual(
        sendLineAndReceive(
            guestSocketFd,
            "JOIN #privado incorrecta\r\n"
        ),
        ":irc.42.local 475 roxana #privado :Cannot join channel (+k)\r\n",
        "Phase 15: an invited JOIN with the wrong key should return 475"
    );

    expectEqual(
        sendLineAndReceive(guestSocketFd, "JOIN #privado\r\n"),
        ":irc.42.local 475 roxana #privado :Cannot join channel (+k)\r\n",
        "Phase 15: an invited JOIN without a key should return 475"
    );

    sendAll(guestSocketFd, "JOIN #privado secreto\r\n");

    const std::string successfulJoin =
        receiveAvailableData(guestSocketFd, 500);

    expectContains(
        successfulJoin,
        " JOIN :#privado\r\n",
        "Phase 15: an invited JOIN with the correct key should succeed"
    );
    expectTrue(
        successfulJoin.find(" 475 ") == std::string::npos,
        "Phase 15: a successful keyed JOIN should not return 475",
        "JOIN without 475",
        successfulJoin
    );

    discardPendingData(operatorSocketFd);

    sendAll(guestSocketFd, "PART #privado\r\n");
    discardPendingData(guestSocketFd);
    discardPendingData(operatorSocketFd);

    expectEqual(
        sendLineAndReceive(
            guestSocketFd,
            "JOIN #privado secreto\r\n"
        ),
        ":irc.42.local 473 roxana #privado :Cannot join channel (+i)\r\n",
        "Phase 15: the invitation should be consumed only after a successful JOIN"
    );

    closeSocket(operatorSocketFd);
    closeSocket(guestSocketFd);
}

/**
 * @brief Phase 15 — disconnecting an invited client clears the pending
 * invitation, so a later connection with the same nickname cannot JOIN a +i
 * channel without a new INVITE.
 */
static void testInviteClearedOnDisconnect()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 15 INVITE cleanup tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int guestSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1 || guestSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 15 INVITE cleanup tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(guestSocketFd);
        return;
    }

    registerClient(operatorSocketFd, "operador");
    registerClient(guestSocketFd, "roxana");

    sendAll(operatorSocketFd, "JOIN #privado\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(operatorSocketFd, "MODE #privado +i\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(operatorSocketFd, "INVITE roxana #privado\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(guestSocketFd);

    sendAll(guestSocketFd, "QUIT :leaving\r\n");
    waitForSocketClosure(guestSocketFd, 1000);
    closeSocket(guestSocketFd);

    sendAll(operatorSocketFd, "PING :pump\r\n");
    discardPendingData(operatorSocketFd);

    const int reconnectedSocketFd = connectToServer(server.getPort());

    if (reconnectedSocketFd == -1)
    {
        reportFailure(
            "A new client should connect after the invited user quits",
            "successful connection",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        return;
    }

    registerClient(reconnectedSocketFd, "roxana");

    expectEqual(
        sendLineAndReceive(reconnectedSocketFd, "JOIN #privado\r\n"),
        ":irc.42.local 473 roxana #privado :Cannot join channel (+i)\r\n",
        "Phase 15: QUIT should clear a pending invitation"
    );

    closeSocket(operatorSocketFd);
    closeSocket(reconnectedSocketFd);
}

int main()
{
    testInviteOnlyJoinAndConsumption();
    testInviteDoesNotBypassChannelKey();
    testInviteClearedOnDisconnect();

    return finishSuite("phase 15 INVITE JOIN");
}
