#include "ProtocolTestHarness.hpp"

/**
 * @brief Phase 17 — MODE before registration returns 451, and a missing or
 * empty channel name returns 461.
 */
static void testModeRegistrationAndParameterErrors()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 17 MODE parameter error tests"
        ))
    {
        return;
    }

    expectEqual(
        sendCommandAndReceive(server.getPort(), "MODE #general\r\n"),
        ":irc.42.local 451 * :You have not registered\r\n",
        "Phase 17: MODE before registration should return 451"
    );

    const int socketFd = connectToServer(server.getPort());

    if (socketFd == -1)
    {
        reportFailure(
            "Client should connect for phase 17 MODE parameter error tests",
            "successful connection",
            "connection failed"
        );
        return;
    }

    registerClient(socketFd, "operador");

    expectEqual(
        sendLineAndReceive(socketFd, "MODE\r\n"),
        ":irc.42.local 461 operador MODE :Not enough parameters\r\n",
        "Phase 17: MODE without a channel should return 461"
    );

    expectEqual(
        sendLineAndReceive(socketFd, "MODE :\r\n"),
        ":irc.42.local 461 operador MODE :Not enough parameters\r\n",
        "Phase 17: MODE with an empty channel parameter should return 461"
    );

    expectTrue(
        server.isRunning(),
        "Phase 17: invalid MODE commands should not stop the server",
        "ircserv remains running",
        "ircserv exited"
    );

    closeSocket(socketFd);
}

/**
 * @brief Phase 17 — modification errors follow the documented order: unknown
 * channel, sender not on channel, sender not an operator, unknown mode letter.
 */
static void testModeTargetAndPrivilegeErrors()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 17 MODE privilege error tests"
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
            "Clients should connect for phase 17 MODE privilege error tests",
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

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #desconocido +i\r\n"),
        ":irc.42.local 403 operador #desconocido :No such channel\r\n",
        "Phase 17: MODE on an unknown channel should return 403"
    );

    sendAll(operatorSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(memberSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    expectEqual(
        sendLineAndReceive(outsiderSocketFd, "MODE #general +i\r\n"),
        ":irc.42.local 442 outsider #general :You're not on that channel\r\n",
        "Phase 17: MODE from outside the channel should return 442"
    );

    expectEqual(
        sendLineAndReceive(memberSocketFd, "MODE #general +i\r\n"),
        ":irc.42.local 482 roxana #general :You're not channel operator\r\n",
        "Phase 17: MODE from a regular member should return 482"
    );

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #general +x\r\n"),
        ":irc.42.local 472 operador x :is unknown mode char to me\r\n",
        "Phase 17: an unknown mode letter should return 472"
    );

    expectEqual(
        receiveAvailableData(memberSocketFd, 200),
        "",
        "Phase 17: a rejected MODE should not notify channel members"
    );

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #general\r\n"),
        ":irc.42.local 324 operador #general +\r\n",
        "Phase 17: a rejected unknown mode should not change the channel"
    );

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
    closeSocket(outsiderSocketFd);
}

/**
 * @brief Phase 17 — +o and -o require a nickname that exists and belongs to
 * the channel. Missing nicknames return 461.
 */
static void testModeOperatorArgumentErrors()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 17 MODE +o error tests"
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
            "Clients should connect for phase 17 MODE +o error tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        closeSocket(guestSocketFd);
        return;
    }

    registerClient(operatorSocketFd, "operador");
    registerClient(memberSocketFd, "roxana");
    registerClient(guestSocketFd, "invitado");

    sendAll(operatorSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(memberSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #general +o\r\n"),
        ":irc.42.local 461 operador MODE :Not enough parameters\r\n",
        "Phase 17: +o without a nickname should return 461"
    );

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #general -o\r\n"),
        ":irc.42.local 461 operador MODE :Not enough parameters\r\n",
        "Phase 17: -o without a nickname should return 461"
    );

    expectEqual(
        sendLineAndReceive(
            operatorSocketFd,
            "MODE #general +o desconocido\r\n"
        ),
        ":irc.42.local 401 operador desconocido :No such nick\r\n",
        "Phase 17: +o of an unknown nickname should return 401"
    );

    expectEqual(
        sendLineAndReceive(
            operatorSocketFd,
            "MODE #general +o invitado\r\n"
        ),
        ":irc.42.local 441 operador invitado #general :They aren't on that channel\r\n",
        "Phase 17: +o of a user who is not on the channel should return 441"
    );

    expectEqual(
        sendLineAndReceive(
            operatorSocketFd,
            "MODE #general -o invitado\r\n"
        ),
        ":irc.42.local 441 operador invitado #general :They aren't on that channel\r\n",
        "Phase 17: -o of a user who is not on the channel should return 441"
    );

    expectEqual(
        receiveAvailableData(memberSocketFd, 200),
        "",
        "Phase 17: a rejected +o should not notify channel members"
    );

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
    closeSocket(guestSocketFd);
}

/**
 * @brief Phase 17 — +k and +l require their arguments, invalid limits are
 * rejected, and a combination missing a later argument does not apply earlier
 * flags.
 */
static void testModeKeyLimitAndPartialCommandErrors()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 17 MODE argument error tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1 || memberSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 17 MODE argument error tests",
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

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #general +k\r\n"),
        ":irc.42.local 461 operador MODE :Not enough parameters\r\n",
        "Phase 17: +k without a key should return 461"
    );

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #general +l\r\n"),
        ":irc.42.local 461 operador MODE :Not enough parameters\r\n",
        "Phase 17: +l without a limit should return 461"
    );

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #general +l 0\r\n"),
        ":irc.42.local 461 operador MODE :Not enough parameters\r\n",
        "Phase 17: +l 0 should be rejected as an invalid limit"
    );

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #general +l -5\r\n"),
        ":irc.42.local 461 operador MODE :Not enough parameters\r\n",
        "Phase 17: +l with a negative value should be rejected"
    );

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #general +l texto\r\n"),
        ":irc.42.local 461 operador MODE :Not enough parameters\r\n",
        "Phase 17: +l with a non-numeric value should be rejected"
    );

    expectEqual(
        sendLineAndReceive(
            operatorSocketFd,
            "MODE #general +l 99999999999999999999999999\r\n"
        ),
        ":irc.42.local 461 operador MODE :Not enough parameters\r\n",
        "Phase 17: +l with an overflowing value should be rejected"
    );

    expectEqual(
        sendLineAndReceive(
            operatorSocketFd,
            "MODE #general +kl secret\r\n"
        ),
        ":irc.42.local 461 operador MODE :Not enough parameters\r\n",
        "Phase 17: +kl without a limit should return 461"
    );

    expectEqual(
        sendLineAndReceive(
            operatorSocketFd,
            "MODE #general +ix\r\n"
        ),
        ":irc.42.local 472 operador x :is unknown mode char to me\r\n",
        "Phase 17: an unknown letter in a combination should return 472"
    );

    expectEqual(
        sendLineAndReceive(
            operatorSocketFd,
            "MODE #general +io desconocido\r\n"
        ),
        ":irc.42.local 401 operador desconocido :No such nick\r\n",
        "Phase 17: a later +o error should reject the whole MODE command"
    );

    expectEqual(
        receiveAvailableData(memberSocketFd, 200),
        "",
        "Phase 17: a rejected combination should not notify channel members"
    );

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #general\r\n"),
        ":irc.42.local 324 operador #general +\r\n",
        "Phase 17: a rejected combination should not apply any mode change"
    );

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
}

int main()
{
    testModeRegistrationAndParameterErrors();
    testModeTargetAndPrivilegeErrors();
    testModeOperatorArgumentErrors();
    testModeKeyLimitAndPartialCommandErrors();

    return finishSuite("phase 17 MODE errors");
}
