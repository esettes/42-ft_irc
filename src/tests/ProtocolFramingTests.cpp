// Copyright 2026 @esettes, @danielfdez17

#include <vector>

#include "Client.hpp"
#include "ProtocolTestHarness.hpp"

/**
 * @brief Phase 19 — incomplete TCP fragments stay in the input buffer and are
 * only returned once a terminator arrives. CR before LF is stripped.
 */
static void testClientReassemblesFragmentedLines() {
    Client client(-1, "localhost");
    std::string completeLine;

    client.appendToInputBuffer("PRIV");
    expectTrue(
        client.extractNextLine(completeLine) == Client::LINE_INCOMPLETE,
        "Phase 19: a command prefix"
        "without a terminator should stay incomplete",
        "LINE_INCOMPLETE",
        "status different from LINE_INCOMPLETE");
    expectEqual(
        client.getInputBuffer(),
        "PRIV",
        "Phase 19: an incomplete fragment should remain in the input buffer");

    client.appendToInputBuffer("MSG #general :ho");
    expectTrue(
        client.extractNextLine(completeLine) == Client::LINE_INCOMPLETE,
        "Phase 19: additional bytes without"
        "a terminator should stay incomplete",
        "LINE_INCOMPLETE",
        "status different from LINE_INCOMPLETE");

    client.appendToInputBuffer("la\r");
    expectTrue(
        client.extractNextLine(completeLine) == Client::LINE_INCOMPLETE,
        "Phase 19: a bare CR should not complete an IRC line",
        "LINE_INCOMPLETE",
        "status different from LINE_INCOMPLETE");

    client.appendToInputBuffer("\n");
    expectTrue(
        client.extractNextLine(completeLine) == Client::LINE_COMPLETE,
        "Phase 19: LF should complete a line assembled from TCP fragments",
        "LINE_COMPLETE",
        "status different from LINE_COMPLETE");
    expectEqual(
        completeLine,
        "PRIVMSG #general :hola",
        "Phase 19: reconstructed fragments should produce a single command");
    expectEqual(
        client.getInputBuffer(),
        "",
        "Phase 19: a completed line should be removed from the input buffer");
}

/**
 * @brief Phase 19 — one recv payload may contain several complete commands
 * plus a trailing incomplete fragment.
 */
static void testClientSplitsBatchedCommands() {
    Client client(-1, "localhost");
    std::string completeLine;
    std::vector<std::string> extractedLines;

    client.appendToInputBuffer(
        "NICK one\r\n"
        "USER one 0 * :One\r\n"
        "JOIN #a\r\n"
        "PRIV");

    while (client.extractNextLine(completeLine) == Client::LINE_COMPLETE)
        extractedLines.push_back(completeLine);

    expectTrue(
        extractedLines.size() == 3,
        "Phase 19: batched input should yield one complete line per terminator",
        "3 complete lines",
        "a different number of complete lines");

    if (extractedLines.size() == 3) {
        expectEqual(
            extractedLines[0],
            "NICK one",
            "Phase 19: the first batched command should be NICK");
        expectEqual(
            extractedLines[1],
            "USER one 0 * :One",
            "Phase 19: the second batched command should be USER");
        expectEqual(
            extractedLines[2],
            "JOIN #a",
            "Phase 19: the third batched command should be JOIN");
    }

    expectEqual(
        client.getInputBuffer(),
        "PRIV",
        "Phase 19: an incomplete trailing fragment should remain buffered");
}

/**
 * @brief Phase 19 — LF-only and CRLF terminators are both accepted, and a
 * lone CR is not treated as a line ending.
 */
static void testClientAcceptsLfAndCrlf() {
    Client client(-1, "localhost");
    std::string completeLine;

    client.appendToInputBuffer("PING :lf\n");
    expectTrue(
        client.extractNextLine(completeLine) == Client::LINE_COMPLETE,
        "Phase 19: a line terminated only with LF should be complete",
        "LINE_COMPLETE",
        "status different from LINE_COMPLETE");
    expectEqual(
        completeLine,
        "PING :lf",
        "Phase 19: LF should be stripped from a complete line");

    client.appendToInputBuffer("PING :crlf\r\n");
    expectTrue(
        client.extractNextLine(completeLine) == Client::LINE_COMPLETE,
        "Phase 19: a line terminated with CRLF should be complete",
        "LINE_COMPLETE",
        "status different from LINE_COMPLETE");
    expectEqual(
        completeLine,
        "PING :crlf",
        "Phase 19: CR before LF should be stripped from a complete line");
}

/**
 * @brief Phase 19 — fragmented TCP writes are reassembled and the resulting
 * server replies always use CRLF.
 */
static void testServerReassemblesFragmentedCommands() {
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 19 fragmented command tests")) {
        return;
    }

    const int socketFd = connectToServer(server.getPort());

    if (socketFd == -1) {
        reportFailure(
            "Client should connect for phase 19 fragmented command tests",
            "successful connection",
            "connection failed");
        return;
    }

    const std::string fragments[] = {
        "PASS se",
        "cret\r\nNICK frag",
        "user\r\nUSER fraguser 0 * :Frag",
        " User\r\n"
    };

    if (!sendChunks(socketFd, fragments, 4)) {
        reportFailure(
            "Fragmented registration should be sent",
            "successful send",
            "send failed");
        closeSocket(socketFd);
        return;
    }

    const std::string welcomeResponse =
        receiveAvailableData(socketFd, 1000);

    expectContains(
        welcomeResponse,
        ":irc.42.local 001 fraguser :Welcome to the IRC Network",
        "Phase 19: fragmented PASS/NICK/USER should complete registration");
    expectTrue(
        everyLineUsesCrlf(welcomeResponse),
        "Phase 19: welcome replies for fragmented input should use CRLF",
        "every line ending with \\r\\n",
        welcomeResponse);

    const std::string privmsgFragments[] = {
        "PRIV",
        "MSG #general :ho",
        "la\r",
        "\n"
    };

    sendAll(socketFd, "JOIN #general\r\n");
    discardPendingData(socketFd);

    if (!sendChunks(socketFd, privmsgFragments, 4)) {
        reportFailure(
            "Fragmented PRIVMSG should be sent",
            "successful send",
            "send failed");
        closeSocket(socketFd);
        return;
    }

    expectEqual(
        receiveAvailableData(socketFd, 500),
        "",
        "Phase 19: a fragmented channel PRIVMSG should not echo to the sender");

    expectTrue(
        server.isRunning(),
        "Phase 19: fragmented commands should not stop the server",
        "ircserv remains running",
        "ircserv exited");

    closeSocket(socketFd);
}

/**
 * @brief Phase 19 — several complete commands in one send are executed in
 * order, and LF-only input still produces CRLF replies.
 */
static void testServerSplitsBatchedCommandsAndLfInput() {
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 19 batched command tests")) {
        return;
    }

    const int batchedSocketFd = connectToServer(server.getPort());

    if (batchedSocketFd == -1) {
        reportFailure(
            "Client should connect for phase 19 batched command tests",
            "successful connection",
            "connection failed");
        return;
    }

    if (!sendAll(
            batchedSocketFd,
            "PASS secret\r\n"
            "NICK one\r\n"
            "USER one 0 * :One\r\n"
            "JOIN #a\r\n")) {
        reportFailure(
            "Batched registration and JOIN should be sent",
            "successful send",
            "send failed");
        closeSocket(batchedSocketFd);
        return;
    }

    const std::string batchedResponse =
        receiveAvailableData(batchedSocketFd, 1000);

    expectContains(
        batchedResponse,
        ":irc.42.local 001 one :Welcome to the IRC Network",
        "Phase 19: batched NICK and USER should register the client");
    expectContains(
        batchedResponse,
        " JOIN :#a\r\n",
        "Phase 19: a JOIN batched with registration should be processed");
    expectTrue(
        everyLineUsesCrlf(batchedResponse),
        "Phase 19: batched-command replies should use CRLF",
        "every line ending with \\r\\n",
        batchedResponse);

    closeSocket(batchedSocketFd);

    const int lfSocketFd = connectToServer(server.getPort());

    if (lfSocketFd == -1) {
        reportFailure(
            "Client should connect for phase 19 LF terminator tests",
            "successful connection",
            "connection failed");
        return;
    }

    if (!sendAll(
            lfSocketFd,
            "PASS secret\n"
            "NICK lfuser\n"
            "USER lfuser 0 * :Lf User\n"
            "PING :lf-token\n")) {
        reportFailure(
            "LF-terminated commands should be sent",
            "successful send",
            "send failed");
        closeSocket(lfSocketFd);
        return;
    }

    const std::string lfResponse =
        receiveAvailableData(lfSocketFd, 1000);

    expectContains(
        lfResponse,
        ":irc.42.local 001 lfuser :Welcome to the IRC Network",
        "Phase 19: LF-terminated registration should succeed");
    expectContains(
        lfResponse,
        "PONG :lf-token\r\n",
        "Phase 19: LF input should still produce CRLF replies");
    expectTrue(
        everyLineUsesCrlf(lfResponse),
        "Phase 19: every server reply must end with CRLF even after LF input",
        "every line ending with \\r\\n",
        lfResponse);

    closeSocket(lfSocketFd);
}

int main() {
    testClientReassemblesFragmentedLines();
    testClientSplitsBatchedCommands();
    testClientAcceptsLfAndCrlf();
    testServerReassemblesFragmentedCommands();
    testServerSplitsBatchedCommandsAndLfInput();

    return finishSuite("phase 19 TCP framing");
}
