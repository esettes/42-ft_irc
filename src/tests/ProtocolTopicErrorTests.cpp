#include "ProtocolTestHarness.hpp"

/**
 * @brief Phase 14 — TOPIC error paths: 451 before registration, 461 without a
 * channel, 403 for unknown channels, and 442 when the client is not a member.
 */
static void testTopicRegistrationAndParameterErrors()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 14 TOPIC error tests"
        ))
    {
        return;
    }

    expectEqual(
        sendCommandAndReceive(server.getPort(), "TOPIC #general\r\n"),
        ":irc.42.local 451 * :You have not registered\r\n",
        "Phase 14: TOPIC before registration should return 451"
    );

    const int socketFd = connectToServer(server.getPort());

    if (socketFd == -1)
    {
        reportFailure(
            "Client should connect for phase 14 TOPIC error tests",
            "successful connection",
            "connection failed"
        );
        return;
    }

    registerClient(socketFd, "roxana");

    expectEqual(
        sendLineAndReceive(socketFd, "TOPIC\r\n"),
        ":irc.42.local 461 roxana TOPIC :Not enough parameters\r\n",
        "Phase 14: TOPIC without a channel should return 461"
    );

    expectEqual(
        sendLineAndReceive(socketFd, "TOPIC :\r\n"),
        ":irc.42.local 461 roxana TOPIC :Not enough parameters\r\n",
        "Phase 14: TOPIC with an empty channel parameter should return 461"
    );

    expectEqual(
        sendLineAndReceive(socketFd, "TOPIC #inexistente\r\n"),
        ":irc.42.local 403 roxana #inexistente :No such channel\r\n",
        "Phase 14: querying an unknown channel should return 403"
    );

    expectEqual(
        sendLineAndReceive(
            socketFd,
            "TOPIC #inexistente :Nuevo tema\r\n"
        ),
        ":irc.42.local 403 roxana #inexistente :No such channel\r\n",
        "Phase 14: setting a topic on an unknown channel should return 403"
    );

    expectTrue(
        server.isRunning(),
        "Phase 14: invalid TOPIC commands should not stop the server",
        "ircserv remains running",
        "ircserv exited"
    );

    closeSocket(socketFd);
}

/**
 * @brief Phase 14 — a registered outsider cannot query or change the topic of
 * an existing channel.
 */
static void testTopicNotOnChannelErrors()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 14 TOPIC membership error tests"
        ))
    {
        return;
    }

    const int ownerSocketFd = connectToServer(server.getPort());
    const int outsiderSocketFd = connectToServer(server.getPort());

    if (ownerSocketFd == -1 || outsiderSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 14 TOPIC membership error tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(ownerSocketFd);
        closeSocket(outsiderSocketFd);
        return;
    }

    registerClient(ownerSocketFd, "owner");
    registerClient(outsiderSocketFd, "outsider");

    sendAll(ownerSocketFd, "JOIN #general\r\n");
    discardPendingData(ownerSocketFd);

    expectEqual(
        sendLineAndReceive(outsiderSocketFd, "TOPIC #general\r\n"),
        ":irc.42.local 442 outsider #general :You're not on that channel\r\n",
        "Phase 14: querying a channel from outside should return 442"
    );

    expectEqual(
        sendLineAndReceive(
            outsiderSocketFd,
            "TOPIC #general :No deberia aplicarse\r\n"
        ),
        ":irc.42.local 442 outsider #general :You're not on that channel\r\n",
        "Phase 14: setting a topic from outside should return 442"
    );

    expectEqual(
        receiveAvailableData(ownerSocketFd, 200),
        "",
        "Phase 14: rejected TOPIC commands should not notify channel members"
    );

    closeSocket(ownerSocketFd);
    closeSocket(outsiderSocketFd);
}

int main()
{
    testTopicRegistrationAndParameterErrors();
    testTopicNotOnChannelErrors();

    return finishSuite("phase 14 TOPIC error");
}
