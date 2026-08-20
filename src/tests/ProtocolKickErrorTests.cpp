#include "ProtocolTestHarness.hpp"

/**
 * @brief Phase 16 — KICK error paths that do not require an existing channel:
 * 451 before registration and 461 when the channel or target nickname is
 * missing.
 */
static void testKickRegistrationAndParameterErrors()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 16 KICK parameter error tests"
        ))
    {
        return;
    }

    expectEqual(
        sendCommandAndReceive(server.getPort(), "KICK #general roxana\r\n"),
        ":irc.42.local 451 * :You have not registered\r\n",
        "Phase 16: KICK before registration should return 451"
    );

    const int socketFd = connectToServer(server.getPort());

    if (socketFd == -1)
    {
        reportFailure(
            "Client should connect for phase 16 KICK parameter error tests",
            "successful connection",
            "connection failed"
        );
        return;
    }

    registerClient(socketFd, "operador");

    expectEqual(
        sendLineAndReceive(socketFd, "KICK\r\n"),
        ":irc.42.local 461 operador KICK :Not enough parameters\r\n",
        "Phase 16: KICK without parameters should return 461"
    );

    expectEqual(
        sendLineAndReceive(socketFd, "KICK #general\r\n"),
        ":irc.42.local 461 operador KICK :Not enough parameters\r\n",
        "Phase 16: KICK without a target nickname should return 461"
    );

    expectEqual(
        sendLineAndReceive(socketFd, "KICK :\r\n"),
        ":irc.42.local 461 operador KICK :Not enough parameters\r\n",
        "Phase 16: KICK with an empty channel parameter should return 461"
    );

    expectEqual(
        sendLineAndReceive(socketFd, "KICK #general :\r\n"),
        ":irc.42.local 461 operador KICK :Not enough parameters\r\n",
        "Phase 16: KICK with an empty target nickname should return 461"
    );

    expectTrue(
        server.isRunning(),
        "Phase 16: invalid KICK commands should not stop the server",
        "ircserv remains running",
        "ircserv exited"
    );

    closeSocket(socketFd);
}

/**
 * @brief Phase 16 — KICK validation order: unknown channel, unknown nick,
 * sender not on channel, sender not an operator, and target not a member.
 */
static void testKickTargetAndPrivilegeErrors()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 16 KICK target error tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());
    const int outsiderSocketFd = connectToServer(server.getPort());
    const int guestSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1
        || memberSocketFd == -1
        || outsiderSocketFd == -1
        || guestSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 16 KICK target error tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        closeSocket(outsiderSocketFd);
        closeSocket(guestSocketFd);
        return;
    }

    registerClient(operatorSocketFd, "operador");
    registerClient(memberSocketFd, "roxana");
    registerClient(outsiderSocketFd, "outsider");
    registerClient(guestSocketFd, "invitado");

    expectEqual(
        sendLineAndReceive(
            operatorSocketFd,
            "KICK #desconocido roxana\r\n"
        ),
        ":irc.42.local 403 operador #desconocido :No such channel\r\n",
        "Phase 16: KICK from an unknown channel should return 403"
    );

    sendAll(operatorSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(memberSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    expectEqual(
        sendLineAndReceive(
            operatorSocketFd,
            "KICK #general desconocido\r\n"
        ),
        ":irc.42.local 401 operador desconocido :No such nick\r\n",
        "Phase 16: KICK of an unknown nickname should return 401"
    );

    expectEqual(
        sendLineAndReceive(
            outsiderSocketFd,
            "KICK #general roxana\r\n"
        ),
        ":irc.42.local 442 outsider #general :You're not on that channel\r\n",
        "Phase 16: KICK from outside the channel should return 442"
    );

    expectEqual(
        sendLineAndReceive(
            memberSocketFd,
            "KICK #general operador\r\n"
        ),
        ":irc.42.local 482 roxana #general :You're not channel operator\r\n",
        "Phase 16: KICK from a regular member should return 482"
    );

    expectEqual(
        sendLineAndReceive(
            operatorSocketFd,
            "KICK #general invitado\r\n"
        ),
        ":irc.42.local 441 operador invitado #general :They aren't on that channel\r\n",
        "Phase 16: KICK of a user who is not on the channel should return 441"
    );

    expectEqual(
        receiveAvailableData(memberSocketFd, 200),
        "",
        "Phase 16: a rejected KICK should not notify channel members"
    );

    expectEqual(
        receiveAvailableData(guestSocketFd, 200),
        "",
        "Phase 16: a rejected KICK should not notify the intended target"
    );

    sendAll(operatorSocketFd, "PRIVMSG #general :sigue dentro\r\n");

    expectContains(
        receiveAvailableData(memberSocketFd, 500),
        " PRIVMSG #general :sigue dentro\r\n",
        "Phase 16: a rejected KICK must not remove the target from the channel"
    );

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
    closeSocket(outsiderSocketFd);
    closeSocket(guestSocketFd);
}

int main()
{
    testKickRegistrationAndParameterErrors();
    testKickTargetAndPrivilegeErrors();

    return finishSuite("phase 16 KICK error");
}
