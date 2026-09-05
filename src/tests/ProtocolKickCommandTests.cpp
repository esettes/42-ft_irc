// Copyright 2026 @esettes, @danielfdez17
#include "ProtocolTestHarness.hpp"

/**
 * @brief Phase 16 — a valid KICK broadcasts the operator prefix, stored
 * channel name and target nickname to every current member, including the
 * expelled user, then removes only that membership.
 */
static void testValidKickBroadcastAndMembership()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 16 KICK delivery tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());
    const int targetSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1
        || memberSocketFd == -1
        || targetSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 16 KICK delivery tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        closeSocket(targetSocketFd);
        return;
    }

    const std::string operatorWelcome =
        registerClient(operatorSocketFd, "admin");
    registerClient(memberSocketFd, "usuario2");
    registerClient(targetSocketFd, "roxana");

    std::string operatorPrefix;

    if (!extractClientPrefixFromWelcome(
            operatorWelcome,
            "admin",
            operatorPrefix
        ))
    {
        reportFailure(
            "Welcome should expose a prefix for phase 16 KICK delivery tests",
            "welcome containing admin!admin@host",
            operatorWelcome
        );
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

    sendAll(
        operatorSocketFd,
        "KICK #general roxana :Comportamiento inapropiado\r\n"
    );

    const std::string expectedKick =
        ":" + operatorPrefix
        + " KICK #general roxana :Comportamiento inapropiado\r\n";

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        expectedKick,
        "Phase 16: the operator should receive the KICK broadcast"
    );
    expectEqual(
        receiveAvailableData(memberSocketFd, 500),
        expectedKick,
        "Phase 16: remaining members should receive the KICK broadcast"
    );
    expectEqual(
        receiveAvailableData(targetSocketFd, 500),
        expectedKick,
        "Phase 16: the expelled user should receive the KICK before removal"
    );

    expectEqual(
        sendLineAndReceive(targetSocketFd, "PRIVMSG #general :ya no estoy\r\n"),
        ":irc.42.local 404 roxana #general :Cannot send to channel\r\n",
        "Phase 16: the expelled user should no longer belong to the channel"
    );

    expectEqual(
        receiveAvailableData(memberSocketFd, 200),
        "",
        "Phase 16: remaining members should not receive messages from a kicked user"
    );

    sendAll(operatorSocketFd, "PRIVMSG #general :despues del kick\r\n");

    expectContains(
        receiveAvailableData(memberSocketFd, 500),
        " PRIVMSG #general :despues del kick\r\n",
        "Phase 16: remaining members should still receive channel messages"
    );
    expectEqual(
        receiveAvailableData(targetSocketFd, 200),
        "",
        "Phase 16: a kicked user should not receive later channel messages"
    );

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
    closeSocket(targetSocketFd);
}

/**
 * @brief Phase 16 — omitting the reason uses the operator nickname, IRC
 * casemapping resolves channel and nick, and kicking a user does not close
 * their server connection or remove them from other channels.
 */
static void testKickDefaultReasonCasemapAndConnection()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 16 KICK casemap tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int targetSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1 || targetSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 16 KICK casemap tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(targetSocketFd);
        return;
    }

    const std::string operatorWelcome =
        registerClient(operatorSocketFd, "admin");
    const std::string targetWelcome =
        registerClient(targetSocketFd, "roxana");

    std::string operatorPrefix;
    std::string targetPrefix;

    if (!extractClientPrefixFromWelcome(
            operatorWelcome,
            "admin",
            operatorPrefix
        )
        || !extractClientPrefixFromWelcome(
            targetWelcome,
            "roxana",
            targetPrefix
        ))
    {
        reportFailure(
            "Welcome should expose prefixes for phase 16 KICK casemap tests",
            "welcome containing admin!admin@host and roxana!roxana@host",
            operatorWelcome + targetWelcome
        );
        closeSocket(operatorSocketFd);
        closeSocket(targetSocketFd);
        return;
    }

    sendAll(operatorSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(targetSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(targetSocketFd);

    sendAll(targetSocketFd, "JOIN #otro\r\n");
    discardPendingData(targetSocketFd);

    sendAll(operatorSocketFd, "KICK #GENERAL ROXANA\r\n");

    const std::string expectedDefaultKick =
        ":" + operatorPrefix + " KICK #general roxana :admin\r\n";

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        expectedDefaultKick,
        "Phase 16: KICK without a reason should default to the operator nickname"
    );
    expectEqual(
        receiveAvailableData(targetSocketFd, 500),
        expectedDefaultKick,
        "Phase 16: KICK should resolve nicknames and channels with casemapping"
    );

    expectEqual(
        sendLineAndReceive(targetSocketFd, "PING :still-here\r\n"),
        "PONG :still-here\r\n",
        "Phase 16: kicking a user should keep their server connection open"
    );

    sendAll(operatorSocketFd, "PRIVMSG roxana :sigues conectada\r\n");

    expectEqual(
        receiveAvailableData(targetSocketFd, 500),
        ":" + operatorPrefix + " PRIVMSG roxana :sigues conectada\r\n",
        "Phase 16: a kicked user should still receive private messages"
    );

    sendAll(
        targetSocketFd,
        "PRIVMSG #otro :sigo en el otro canal\r\n"
    );

    expectEqual(
        receiveAvailableData(targetSocketFd, 200),
        "",
        "Phase 16: a kicked user should keep membership of other channels"
    );

    sendAll(targetSocketFd, "JOIN #general\r\n");

    const std::string rejoinResponse =
        receiveAvailableData(targetSocketFd, 500);

    expectContains(
        rejoinResponse,
        ":" + targetPrefix + " JOIN :#general\r\n",
        "Phase 16: a kicked user should be able to JOIN the channel again"
    );
    expectContains(
        rejoinResponse,
        ":irc.42.local 353 roxana = #general :",
        "Phase 16: rejoining after a KICK should report the current members"
    );
    expectTrue(
        rejoinResponse.find("@roxana") == std::string::npos,
        "Phase 16: rejoining after a KICK should not restore operator status",
        "NAMES without @roxana",
        rejoinResponse
    );

    closeSocket(operatorSocketFd);
    closeSocket(targetSocketFd);
}

/**
 * @brief Phase 16 — kicking the last remaining member besides the operator
 * keeps the channel, while kicking the last member deletes it so a later
 * JOIN recreates it and grants operator status.
 */
static void testKickEmptyChannelDeletion()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 16 KICK cleanup tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1 || memberSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 16 KICK cleanup tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        return;
    }

    const std::string operatorWelcome =
        registerClient(operatorSocketFd, "admin");
    registerClient(memberSocketFd, "roxana");

    std::string operatorPrefix;

    if (!extractClientPrefixFromWelcome(
            operatorWelcome,
            "admin",
            operatorPrefix
        ))
    {
        reportFailure(
            "Welcome should expose a prefix for phase 16 KICK cleanup tests",
            "welcome containing admin!admin@host",
            operatorWelcome
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        return;
    }

    sendAll(operatorSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);

    sendAll(memberSocketFd, "JOIN #general\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    sendAll(operatorSocketFd, "KICK #general roxana :fuera\r\n");
    discardPendingData(operatorSocketFd);
    discardPendingData(memberSocketFd);

    sendAll(operatorSocketFd, "PRIVMSG #general :canal sigue\r\n");

    expectEqual(
        receiveAvailableData(operatorSocketFd, 200),
        "",
        "Phase 16: kicking a non-last member should keep the channel"
    );

    sendAll(operatorSocketFd, "KICK #general admin :me voy\r\n");

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        ":" + operatorPrefix + " KICK #general admin :me voy\r\n",
        "Phase 16: an operator should be able to kick themselves"
    );

    sendAll(memberSocketFd, "JOIN #general\r\n");

    const std::string expectedRecreatedChannel =
        receiveAvailableData(memberSocketFd, 500);

    expectContains(
        expectedRecreatedChannel,
        " JOIN :#general\r\n",
        "Phase 16: joining a channel emptied by KICK should recreate it"
    );
    expectContains(
        expectedRecreatedChannel,
        ":irc.42.local 353 roxana = #general :@roxana\r\n",
        "Phase 16: the first member of a recreated channel should become operator"
    );

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
}

int main()
{
    testValidKickBroadcastAndMembership();
    testKickDefaultReasonCasemapAndConnection();
    testKickEmptyChannelDeletion();

    return finishSuite("phase 16 KICK command");
}
