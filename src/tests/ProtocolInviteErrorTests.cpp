// Copyright 2026 @esettes, @danielfdez17
#include "ProtocolTestHarness.hpp"

/**
 * @brief Phase 15 — INVITE error paths that do not require an existing
 * channel: 451 before registration and 461 when nickname or channel is
 * missing.
 */
static void testInviteRegistrationAndParameterErrors() {
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 15 INVITE parameter error tests")) {
        return;
    }

    expectEqual(
        sendCommandAndReceive(
            server.getPort(),
            "INVITE roxana #privado\r\n"),
        ":irc.42.local 451 * :You have not registered\r\n",
        "Phase 15: INVITE before registration should return 451");

    const int socketFd = connectToServer(server.getPort());

    if (socketFd == -1) {
        reportFailure(
            "Client should connect for phase 15 INVITE parameter error tests",
            "successful connection",
            "connection failed");
        return;
    }

    registerClient(socketFd, "operador");

    expectEqual(
        sendLineAndReceive(socketFd, "INVITE\r\n"),
        ":irc.42.local 461 operador INVITE :Not enough parameters\r\n",
        "Phase 15: INVITE without parameters should return 461");

    expectEqual(
        sendLineAndReceive(socketFd, "INVITE roxana\r\n"),
        ":irc.42.local 461 operador INVITE :Not enough parameters\r\n",
        "Phase 15: INVITE without a channel should return 461");

    expectTrue(
        server.isRunning(),
        "Phase 15: invalid INVITE commands should not stop the server",
        "ircserv remains running",
        "ircserv exited");

    closeSocket(socketFd);
}

/**
 * @brief Phase 15 — INVITE validation order: unknown channel, unknown nick,
 * sender not on channel, target already a member, and sender not an operator.
 */
static void testInviteTargetAndPrivilegeErrors() {
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 15 INVITE target error tests")) {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());
    const int outsiderSocketFd = connectToServer(server.getPort());
    const int guestSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1
        || memberSocketFd == -1
        || outsiderSocketFd == -1
        || guestSocketFd == -1) {
        reportFailure(
            "Clients should connect for phase 15 INVITE target error tests",
            "successful connections",
            "connection failed");
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
            "INVITE roxana #desconocido\r\n"),
        ":irc.42.local 403 operador #desconocido :No such channel\r\n",
        "Phase 15: INVITE to an unknown channel should return 403");

    sendAll(operatorSocketFd, "JOIN #privado\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(memberSocketFd, "JOIN #privado\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    expectEqual(
        sendLineAndReceive(
            operatorSocketFd,
            "INVITE desconocido #privado\r\n"),
        ":irc.42.local 401 operador desconocido :No such nick\r\n",
        "Phase 15: INVITE to an unknown nickname should return 401");

    expectEqual(
        sendLineAndReceive(
            outsiderSocketFd,
            "INVITE invitado #privado\r\n"),
        ":irc.42.local 442 outsider #privado :You're not on that channel\r\n",
        "Phase 15: INVITE from outside the channel should return 442");

    expectEqual(
        sendLineAndReceive(
            operatorSocketFd,
            "INVITE roxana #privado\r\n"),
        ":irc.42.local 443 operador roxana #privado :is already on channel\r\n",
        "Phase 15: INVITE of a current member should return 443");

    expectEqual(
        sendLineAndReceive(
            memberSocketFd,
            "INVITE invitado #privado\r\n"),
        ":irc.42.local 482 roxana #privado :You're not channel operator\r\n",
        "Phase 15: INVITE from a regular member should return 482");

    expectEqual(
        receiveAvailableData(guestSocketFd, 200),
        "",
        "Phase 15: a rejected INVITE should not notify the target");

    sendAll(operatorSocketFd, "MODE #privado +i\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    expectEqual(
        sendLineAndReceive(guestSocketFd, "JOIN #privado\r\n"),
        ":irc.42.local 473 invitado #privado :Cannot join channel (+i)\r\n",
        "Phase 15: a rejected INVITE must not store an invitation");

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
    closeSocket(outsiderSocketFd);
    closeSocket(guestSocketFd);
}

int main() {
    testInviteRegistrationAndParameterErrors();
    testInviteTargetAndPrivilegeErrors();

    return finishSuite("phase 15 INVITE error");
}
