// Copyright 2026 @esettes, @danielfdez17
#include "ProtocolTestHarness.hpp"

/**
 * @brief Phase 15 — a valid INVITE confirms with 341 RPL_INVITING to the
 * sender, delivers INVITE with the sender prefix only to the target, and
 * does not notify other channel members.
 */
static void testValidInviteDelivery() {
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 15 INVITE delivery tests")) {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());
    const int guestSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1
        || memberSocketFd == -1
        || guestSocketFd == -1) {
        reportFailure(
            "Clients should connect for phase 15 INVITE delivery tests",
            "successful connections",
            "connection failed");
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
            operatorPrefix)) {
        reportFailure(
            "Welcome should expose a prefix for phase 15 INVITE delivery tests",
            "welcome containing operador!operador@host",
            operatorWelcome);
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        closeSocket(guestSocketFd);
        return;
    }

    sendAll(operatorSocketFd, "JOIN #privado\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(memberSocketFd, "JOIN #privado\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    sendAll(operatorSocketFd, "INVITE roxana #privado\r\n");

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        ":irc.42.local 341 operador roxana #privado\r\n",
        "Phase 15: the inviter should receive 341 RPL_INVITING");

    expectEqual(
        receiveAvailableData(guestSocketFd, 500),
        ":" + operatorPrefix + " INVITE roxana :#privado\r\n",
        "Phase 15: the target should receive INVITE with the sender prefix");

    expectEqual(
        receiveAvailableData(memberSocketFd, 200),
        "",
        "Phase 15: INVITE should not be broadcast to other channel members");

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
    closeSocket(guestSocketFd);
}

/**
 * @brief Phase 15 — INVITE looks up nicknames and channels with RFC 1459
 * casemapping and stores a single invitation even when the command is
 * repeated.
 */
static void testInviteCasemappingAndDuplicates() {
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 15 INVITE casemap tests")) {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int guestSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1 || guestSocketFd == -1) {
        reportFailure(
            "Clients should connect for phase 15 INVITE casemap tests",
            "successful connections",
            "connection failed");
        closeSocket(operatorSocketFd);
        closeSocket(guestSocketFd);
        return;
    }

    const std::string operatorWelcome =
        registerClient(operatorSocketFd, "operador");
    registerClient(guestSocketFd, "roxana");

    std::string operatorPrefix;

    if (!extractClientPrefixFromWelcome(
            operatorWelcome,
            "operador",
            operatorPrefix)) {
        reportFailure(
            "Welcome should expose a prefix for phase 15 INVITE casemap tests",
            "welcome containing operador!operador@host",
            operatorWelcome);
        closeSocket(operatorSocketFd);
        closeSocket(guestSocketFd);
        return;
    }

    sendAll(operatorSocketFd, "JOIN #Privado\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(operatorSocketFd, "INVITE ROXANA #privado\r\n");

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        ":irc.42.local 341 operador roxana #Privado\r\n",
        "Phase 15: INVITE should resolve"
        "nicknames and channels with casemapping");

    expectEqual(
        receiveAvailableData(guestSocketFd, 500),
        ":" + operatorPrefix + " INVITE roxana :#Privado\r\n",
        "Phase 15: INVITE should notify"
        "the target using the stored channel name");

    sendAll(operatorSocketFd, "INVITE roxana #PRIVADO\r\n");

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        ":irc.42.local 341 operador roxana #Privado\r\n",
        "Phase 15: a repeated INVITE should still confirm with 341");

    expectEqual(
        receiveAvailableData(guestSocketFd, 500),
        ":" + operatorPrefix + " INVITE roxana :#Privado\r\n",
        "Phase 15: a repeated INVITE should still notify the target");

    closeSocket(operatorSocketFd);
    closeSocket(guestSocketFd);
}

int main() {
    testValidInviteDelivery();
    testInviteCasemappingAndDuplicates();

    return finishSuite("phase 15 INVITE command");
}
