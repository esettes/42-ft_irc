// Copyright 2026 @esettes, @danielfdez17
#include "ProtocolTestHarness.hpp"

/**
 * @brief Phase 17 — several flags in one MODE string are applied together and
 * broadcast with a single reconstructed mode list.
 */
static void testCombinedModeFlags()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 17 MODE combination tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1 || memberSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 17 MODE combination tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        return;
    }

    const std::string operatorWelcome =
        registerClient(operatorSocketFd, "operador");
    registerClient(memberSocketFd, "roxana");

    std::string operatorPrefix;

    if (!extractClientPrefixFromWelcome(
            operatorWelcome,
            "operador",
            operatorPrefix
        ))
    {
        reportFailure(
            "Welcome should expose a prefix for phase 17 MODE combination tests",
            "welcome containing operador!operador@host",
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

    sendAll(operatorSocketFd, "MODE #general +it\r\n");

    const std::string expectedEnable =
        ":" + operatorPrefix + " MODE #general +it\r\n";

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        expectedEnable,
        "Phase 17: +it should notify the operator as a single MODE message"
    );
    expectEqual(
        receiveAvailableData(memberSocketFd, 500),
        expectedEnable,
        "Phase 17: +it should notify every channel member"
    );

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #general\r\n"),
        ":irc.42.local 324 operador #general +it\r\n",
        "Phase 17: +it should enable both invite-only and topic restriction"
    );

    sendAll(operatorSocketFd, "MODE #general -it\r\n");

    const std::string expectedDisable =
        ":" + operatorPrefix + " MODE #general -it\r\n";

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        expectedDisable,
        "Phase 17: -it should notify the operator as a single MODE message"
    );
    expectEqual(
        receiveAvailableData(memberSocketFd, 500),
        expectedDisable,
        "Phase 17: -it should notify every channel member"
    );

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #general\r\n"),
        ":irc.42.local 324 operador #general +\r\n",
        "Phase 17: -it should clear both invite-only and topic restriction"
    );

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
}

/**
 * @brief Phase 17 — modes that take arguments consume them in letter order,
 * including when + and - appear in the same string.
 */
static void testCombinedModeArgumentsAndSignChange()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 17 MODE argument combination tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1 || memberSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 17 MODE argument combination tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        return;
    }

    const std::string operatorWelcome =
        registerClient(operatorSocketFd, "operador");
    registerClient(memberSocketFd, "roxana");

    std::string operatorPrefix;

    if (!extractClientPrefixFromWelcome(
            operatorWelcome,
            "operador",
            operatorPrefix
        ))
    {
        reportFailure(
            "Welcome should expose a prefix for phase 17 MODE argument combination tests",
            "welcome containing operador!operador@host",
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

    sendAll(operatorSocketFd, "MODE #general +kl secret 10\r\n");

    const std::string expectedKeyAndLimit =
        ":" + operatorPrefix + " MODE #general +kl secret 10\r\n";

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        expectedKeyAndLimit,
        "Phase 17: +kl should consume the key then the limit"
    );
    expectEqual(
        receiveAvailableData(memberSocketFd, 500),
        expectedKeyAndLimit,
        "Phase 17: +kl should notify every channel member"
    );

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #general\r\n"),
        ":irc.42.local 324 operador #general +kl secret 10\r\n",
        "Phase 17: +kl should store both the key and the user limit"
    );

    sendAll(operatorSocketFd, "MODE #general +o-l roxana\r\n");

    const std::string expectedSignChange =
        ":" + operatorPrefix + " MODE #general +o-l roxana\r\n";

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        expectedSignChange,
        "Phase 17: +o-l should grant operator status then remove the limit"
    );
    expectEqual(
        receiveAvailableData(memberSocketFd, 500),
        expectedSignChange,
        "Phase 17: +o-l should notify every channel member"
    );

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #general\r\n"),
        ":irc.42.local 324 operador #general +k secret\r\n",
        "Phase 17: +o-l should leave the key set and clear the user limit"
    );

    sendAll(memberSocketFd, "MODE #general +i\r\n");

    expectContains(
        receiveAvailableData(memberSocketFd, 500),
        " MODE #general +i\r\n",
        "Phase 17: +o should allow the promoted member to change modes"
    );

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
}

/**
 * @brief Phase 17 — +kol consumes the key, nickname and limit in that order.
 */
static void testCombinedKolParameterOrder()
{
    TestServerProcess server;

    if (!startServerOrFail(
            server,
            "Server should start for phase 17 MODE +kol tests"
        ))
    {
        return;
    }

    const int operatorSocketFd = connectToServer(server.getPort());
    const int memberSocketFd = connectToServer(server.getPort());

    if (operatorSocketFd == -1 || memberSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 17 MODE +kol tests",
            "successful connections",
            "connection failed"
        );
        closeSocket(operatorSocketFd);
        closeSocket(memberSocketFd);
        return;
    }

    const std::string operatorWelcome =
        registerClient(operatorSocketFd, "operador");
    const std::string memberWelcome =
        registerClient(memberSocketFd, "roxana");

    std::string operatorPrefix;
    std::string memberPrefix;

    if (!extractClientPrefixFromWelcome(
            operatorWelcome,
            "operador",
            operatorPrefix
        )
        || !extractClientPrefixFromWelcome(
            memberWelcome,
            "roxana",
            memberPrefix
        ))
    {
        reportFailure(
            "Welcome should expose a prefix for phase 17 MODE +kol tests",
            "welcome containing operador and roxana prefixes",
            operatorWelcome + memberWelcome
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

    sendAll(operatorSocketFd, "MODE #general +kol secret roxana 10\r\n");

    const std::string expectedKol =
        ":" + operatorPrefix + " MODE #general +kol secret roxana 10\r\n";

    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        expectedKol,
        "Phase 17: +kol should consume key, nickname and limit in letter order"
    );
    expectEqual(
        receiveAvailableData(memberSocketFd, 500),
        expectedKol,
        "Phase 17: +kol should notify every channel member"
    );

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #general\r\n"),
        ":irc.42.local 324 operador #general +kl secret 10\r\n",
        "Phase 17: +kol should set the key and limit without listing +o"
    );

    sendAll(memberSocketFd, "MODE #general +it-k\r\n");

    const std::string expectedSignFlip =
        ":" + memberPrefix + " MODE #general +it-k\r\n";

    expectEqual(
        receiveAvailableData(memberSocketFd, 500),
        expectedSignFlip,
        "Phase 17: +it-k should add i and t then remove the key"
    );
    expectEqual(
        receiveAvailableData(operatorSocketFd, 500),
        expectedSignFlip,
        "Phase 17: +it-k should notify every channel member"
    );

    expectEqual(
        sendLineAndReceive(operatorSocketFd, "MODE #general\r\n"),
        ":irc.42.local 324 operador #general +itl 10\r\n",
        "Phase 17: +it-k should leave invite-only, topic restriction and the limit"
    );

    closeSocket(operatorSocketFd);
    closeSocket(memberSocketFd);
}

int main()
{
    testCombinedModeFlags();
    testCombinedModeArgumentsAndSignChange();
    testCombinedKolParameterOrder();

    return finishSuite("phase 17 MODE combinations");
}
