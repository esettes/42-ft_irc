// Copyright 2026 @esettes, @danielfdez17
#include "ProtocolTestHarness.hpp"

/**
 * @brief Phase 19 — registration commands with missing or invalid parameters
 * return the documented numerics and leave the connection usable.
 */
static void testRegistrationProtocolErrors() {
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 19 registration error tests")) {
        return;
    }

    expectEqual(
        sendCommandAndReceive(server.getPort(), "PASS\r\n"),
        ":irc.42.local 461 * PASS :Not enough parameters\r\n",
        "Phase 19: PASS without a password should return 461");

    expectEqual(
        sendCommandAndReceive(server.getPort(), "PASS incorrectPassword\r\n"),
        ":irc.42.local 464 * :Password incorrect\r\n",
        "Phase 19: PASS with the wrong password should return 464");

    expectEqual(
        sendCommandAndReceive(server.getPort(), "NICK\r\n"),
        ":irc.42.local 431 * :No nickname given\r\n",
        "Phase 19: NICK without a parameter should return 431");

    expectEqual(
        sendCommandAndReceive(server.getPort(), "USER\r\n"),
        ":irc.42.local 461 * USER :Not enough parameters\r\n",
        "Phase 19: USER without parameters should return 461");

    expectEqual(
        sendCommandAndReceive(server.getPort(), "USER roxana\r\n"),
        ":irc.42.local 461 * USER :Not enough parameters\r\n",
        "Phase 19: USER with too few parameters should return 461");

    const int firstSocketFd = connectToServer(server.getPort());
    const int secondSocketFd = connectToServer(server.getPort());

    if (firstSocketFd == -1 || secondSocketFd == -1) {
        reportFailure(
            "Clients should connect for phase 19 nickname collision tests",
            "successful connections",
            "connection failed");
        closeSocket(firstSocketFd);
        closeSocket(secondSocketFd);
        return;
    }

    registerClient(firstSocketFd, "existingNick");
    registerClient(secondSocketFd, "challenger");

    expectEqual(
        sendLineAndReceive(secondSocketFd, "NICK existingNick\r\n"),
        ":irc.42.local 433 challenger "
        "existingNick :Nickname is already in use\r\n",
        "Phase 19: NICK of an in-use nickname should return 433");

    expectEqual(
        sendLineAndReceive(secondSocketFd, "PING :still-here\r\n"),
        "PONG :still-here\r\n",
        "Phase 19: a rejected NICK should keep the client connected");

    expectTrue(
        server.isRunning(),
        "Phase 19: invalid registration commands should not stop the server",
        "ircserv remains running",
        "ircserv exited");

    closeSocket(firstSocketFd);
    closeSocket(secondSocketFd);
}

/**
 * @brief Phase 19 — incomplete channel, message and operator commands return
 * coherent numerics without applying partial state or stopping the server.
 */
static void testChannelMessageAndOperatorProtocolErrors() {
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 19 protocol error barrage tests")) {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1 || memberSocketFd == -1) {
        reportFailure(
            "Clients should connect for phase 19 protocol error barrage tests",
            "successful connections",
            "connection failed");
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        return;
    }

    registerClient(operatorSocketFd, "operador");
    registerClient(memberSocketFd, "roxana");

    sendAll(operatorSocketFd, "JOIN #channel\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(operatorSocketFd, "MODE #channel +k secret\r\n");
    discardPendingData(operatorSocketFd);

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "JOIN\r\n"),
        ":irc.42.local 461 operador JOIN :Not enough parameters\r\n",
        "Phase 19: JOIN without parameters should return 461");
    expectEqual(
        sendLineAndReceive(operatorSocketFd, "JOIN invalid\r\n"),
        ":irc.42.local 403 operador invalid :No such channel\r\n",
        "Phase 19: JOIN with an invalid channel name should return 403");
    expectEqual(
        sendLineAndReceive(memberSocketFd, "JOIN #channel\r\n"),
        ":irc.42.local 475 roxana #channel :Cannot join channel (+k)\r\n",
        "Phase 19: JOIN without the required key should return 475");
    expectEqual(
        sendLineAndReceive(operatorSocketFd, "PART\r\n"),
        ":irc.42.local 461 operador PART :Not enough parameters\r\n",
        "Phase 19: PART without parameters should return 461");
    expectEqual(
        sendLineAndReceive(operatorSocketFd, "PART #nonexistent\r\n"),
        ":irc.42.local 403 operador #nonexistent :No such channel\r\n",
        "Phase 19: PART of an unknown channel should return 403");

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "PRIVMSG\r\n"),
        ":irc.42.local 411 operador :No recipient given (PRIVMSG)\r\n",
        "Phase 19: PRIVMSG without a recipient should return 411");
    expectEqual(
        sendLineAndReceive(operatorSocketFd, "PRIVMSG roxana\r\n"),
        ":irc.42.local 412 operador :No text to send\r\n",
        "Phase 19: PRIVMSG without text should return 412");
    expectEqual(
        sendLineAndReceive(operatorSocketFd, "PRIVMSG nobody :hello\r\n"),
        ":irc.42.local 401 operador nobody :No such nick\r\n",
        "Phase 19: PRIVMSG to an unknown nick should return 401");
    expectEqual(
        sendLineAndReceive(operatorSocketFd, "PRIVMSG #nonexistent :hello\r\n"),
        ":irc.42.local 403 operador #nonexistent :No such channel\r\n",
        "Phase 19: PRIVMSG to an unknown channel should return 403");

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE\r\n"),
        ":irc.42.local 461 operador MODE :Not enough parameters\r\n",
        "Phase 19: MODE without a target should return 461");
    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #channel +k\r\n"),
        ":irc.42.local 461 operador MODE :Not enough parameters\r\n",
        "Phase 19: MODE +k without a key should return 461");
    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #channel +l\r\n"),
        ":irc.42.local 461 operador MODE :Not enough parameters\r\n",
        "Phase 19: MODE +l without a limit should return 461");
    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #channel +o\r\n"),
        ":irc.42.local 461 operador MODE :Not enough parameters\r\n",
        "Phase 19: MODE +o without a nickname should return 461");
    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #channel +o nobody\r\n"),
        ":irc.42.local 401 operador nobody :No such nick\r\n",
        "Phase 19: MODE +o of an unknown nick should return 401");

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "KICK\r\n"),
        ":irc.42.local 461 operador KICK :Not enough parameters\r\n",
        "Phase 19: KICK without parameters should return 461");
    expectEqual(
        sendLineAndReceive(operatorSocketFd, "KICK #channel\r\n"),
        ":irc.42.local 461 operador KICK :Not enough parameters\r\n",
        "Phase 19: KICK without a target should return 461");
    expectEqual(
        sendLineAndReceive(operatorSocketFd, "KICK #channel nobody\r\n"),
        ":irc.42.local 401 operador nobody :No such nick\r\n",
        "Phase 19: KICK of an unknown nick should return 401");

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "INVITE\r\n"),
        ":irc.42.local 461 operador INVITE :Not enough parameters\r\n",
        "Phase 19: INVITE without parameters should return 461");
    expectEqual(
        sendLineAndReceive(operatorSocketFd, "INVITE nobody #channel\r\n"),
        ":irc.42.local 401 operador nobody :No such nick\r\n",
        "Phase 19: INVITE of an unknown nick should return 401");

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "TOPIC\r\n"),
        ":irc.42.local 461 operador TOPIC :Not enough parameters\r\n",
        "Phase 19: TOPIC without a channel should return 461");
    expectEqual(
        sendLineAndReceive(operatorSocketFd, "TOPIC #nonexistent\r\n"),
        ":irc.42.local 403 operador #nonexistent :No such channel\r\n",
        "Phase 19: TOPIC on an unknown channel should return 403");

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #channel\r\n"),
        ":irc.42.local 324 operador #channel +k secret\r\n",
        "Phase 19: rejected operator"
        "commands should not apply partial MODE state");

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "PING :after-errors\r\n"),
        "PONG :after-errors\r\n",
        "Phase 19: the client should keep"
        "sending commands after protocol errors");

    expectTrue(
        server.isRunning(),
        "Phase 19: invalid channel and"
        "operator commands should not stop the server",
        "ircserv remains running",
        "ircserv exited");

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
}

int main() {
    testRegistrationProtocolErrors();
    testChannelMessageAndOperatorProtocolErrors();

    return finishSuite("phase 19 protocol errors");
}
