#include "ProtocolTestHarness.hpp"

/**
 * @brief Phase 17 — MODE with only a channel name returns 324 RPL_CHANNELMODEIS.
 * A new channel has no flags. Querying does not require operator privileges
 * and does not require membership.
 */
static void testModeQueryDefaultAndPrivileges()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 17 MODE query tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());
    const int outsiderSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1
        || memberSocketFd == -1
        || outsiderSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 17 MODE query tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        closeSocket(outsiderSocketFd);
        return;
    }

    registerClient(operatorSocketFd, "operador");
    registerClient(memberSocketFd, "roxana");
    registerClient(outsiderSocketFd, "outsider");

    sendAll(operatorSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(memberSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #general\r\n"),
        ":irc.42.local 324 operador #general +\r\n",
        "Phase 17: a new channel should report no active modes"
    );

    expectEqual(
        sendLineAndReceive(memberSocketFd, "MODE #general\r\n"),
        ":irc.42.local 324 roxana #general +\r\n",
        "Phase 17: a regular member should be able to query channel modes"
    );

    expectEqual(
        sendLineAndReceive(outsiderSocketFd, "MODE #general\r\n"),
        ":irc.42.local 324 outsider #general +\r\n",
        "Phase 17: querying modes should not require channel membership"
    );

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #GENERAL\r\n"),
        ":irc.42.local 324 operador #general +\r\n",
        "Phase 17: MODE query should resolve the channel through IRC casemapping"
    );

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
    closeSocket(outsiderSocketFd);
}

/**
 * @brief Phase 17 — RPL_CHANNELMODEIS lists flags in itkl order, includes the
 * key and user limit when those modes are set, and omits +o because operator
 * status is a per-user privilege shown in NAMES instead.
 */
static void testModeQueryReportsActiveFlags()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 17 MODE query flag tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1 || memberSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 17 MODE query flag tests",
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

    sendAll(
        operatorSocketFd,
        "MODE #general +kol secret roxana 10\r\n"
    );
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    sendAll(operatorSocketFd, "MODE #general +it\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #general\r\n"),
        ":irc.42.local 324 operador #general +itkl secret 10\r\n",
        "Phase 17: MODE query should list itkl flags, the key and the limit"
    );

    expectEqual(
        sendLineAndReceive(memberSocketFd, "MODE #general\r\n"),
        ":irc.42.local 324 roxana #general +itkl secret 10\r\n",
        "Phase 17: MODE query should not include +o in the channel flag list"
    );

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
}

int main()
{
    testModeQueryDefaultAndPrivileges();
    testModeQueryReportsActiveFlags();

    return finishSuite("phase 17 MODE query");
}
