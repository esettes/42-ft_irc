// Copyright 2026 @esettes, @danielfdez17
#include "Client.hpp"
#include "IrcMessage.hpp"
#include "ProtocolTestHarness.hpp"

/**
 * @brief Phase 19 — a complete line longer than 512 bytes, including the
 * terminator, is rejected at the framing layer.
 */
static void testClientRejectsOversizedCompleteLine() {
    Client client(-1, "localhost");
    std::string completeLine;

    client.appendToInputBuffer(
        "PASS " + std::string(506, 'A') + "\r\n");

    expectTrue(
        client.extractNextLine(completeLine) == Client::LINE_TOO_LONG,
        "Phase 19: a CRLF line longer than 512 bytes should be rejected",
        "LINE_TOO_LONG",
        "status different from LINE_TOO_LONG");
    expectEqual(
        client.getInputBuffer(),
        "",
        "Phase 19: an oversized complete line should be discarded");
}

/**
 * @brief Phase 19 — unterminated input that can never fit in 512 bytes is
 * rejected instead of growing without a bound.
 */
static void testClientRejectsUnterminatedOversizedInput() {
    Client client(-1, "localhost");
    std::string completeLine;

    client.appendToInputBuffer(std::string(IRC_MAX_MESSAGE_LENGTH, 'A'));

    expectTrue(
        client.extractNextLine(completeLine) == Client::LINE_TOO_LONG,
        "Phase 19: unterminated input of 512 bytes should be rejected",
        "LINE_TOO_LONG",
        "status different from LINE_TOO_LONG");
    expectEqual(
        client.getInputBuffer(),
        "",
        "Phase 19: oversized unterminated input should be discarded");
}

/**
 * @brief Phase 19 — sending a line over 512 bytes disconnects that client
 * without stopping the server or processing a later command in the same burst.
 */
static void testOversizedLineDisconnectsClient() {
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 19 oversized-line tests")) {
        return;
    }

    const int abusiveSocketFd = connectToServer(server.getPort());

    if (abusiveSocketFd == -1) {
        reportFailure(
            "Client should connect for phase 19 oversized-line tests",
            "successful connection",
            "connection failed");
        return;
    }

    registerClient(abusiveSocketFd, "abusive");

    const std::string oversizedCommand =
        "PING :" + std::string(600, 'A') + "\r\n"
        "PING :should-not-run\r\n";

    sendAll(abusiveSocketFd, oversizedCommand);

    expectTrue(
        waitForSocketClosure(abusiveSocketFd, 1000),
        "Phase 19: an oversized IRC line should disconnect the sender",
        "closed TCP connection",
        "connection remained open");

    closeSocket(abusiveSocketFd);

    expectEqual(
        sendCommandAndReceive(server.getPort(), "PING :still-alive\r\n"),
        "PONG :still-alive\r\n",
        "Phase 19: the server should"
        "keep serving clients after an oversized line");
}

/**
 * @brief Phase 19 — a client that never sends a terminator is disconnected
 * once the input exceeds the per-connection buffer limit.
 */
static void testUnterminatedFloodDisconnectsClient() {
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 19 unterminated-flood tests")) {
        return;
    }

    const int floodSocketFd = connectToServer(server.getPort());

    if (floodSocketFd == -1) {
        reportFailure(
            "Client should connect for phase 19 unterminated-flood tests",
            "successful connection",
            "connection failed");
        return;
    }

    const std::string flood(IRC_MAX_INPUT_BUFFER_SIZE + 64, 'A');

    sendAll(floodSocketFd, flood);

    expectTrue(
        waitForSocketClosure(floodSocketFd, 1000),
        "Phase 19: unterminated input past"
        "the buffer limit should disconnect the client",
        "closed TCP connection",
        "connection remained open");

    closeSocket(floodSocketFd);

    expectTrue(
        server.isRunning(),
        "Phase 19: an unterminated flood should not stop the server",
        "ircserv remains running",
        "ircserv exited");

    expectEqual(
        sendCommandAndReceive(server.getPort(), "UNKNOWN\r\n"),
        ":irc.42.local 421 * UNKNOWN :Unknown command\r\n",
        "Phase 19: a new client should be served after an unterminated flood");
}

int main() {
    testClientRejectsOversizedCompleteLine();
    testClientRejectsUnterminatedOversizedInput();
    testOversizedLineDisconnectsClient();
    testUnterminatedFloodDisconnectsClient();

    return finishSuite("phase 19 oversized input");
}
