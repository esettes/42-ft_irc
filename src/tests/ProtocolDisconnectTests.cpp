// Copyright 2026 @esettes, @danielfdez17
#include "ProtocolTestHarness.hpp"

/**
 * @brief Phase 19 — closing an unregistered TCP connection does not stop the
 * server or leak the nickname namespace for later clients.
 */
static void testDisconnectUnregisteredClient()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 19 unregistered disconnect tests"
        ))
    {
        return;
    }

    const int unregisteredSocketFd = connectToServer(server.getPort());

    if (unregisteredSocketFd == -1)
    {
        reportFailure(
            "Client should connect for phase 19 unregistered disconnect tests",
            "successful connection",
            "connection failed"
        );
        return;
    }

    closeSocket(unregisteredSocketFd);
    ::usleep(100000);

    expectEqual(
        sendCommandAndReceive(server.getPort(), "UNKNOWN\r\n"),
        ":irc.42.local 421 * UNKNOWN :Unknown command\r\n",
        "Phase 19: the server should accept a new client after an unregistered disconnect"
    );

    expectTrue(
        server.isRunning(),
        "Phase 19: disconnecting an unregistered client should not stop the server",
        "ircserv remains running",
        "ircserv exited"
    );
}

/**
 * @brief Phase 19 — a registered user with no channels can be closed with
 * recv()==0, and the nickname becomes available again.
 */
static void testDisconnectRegisteredUserWithoutChannels()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 19 idle-user disconnect tests"
        ))
    {
        return;
    }

    const int socketFd = connectToServer(server.getPort());

    if (socketFd == -1)
    {
        reportFailure(
            "Client should connect for phase 19 idle-user disconnect tests",
            "successful connection",
            "connection failed"
        );
        return;
    }

    registerClient(socketFd, "idleuser");
    closeSocket(socketFd);
    ::usleep(100000);

    const int reusedSocketFd = connectToServer(server.getPort());

    if (reusedSocketFd == -1)
    {
        reportFailure(
            "A new client should connect after an idle disconnect",
            "successful connection",
            "connection failed"
        );
        return;
    }

    const std::string reusedResponse =
        registerClient(reusedSocketFd, "idleuser");

    expectContains(
        reusedResponse,
        ":irc.42.local 001 idleuser ",
        "Phase 19: an unexpected disconnect should release the nickname"
    );
    expectTrue(
        reusedResponse.find(" 433 ") == std::string::npos,
        "Phase 19: nickname reuse after recv()==0 should not return 433",
        "registration without numeric 433",
        reusedResponse
    );

    closeSocket(reusedSocketFd);
}

/**
 * @brief Phase 19 — a user present in several channels produces a single QUIT
 * for a peer that shared more than one channel.
 */
static void testDisconnectFromMultipleChannelsSendsOneQuit()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 19 multi-channel QUIT tests"
        ))
    {
        return;
    }

    const int aliceSocketFd = connectToServer(server.getPort());
    const int bobSocketFd = connectToServer(server.getPort());
    const int carolSocketFd = connectToServer(server.getPort());

    if (aliceSocketFd == -1 || bobSocketFd == -1 || carolSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 19 multi-channel QUIT tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(aliceSocketFd);
        closeSocket(bobSocketFd);
        closeSocket(carolSocketFd);
        return;
    }

    const std::string aliceWelcome =
        registerClient(aliceSocketFd, "alice");
    registerClient(bobSocketFd, "bob");
    registerClient(carolSocketFd, "carol");

    std::string alicePrefix;

    if (!extractClientPrefixFromWelcome(
            aliceWelcome,
            "alice",
            alicePrefix
        ))
    {
        reportFailure(
            "Welcome should expose a prefix for phase 19 multi-channel QUIT tests",
            "welcome containing alice!alice@host",
            aliceWelcome
        );
        closeSocket(aliceSocketFd);
        closeSocket(bobSocketFd);
        closeSocket(carolSocketFd);
        return;
    }

    sendAll(aliceSocketFd, "JOIN #one\r\nJOIN #two\r\n");
    discardPendingData(aliceSocketFd);

    sendAll(bobSocketFd, "JOIN #one\r\nJOIN #two\r\n");
    discardPendingData(aliceSocketFd);
    discardPendingData(bobSocketFd);

    sendAll(carolSocketFd, "JOIN #one\r\n");
    discardPendingData(aliceSocketFd);
    discardPendingData(bobSocketFd);
    discardPendingData(carolSocketFd);

    sendAll(aliceSocketFd, "QUIT :leaving both\r\n");
    waitForSocketClosure(aliceSocketFd, 1000);
    closeSocket(aliceSocketFd);

    const std::string bobQuit =
        receiveAvailableData(bobSocketFd, 500);
    const std::string carolQuit =
        receiveAvailableData(carolSocketFd, 500);
    const std::string expectedQuit =
        ":" + alicePrefix + " QUIT :leaving both\r\n";

    expectEqual(
        bobQuit,
        expectedQuit,
        "Phase 19: a peer sharing two channels should receive exactly one QUIT"
    );
    expectEqual(
        carolQuit,
        expectedQuit,
        "Phase 19: a peer sharing one channel should receive the QUIT"
    );
    expectTrue(
        countOccurrences(bobQuit, " QUIT :") == 1,
        "Phase 19: QUIT must not be duplicated for a multi-channel peer",
        "exactly one QUIT",
        bobQuit
    );

    closeSocket(bobSocketFd);
    closeSocket(carolSocketFd);
}

/**
 * @brief Phase 19 — an unexpected TCP close (recv()==0 / POLLHUP) notifies
 * remaining members with QUIT and keeps the rest of the network usable.
 */
static void testUnexpectedTcpCloseNotifiesPeers()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 19 unexpected close tests"
        ))
    {
        return;
    }

    const int aliceSocketFd = connectToServer(server.getPort());
    const int bobSocketFd = connectToServer(server.getPort());

    if (aliceSocketFd == -1 || bobSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 19 unexpected close tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(aliceSocketFd);
        closeSocket(bobSocketFd);
        return;
    }

    const std::string aliceWelcome =
        registerClient(aliceSocketFd, "alice");
    registerClient(bobSocketFd, "bob");

    std::string alicePrefix;

    if (!extractClientPrefixFromWelcome(
            aliceWelcome,
            "alice",
            alicePrefix
        ))
    {
        reportFailure(
            "Welcome should expose a prefix for phase 19 unexpected close tests",
            "welcome containing alice!alice@host",
            aliceWelcome
        );
        closeSocket(aliceSocketFd);
        closeSocket(bobSocketFd);
        return;
    }

    sendAll(aliceSocketFd, "JOIN #general\r\n");
    discardPendingData(aliceSocketFd);

    sendAll(bobSocketFd, "JOIN #general\r\n");
    discardPendingData(aliceSocketFd);
    discardPendingData(bobSocketFd);

    closeSocket(aliceSocketFd);

    const std::string bobNotification =
        receiveAvailableData(bobSocketFd, 1000);

    expectContains(
        bobNotification,
        ":" + alicePrefix + " QUIT :",
        "Phase 19: recv()==0 should notify remaining members with QUIT"
    );

    expectEqual(
        sendLineAndReceive(bobSocketFd, "PING :after-hup\r\n"),
        "PONG :after-hup\r\n",
        "Phase 19: a remaining member should stay connected after a peer hangup"
    );

    closeSocket(bobSocketFd);
}

/**
 * @brief Phase 19 — shutting the server down while clients are connected
 * must close those connections instead of hanging.
 */
static void testServerShutdownWithConnectedClients()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 19 shutdown tests"
        ))
    {
        return;
    }

    const int firstSocketFd = connectToServer(server.getPort());
    const int secondSocketFd = connectToServer(server.getPort());

    if (firstSocketFd == -1 || secondSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 19 shutdown tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(firstSocketFd);
        closeSocket(secondSocketFd);
        return;
    }

    registerClient(firstSocketFd, "alpha");
    registerClient(secondSocketFd, "beta");

    sendAll(firstSocketFd, "JOIN #shutdown\r\n");
    discardPendingData(firstSocketFd);
    sendAll(secondSocketFd, "JOIN #shutdown\r\n");
    discardPendingData(firstSocketFd);
    discardPendingData(secondSocketFd);

    server.stop();

    expectTrue(
        !server.isRunning(),
        "Phase 19: SIGINT should stop the server while clients are connected",
        "ircserv has exited",
        "ircserv still running"
    );

    closeSocket(firstSocketFd);
    closeSocket(secondSocketFd);
}

int main()
{
    testDisconnectUnregisteredClient();
    testDisconnectRegisteredUserWithoutChannels();
    testDisconnectFromMultipleChannelsSendsOneQuit();
    testUnexpectedTcpCloseNotifiesPeers();
    testServerShutdownWithConnectedClients();

    return finishSuite("phase 19 disconnects");
}
