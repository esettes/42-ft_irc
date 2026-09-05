// Copyright 2026 @esettes, @danielfdez17
#include <sstream>

#include "ProtocolTestHarness.hpp"

namespace {
std::string formatMessageIndex(int index) {
    std::ostringstream stream;

    stream << "msg-";
    if (index < 10)
        stream << '0';
    stream << index;
    return stream.str();
}
}  // namespace

/**
 * @brief Phase 19 — queued channel traffic is delivered in order without
 * lost or duplicated payloads when the recipient drains the socket slowly.
 */
static void testSlowDrainPreservesMessageOrder() {
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 19 partial-write tests")) {
        return;
    }

    const int senderSocketFd = connectToServer(server.getPort());
    const int slowSocketFd = connectToServer(server.getPort());

    if (senderSocketFd == -1 || slowSocketFd == -1) {
        reportFailure(
            "Clients should connect for phase 19 partial-write tests",
            "successful connections",
            "connection failed");
        closeSocket(senderSocketFd);
        closeSocket(slowSocketFd);
        return;
    }

    const std::string senderWelcome =
        registerClient(senderSocketFd, "alice");
    registerClient(slowSocketFd, "bob");

    std::string senderPrefix;

    if (!extractClientPrefixFromWelcome(
            senderWelcome,
            "alice",
            senderPrefix)) {
        reportFailure(
            "Welcome should expose a prefix for phase 19 partial-write tests",
            "welcome containing alice!alice@host",
            senderWelcome);
        closeSocket(senderSocketFd);
        closeSocket(slowSocketFd);
        return;
    }

    sendAll(senderSocketFd, "JOIN #burst\r\n");
    discardPendingData(senderSocketFd);

    sendAll(slowSocketFd, "JOIN #burst\r\n");
    discardPendingData(senderSocketFd);
    discardPendingData(slowSocketFd);

    int receiveBufferSize = 256;

    ::setsockopt(
        slowSocketFd,
        SOL_SOCKET,
        SO_RCVBUF,
        &receiveBufferSize,
        sizeof(receiveBufferSize));

    const int messageCount = 80;
    std::string burst;

    for (int index = 0; index < messageCount; ++index) {
        burst += "PRIVMSG #burst :" + formatMessageIndex(index) + "\r\n";
    }

    if (!sendAll(senderSocketFd, burst)) {
        reportFailure(
            "Burst PRIVMSG commands should be sent",
            "successful send",
            "send failed");
        closeSocket(senderSocketFd);
        closeSocket(slowSocketFd);
        return;
    }

    ::usleep(200000);

    const std::string drainedOutput =
        receiveAvailableData(slowSocketFd, 2000);

    expectTrue(
        everyLineUsesCrlf(drainedOutput),
        "Phase 19: drained output after a slow receive window should use CRLF",
        "every line ending with \\r\\n",
        drainedOutput);

    for (int index = 0; index < messageCount; ++index) {
        const std::string expectedPayload =
            " PRIVMSG #burst :" + formatMessageIndex(index) + "\r\n";

        expectTrue(
            countOccurrences(drainedOutput, expectedPayload) == 1,
            "Phase 19: each burst payload should arrive once: "
                + formatMessageIndex(index),
            "exactly one occurrence",
            drainedOutput);
    }

    expectEqual(
        receiveAvailableData(senderSocketFd, 200),
        "",
        "Phase 19: channel PRIVMSG should"
        "not echo to the sender during a burst");

    expectTrue(
        server.isRunning(),
        "Phase 19: a slow recipient should not stop the server",
        "ircserv remains running",
        "ircserv exited");

    closeSocket(senderSocketFd);
    closeSocket(slowSocketFd);
}

/**
 * @brief Phase 19 — closing a client that still has pending output must not
 * stop the server or break an unrelated connection.
 */
static void testDisconnectWithPendingOutputKeepsServerAlive() {
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for"
            "phase 19 pending-output disconnect tests")) {
        return;
    }

    const int senderSocketFd = connectToServer(server.getPort());
    const int pendingSocketFd = connectToServer(server.getPort());

    if (senderSocketFd == -1 || pendingSocketFd == -1) {
        reportFailure(
            "Clients should connect for"
            "phase 19 pending-output disconnect tests",
            "successful connections",
            "connection failed");
        closeSocket(senderSocketFd);
        closeSocket(pendingSocketFd);
        return;
    }

    registerClient(senderSocketFd, "alice");
    registerClient(pendingSocketFd, "bob");

    sendAll(senderSocketFd, "JOIN #pending\r\n");
    discardPendingData(senderSocketFd);

    sendAll(pendingSocketFd, "JOIN #pending\r\n");
    discardPendingData(senderSocketFd);
    discardPendingData(pendingSocketFd);

    int receiveBufferSize = 256;

    ::setsockopt(
        pendingSocketFd,
        SOL_SOCKET,
        SO_RCVBUF,
        &receiveBufferSize,
        sizeof(receiveBufferSize));

    std::string burst;

    for (int index = 0; index < 120; ++index)
        burst += "PRIVMSG #pending :queued-output\r\n";

    sendAll(senderSocketFd, burst);
    ::usleep(100000);
    closeSocket(pendingSocketFd);
    ::usleep(100000);
    discardPendingData(senderSocketFd);

    expectEqual(
        sendLineAndReceive(senderSocketFd, "PING :after-pending-close\r\n"),
        "PONG :after-pending-close\r\n",
        "Phase 19: the remaining client should"
        "stay usable after a pending-output disconnect");

    expectEqual(
        sendCommandAndReceive(server.getPort(), "UNKNOWN\r\n"),
        ":irc.42.local 421 * UNKNOWN :Unknown command\r\n",
        "Phase 19: a new client should"
        "be served after a pending-output disconnect");

    expectTrue(
        server.isRunning(),
        "Phase 19: disconnecting a client"
        "with pending output should not stop the server",
        "ircserv remains running",
        "ircserv exited");

    closeSocket(senderSocketFd);
}

int main() {
    testSlowDrainPreservesMessageOrder();
    testDisconnectWithPendingOutputKeepsServerAlive();

    return finishSuite("phase 19 partial writes");
}
