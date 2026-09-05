// Copyright 2026 @esettes, @danielfdez17
#include "ProtocolTestHarness.hpp"

/**
 * @brief Phase 19 — several clients register independently with unique
 * nicknames, join the same channel, exchange private and channel messages,
 * and survive one peer closing while the others continue.
 */
static void testMultipleClientsIndependentRegistrationAndMessaging()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 19 multi-client tests"
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
            "Clients should connect for phase 19 multi-client tests",
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
    const std::string bobWelcome =
        registerClient(bobSocketFd, "bob");
    registerClient(carolSocketFd, "carol");

    std::string alicePrefix;
    std::string bobPrefix;

    if (!extractClientPrefixFromWelcome(aliceWelcome, "alice", alicePrefix)
        || !extractClientPrefixFromWelcome(bobWelcome, "bob", bobPrefix))
    {
        reportFailure(
            "Welcome should expose prefixes for phase 19 multi-client tests",
            "welcome containing alice and bob prefixes",
            aliceWelcome + bobWelcome
        );
        closeSocket(aliceSocketFd);
        closeSocket(bobSocketFd);
        closeSocket(carolSocketFd);
        return;
    }

    expectEqual(
        sendLineAndReceive(carolSocketFd, "NICK alice\r\n"),
        ":irc.42.local 433 carol alice :Nickname is already in use\r\n",
        "Phase 19: nicknames must stay unique across simultaneous clients"
    );

    sendAll(aliceSocketFd, "JOIN #room\r\n");
    discardPendingData(aliceSocketFd);
    sendAll(bobSocketFd, "JOIN #room\r\n");
    discardPendingData(aliceSocketFd);
    discardPendingData(bobSocketFd);
    sendAll(carolSocketFd, "JOIN #room\r\n");
    discardPendingData(aliceSocketFd);
    discardPendingData(bobSocketFd);
    discardPendingData(carolSocketFd);

    sendAll(aliceSocketFd, "PRIVMSG bob :privado\r\n");
    expectEqual(
        receiveAvailableData(bobSocketFd, 500),
        ":" + alicePrefix + " PRIVMSG bob :privado\r\n",
        "Phase 19: private messages should reach only the intended client"
    );
    expectEqual(
        receiveAvailableData(carolSocketFd, 200),
        "",
        "Phase 19: a private message should not reach an unrelated client"
    );

    sendAll(aliceSocketFd, "PRIVMSG #room :canal\r\n");
    const std::string expectedChannelMessage =
        ":" + alicePrefix + " PRIVMSG #room :canal\r\n";

    expectEqual(
        receiveAvailableData(bobSocketFd, 500),
        expectedChannelMessage,
        "Phase 19: channel messages should reach bob"
    );
    expectEqual(
        receiveAvailableData(carolSocketFd, 500),
        expectedChannelMessage,
        "Phase 19: channel messages should reach carol"
    );

    sendAll(aliceSocketFd, "TOPIC #room :tema compartido\r\n");
    const std::string expectedTopic =
        ":" + alicePrefix + " TOPIC #room :tema compartido\r\n";

    expectEqual(
        receiveAvailableData(aliceSocketFd, 500),
        expectedTopic,
        "Phase 19: a topic change should notify the sender"
    );
    expectEqual(
        receiveAvailableData(bobSocketFd, 500),
        expectedTopic,
        "Phase 19: a topic change should notify bob"
    );
    expectEqual(
        receiveAvailableData(carolSocketFd, 500),
        expectedTopic,
        "Phase 19: a topic change should notify carol"
    );

    closeSocket(carolSocketFd);

    const std::string aliceAfterClose =
        receiveAvailableData(aliceSocketFd, 1000);
    const std::string bobAfterClose =
        receiveAvailableData(bobSocketFd, 1000);

    expectContains(
        aliceAfterClose,
        " QUIT :",
        "Phase 19: remaining clients should be notified when another client closes"
    );
    expectContains(
        bobAfterClose,
        " QUIT :",
        "Phase 19: every remaining member should receive the closing client's QUIT"
    );

    sendAll(bobSocketFd, "PRIVMSG #room :seguimos\r\n");
    expectEqual(
        receiveAvailableData(aliceSocketFd, 500),
        ":" + bobPrefix + " PRIVMSG #room :seguimos\r\n",
        "Phase 19: remaining clients should keep exchanging channel messages"
    );

    expectTrue(
        server.isRunning(),
        "Phase 19: closing one of several clients should not stop the server",
        "ircserv remains running",
        "ircserv exited"
    );

    closeSocket(aliceSocketFd);
    closeSocket(bobSocketFd);
}

/**
 * @brief Phase 19 — invitations, kicks and mode changes remain consistent
 * when three clients interact on the same channel.
 */
static void testMultipleClientsInviteKickAndMode()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 19 multi-client operator tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());
    const int guestSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1
        || memberSocketFd == -1
        || guestSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 19 multi-client operator tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        closeSocket(guestSocketFd);
        return;
    }

    const std::string operatorWelcome =
        registerClient(operatorSocketFd, "operador");
    registerClient(memberSocketFd, "miembro");
    registerClient(guestSocketFd, "invitado");

    std::string operatorPrefix;

    if (!extractClientPrefixFromWelcome(
            operatorWelcome,
            "operador",
            operatorPrefix
        ))
    {
        reportFailure(
            "Welcome should expose a prefix for phase 19 multi-client operator tests",
            "welcome containing operador!operador@host",
            operatorWelcome
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        closeSocket(guestSocketFd);
        return;
    }

    sendAll(operatorSocketFd, "JOIN #ops\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(memberSocketFd, "JOIN #ops\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    sendAll(operatorSocketFd, "MODE #ops +i\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    expectEqual(
        sendLineAndReceive(guestSocketFd, "JOIN #ops\r\n"),
        ":irc.42.local 473 invitado #ops :Cannot join channel (+i)\r\n",
        "Phase 19: invite-only should reject an uninvited simultaneous client"
    );

    sendAll(operatorSocketFd, "INVITE invitado #ops\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(guestSocketFd);

    sendAll(guestSocketFd, "JOIN #ops\r\n");
    expectContains(
        receiveAvailableData(guestSocketFd, 500),
        " JOIN :#ops\r\n",
        "Phase 19: an invited client should be able to join while others remain connected"
    );
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    sendAll(operatorSocketFd, "KICK #ops invitado :fuera\r\n");
    const std::string expectedKick =
        ":" + operatorPrefix + " KICK #ops invitado :fuera\r\n";

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        expectedKick,
        "Phase 19: the operator should receive the KICK broadcast"
    );
    expectEqual(
        receiveAvailableData(memberSocketFd, 500),
        expectedKick,
        "Phase 19: a remaining member should receive the KICK broadcast"
    );
    expectEqual(
        receiveAvailableData(guestSocketFd, 500),
        expectedKick,
        "Phase 19: the kicked client should receive the KICK broadcast"
    );

    expectEqual(
        sendLineAndReceive(guestSocketFd, "PING :kicked-but-connected\r\n"),
        "PONG :kicked-but-connected\r\n",
        "Phase 19: kicking one client should keep that TCP connection open"
    );

    expectEqual(
        sendLineAndReceive(memberSocketFd, "PING :untouched\r\n"),
        "PONG :untouched\r\n",
        "Phase 19: a kick should not disconnect unrelated clients"
    );

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
    closeSocket(guestSocketFd);
}

int main()
{
    testMultipleClientsIndependentRegistrationAndMessaging();
    testMultipleClientsInviteKickAndMode();

    return finishSuite("phase 19 multi-client");
}
