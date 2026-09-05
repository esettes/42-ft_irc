// Copyright 2026 @esettes, @danielfdez17
#include "ProtocolTestHarness.hpp"

/**
 * @brief Phase 17 — +o grants channel operator privileges to a member, those
 * privileges use the same operator collection as KICK, and the user remains
 * in the channel. The MODE change is broadcast to every member.
 */
static void testGrantOperatorPrivileges() {
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 17 MODE +o tests")) {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());
    const int targetSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1
        || memberSocketFd == -1
        || targetSocketFd == -1) {
        reportFailure(
            "Clients should connect for phase 17 MODE +o tests",
            "successful connections",
            "connection failed");
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        closeSocket(targetSocketFd);
        return;
    }

    const std::string operatorWelcome =
        registerClient(operatorSocketFd, "operador");
    registerClient(memberSocketFd, "roxana");
    registerClient(targetSocketFd, "alice");

    std::string operatorPrefix;

    if (!extractClientPrefixFromWelcome(
            operatorWelcome,
            "operador",
            operatorPrefix)) {
        reportFailure(
            "Welcome should expose a prefix for phase 17 MODE +o tests",
            "welcome containing operador!operador@host",
            operatorWelcome);
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        closeSocket(targetSocketFd);
        return;
    }

    sendAll(operatorSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(memberSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    sendAll(targetSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);
    discardPendingData(targetSocketFd);

    sendAll(operatorSocketFd, "MODE #general +o ROXANA\r\n");

    const std::string expectedGrant =
        ":" + operatorPrefix + " MODE #general +o roxana\r\n";

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        expectedGrant,
        "Phase 17: +o should notify the operator using the stored nickname");
    expectEqual(
        receiveAvailableData(memberSocketFd, 500),
        expectedGrant,
        "Phase 17: +o should notify the promoted member");
    expectEqual(
        receiveAvailableData(targetSocketFd, 500),
        expectedGrant,
        "Phase 17: +o should notify every other channel member");

    sendAll(memberSocketFd, "PRIVMSG #general :sigo dentro\r\n");

    expectContains(
        receiveAvailableData(targetSocketFd, 500),
        " PRIVMSG #general :sigo dentro\r\n",
        "Phase 17: +o should not remove the promoted user from the channel");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    sendAll(memberSocketFd, "KICK #general alice :ahora puedo\r\n");

    expectContains(
        receiveAvailableData(memberSocketFd, 500),
        " KICK #general alice :ahora puedo\r\n",
        "Phase 17: a member promoted with +o should be able to KICK");

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
    closeSocket(targetSocketFd);
}

/**
 * @brief Phase 17 — -o revokes operator privileges without removing membership.
 * The former operator can no longer KICK, and the MODE change is broadcast.
 */
static void testRevokeOperatorPrivileges() {
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 17 MODE -o tests")) {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());
    const int otherSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1
        || memberSocketFd == -1
        || otherSocketFd == -1) {
        reportFailure(
            "Clients should connect for phase 17 MODE -o tests",
            "successful connections",
            "connection failed");
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        closeSocket(otherSocketFd);
        return;
    }

    const std::string operatorWelcome =
        registerClient(operatorSocketFd, "operador");
    registerClient(memberSocketFd, "roxana");
    registerClient(otherSocketFd, "alice");

    std::string operatorPrefix;

    if (!extractClientPrefixFromWelcome(
            operatorWelcome,
            "operador",
            operatorPrefix)) {
        reportFailure(
            "Welcome should expose a prefix for phase 17 MODE -o tests",
            "welcome containing operador!operador@host",
            operatorWelcome);
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        closeSocket(otherSocketFd);
        return;
    }

    sendAll(operatorSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(memberSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    sendAll(otherSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);
    discardPendingData(otherSocketFd);

    sendAll(operatorSocketFd, "MODE #general +o roxana\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);
    discardPendingData(otherSocketFd);

    sendAll(operatorSocketFd, "MODE #general -o roxana\r\n");

    const std::string expectedRevoke =
        ":" + operatorPrefix + " MODE #general -o roxana\r\n";

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        expectedRevoke,
        "Phase 17: -o should notify the remaining operator");
    expectEqual(
        receiveAvailableData(memberSocketFd, 500),
        expectedRevoke,
        "Phase 17: -o should notify the demoted member");
    expectEqual(
        receiveAvailableData(otherSocketFd, 500),
        expectedRevoke,
        "Phase 17: -o should notify every other channel member");

    expectEqual(
        sendLineAndReceive(memberSocketFd, "KICK #general alice\r\n"),
        ":irc.42.local 482 roxana #general :You're not channel operator\r\n",
        "Phase 17: a demoted member should no longer be able to KICK");

    sendAll(memberSocketFd, "PRIVMSG #general :sigo dentro\r\n");

    expectContains(
        receiveAvailableData(otherSocketFd, 500),
        " PRIVMSG #general :sigo dentro\r\n",
        "Phase 17: -o should leave the demoted user in the channel");

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
    closeSocket(otherSocketFd);
}

int main() {
    testGrantOperatorPrivileges();
    testRevokeOperatorPrivileges();

    return finishSuite("phase 17 MODE operator");
}
