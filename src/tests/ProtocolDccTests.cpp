// Copyright 2026 @esettes, @danielfdez17
#include "ProtocolTestHarness.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
    const char CTCP_MARKER = '\x01';

    std::string wrapCtcp(const std::string &payload)
    {
        return std::string(1, CTCP_MARKER) + payload + std::string(1, CTCP_MARKER);
    }

    std::string formatDccAddress(in_addr_t networkAddress)
    {
        std::ostringstream stream;

        stream << static_cast<unsigned long>(::ntohl(networkAddress));
        return stream.str();
    }

    int createLoopbackListener(int &port)
    {
        const int listenFd = ::socket(AF_INET, SOCK_STREAM, 0);

        if (listenFd == -1)
            return -1;

        int reuseAddress = 1;

        ::setsockopt(
            listenFd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuseAddress,
            sizeof(reuseAddress)
        );

        struct sockaddr_in address;
        std::memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(0);

        if (::bind(
                listenFd,
                reinterpret_cast<const struct sockaddr *>(&address),
                sizeof(address)
            ) == -1)
        {
            ::close(listenFd);
            return -1;
        }

        if (::listen(listenFd, 1) == -1)
        {
            ::close(listenFd);
            return -1;
        }

        socklen_t addressLength = sizeof(address);

        if (::getsockname(
                listenFd,
                reinterpret_cast<struct sockaddr *>(&address),
                &addressLength
            ) == -1)
        {
            ::close(listenFd);
            return -1;
        }

        port = static_cast<int>(ntohs(address.sin_port));
        return listenFd;
    }

    bool waitForReadable(int socketFd, int timeoutMilliseconds)
    {
        struct pollfd descriptor;

        descriptor.fd = socketFd;
        descriptor.events = POLLIN;
        descriptor.revents = 0;

        const int pollResult = ::poll(&descriptor, 1, timeoutMilliseconds);

        return pollResult > 0 && (descriptor.revents & POLLIN) != 0;
    }

    int acceptWithTimeout(int listenFd, int timeoutMilliseconds)
    {
        if (!waitForReadable(listenFd, timeoutMilliseconds))
            return -1;

        return ::accept(listenFd, NULL, NULL);
    }

    int connectToLoopback(int port)
    {
        const int socketFd = ::socket(AF_INET, SOCK_STREAM, 0);

        if (socketFd == -1)
            return -1;

        struct sockaddr_in address;
        std::memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(static_cast<unsigned short>(port));

        if (::connect(
                socketFd,
                reinterpret_cast<const struct sockaddr *>(&address),
                sizeof(address)
            ) == -1)
        {
            ::close(socketFd);
            return -1;
        }

        return socketFd;
    }

    bool sendAllBytes(int socketFd, const std::string &data)
    {
        std::size_t sentByteCount = 0;

        while (sentByteCount < data.size())
        {
            const ssize_t result = ::send(
                socketFd,
                data.data() + sentByteCount,
                data.size() - sentByteCount,
                0
            );

            if (result > 0)
            {
                sentByteCount += static_cast<std::size_t>(result);
                continue;
            }

            if (result == -1 && errno == EINTR)
                continue;

            return false;
        }

        return true;
    }

    std::string receiveExactBytes(
        int socketFd,
        std::size_t expectedByteCount,
        int timeoutMilliseconds
    )
    {
        std::string received;
        char buffer[256];

        while (received.size() < expectedByteCount)
        {
            if (!waitForReadable(socketFd, timeoutMilliseconds))
                break;

            const ssize_t result = ::recv(
                socketFd,
                buffer,
                sizeof(buffer),
                0
            );

            if (result <= 0)
                break;

            received.append(buffer, static_cast<std::size_t>(result));
            timeoutMilliseconds = 200;
        }

        return received;
    }

    bool connectTwoRegisteredClients(
        TestServerProcess &server,
        int &aliceSocketFd,
        int &bobSocketFd,
        std::string &alicePrefix,
        std::string &bobPrefix,
        const std::string &testName
    )
    {
        if (!startServerOrFail(server, testName))
            return false;

        aliceSocketFd = connectToServer(server.getPort());
        bobSocketFd = connectToServer(server.getPort());

        if (aliceSocketFd == -1 || bobSocketFd == -1)
        {
            reportFailure(
                testName,
                "successful connections",
                "connection failed"
            );
            closeSocket(aliceSocketFd);
            closeSocket(bobSocketFd);
            aliceSocketFd = -1;
            bobSocketFd = -1;
            return false;
        }

        const std::string aliceWelcome = registerClient(aliceSocketFd, "alice");
        const std::string bobWelcome = registerClient(bobSocketFd, "bob");

        if (!extractClientPrefixFromWelcome(aliceWelcome, "alice", alicePrefix)
            || !extractClientPrefixFromWelcome(bobWelcome, "bob", bobPrefix))
        {
            reportFailure(
                testName,
                "welcome containing alice and bob prefixes",
                aliceWelcome + bobWelcome
            );
            closeSocket(aliceSocketFd);
            closeSocket(bobSocketFd);
            aliceSocketFd = -1;
            bobSocketFd = -1;
            return false;
        }

        return true;
    }
}

/**
 * @brief Bonus — a CTCP DCC SEND PRIVMSG is forwarded verbatim to the
 * target nickname, including SOH delimiters and the space-separated DCC
 * arguments. The server must not split or reinterpret the trailing text.
 */
static void testDccSendCtcpIsRelayedExactly()
{
    TestServerProcess server;
    int aliceSocketFd = -1;
    int bobSocketFd = -1;
    std::string alicePrefix;
    std::string bobPrefix;

    if (!connectTwoRegisteredClients(
            server,
            aliceSocketFd,
            bobSocketFd,
            alicePrefix,
            bobPrefix,
            "Server should start for DCC SEND relay tests"
        ))
    {
        return;
    }

    (void)bobPrefix;

    const std::string dccPayload =
        wrapCtcp("DCC SEND example.txt 2130706433 5000 1200");

    sendAll(aliceSocketFd, "PRIVMSG bob :" + dccPayload + "\r\n");

    expectEqual(
        receiveAvailableData(bobSocketFd, 500),
        ":" + alicePrefix + " PRIVMSG bob :" + dccPayload + "\r\n",
        "DCC: PRIVMSG CTCP DCC SEND should reach the recipient unchanged"
    );
    expectEqual(
        receiveAvailableData(aliceSocketFd, 200),
        "",
        "DCC: DCC SEND should not be echoed to the sender"
    );

    closeSocket(aliceSocketFd);
    closeSocket(bobSocketFd);
}

/**
 * @brief Bonus — quoted DCC filenames keep their spaces because they live
 * entirely in the trailing parameter.
 */
static void testDccSendQuotedFilenameIsPreserved()
{
    TestServerProcess server;
    int aliceSocketFd = -1;
    int bobSocketFd = -1;
    std::string alicePrefix;
    std::string bobPrefix;

    if (!connectTwoRegisteredClients(
            server,
            aliceSocketFd,
            bobSocketFd,
            alicePrefix,
            bobPrefix,
            "Server should start for DCC quoted filename tests"
        ))
    {
        return;
    }

    (void)bobPrefix;

    const std::string dccPayload =
        wrapCtcp("DCC SEND \"my file.txt\" 2130706433 5000 12");

    sendAll(aliceSocketFd, "PRIVMSG bob :" + dccPayload + "\r\n");

    expectEqual(
        receiveAvailableData(bobSocketFd, 500),
        ":" + alicePrefix + " PRIVMSG bob :" + dccPayload + "\r\n",
        "DCC: quoted filenames with spaces must stay inside the CTCP payload"
    );

    closeSocket(aliceSocketFd);
    closeSocket(bobSocketFd);
}

/**
 * @brief Bonus — DCC CHAT uses the same CTCP PRIVMSG path as DCC SEND.
 */
static void testDccChatCtcpIsRelayedExactly()
{
    TestServerProcess server;
    int aliceSocketFd = -1;
    int bobSocketFd = -1;
    std::string alicePrefix;
    std::string bobPrefix;

    if (!connectTwoRegisteredClients(
            server,
            aliceSocketFd,
            bobSocketFd,
            alicePrefix,
            bobPrefix,
            "Server should start for DCC CHAT relay tests"
        ))
    {
        return;
    }

    (void)bobPrefix;

    const std::string dccPayload =
        wrapCtcp("DCC CHAT chat 2130706433 5001");

    sendAll(aliceSocketFd, "PRIVMSG BOB :" + dccPayload + "\r\n");

    expectEqual(
        receiveAvailableData(bobSocketFd, 500),
        ":" + alicePrefix + " PRIVMSG BOB :" + dccPayload + "\r\n",
        "DCC: DCC CHAT should find nicknames with IRC casemapping"
    );

    const int carolSocketFd = connectToServer(server.getPort());

    if (carolSocketFd == -1)
    {
        reportFailure(
            "Unrelated client should connect for DCC isolation tests",
            "successful connection",
            "connection failed"
        );
        closeSocket(aliceSocketFd);
        closeSocket(bobSocketFd);
        return;
    }

    registerClient(carolSocketFd, "carol");
    sendAll(aliceSocketFd, "PRIVMSG bob :" + dccPayload + "\r\n");
    receiveAvailableData(bobSocketFd, 500);

    expectEqual(
        receiveAvailableData(carolSocketFd, 200),
        "",
        "DCC: a DCC handshake must not reach unrelated clients"
    );

    closeSocket(aliceSocketFd);
    closeSocket(bobSocketFd);
    closeSocket(carolSocketFd);
}

/**
 * @brief Bonus — a DCC SEND split across several TCP writes is reassembled
 * before the CTCP is forwarded, matching the framing rule of the subject.
 */
static void testFragmentedDccSendIsReassembled()
{
    TestServerProcess server;
    int aliceSocketFd = -1;
    int bobSocketFd = -1;
    std::string alicePrefix;
    std::string bobPrefix;

    if (!connectTwoRegisteredClients(
            server,
            aliceSocketFd,
            bobSocketFd,
            alicePrefix,
            bobPrefix,
            "Server should start for fragmented DCC tests"
        ))
    {
        return;
    }

    (void)bobPrefix;

    const std::string dccPayload =
        wrapCtcp("DCC SEND example.txt 2130706433 5000 5");

    sendAll(aliceSocketFd, "PRIVMSG bob :");
    sendAll(aliceSocketFd, dccPayload.substr(0, 8));
    sendAll(aliceSocketFd, dccPayload.substr(8));
    sendAll(aliceSocketFd, "\r\n");

    expectEqual(
        receiveAvailableData(bobSocketFd, 500),
        ":" + alicePrefix + " PRIVMSG bob :" + dccPayload + "\r\n",
        "DCC: a fragmented DCC SEND must be rebuilt before delivery"
    );

    closeSocket(aliceSocketFd);
    closeSocket(bobSocketFd);
}

/**
 * @brief Bonus — DCC to an unknown nick still uses PRIVMSG errors; NOTICE
 * CTCP replies such as DCC REJECT stay silent when the target is missing.
 */
static void testDccErrorsAndNoticeReject()
{
    TestServerProcess server;
    int aliceSocketFd = -1;
    int bobSocketFd = -1;
    std::string alicePrefix;
    std::string bobPrefix;

    if (!connectTwoRegisteredClients(
            server,
            aliceSocketFd,
            bobSocketFd,
            alicePrefix,
            bobPrefix,
            "Server should start for DCC error tests"
        ))
    {
        return;
    }

    (void)alicePrefix;
    (void)bobPrefix;

    const std::string dccPayload =
        wrapCtcp("DCC SEND missing.txt 2130706433 5000 1");

    expectEqual(
        sendLineAndReceive(
            aliceSocketFd,
            "PRIVMSG nobody :" + dccPayload + "\r\n"
        ),
        ":irc.42.local 401 alice nobody :No such nick\r\n",
        "DCC: PRIVMSG DCC SEND to an unknown nick should return 401"
    );

    const std::string rejectPayload =
        wrapCtcp("DCC REJECT SEND example.txt");

    expectEqual(
        sendLineAndReceive(
            aliceSocketFd,
            "NOTICE nobody :" + rejectPayload + "\r\n"
        ),
        "",
        "DCC: NOTICE DCC REJECT to an unknown nick should stay silent"
    );

    closeSocket(aliceSocketFd);
    closeSocket(bobSocketFd);
}

/**
 * @brief Bonus — NOTICE delivers DCC REJECT to the original sender without
 * automatic errors, which is how clients abort a file offer.
 */
static void testNoticeRelaysDccReject()
{
    TestServerProcess server;
    int aliceSocketFd = -1;
    int bobSocketFd = -1;
    std::string alicePrefix;
    std::string bobPrefix;

    if (!connectTwoRegisteredClients(
            server,
            aliceSocketFd,
            bobSocketFd,
            alicePrefix,
            bobPrefix,
            "Server should start for NOTICE DCC REJECT tests"
        ))
    {
        return;
    }

    (void)alicePrefix;

    const std::string rejectPayload =
        wrapCtcp("DCC REJECT SEND example.txt");

    sendAll(bobSocketFd, "NOTICE alice :" + rejectPayload + "\r\n");

    expectEqual(
        receiveAvailableData(aliceSocketFd, 500),
        ":" + bobPrefix + " NOTICE alice :" + rejectPayload + "\r\n",
        "DCC: NOTICE DCC REJECT should reach the original sender"
    );
    expectEqual(
        receiveAvailableData(bobSocketFd, 200),
        "",
        "DCC: NOTICE DCC REJECT should not echo to the sender"
    );

    closeSocket(aliceSocketFd);
    closeSocket(bobSocketFd);
}

/**
 * @brief Bonus — after the server relays DCC SEND, the receiver can connect
 * to the advertised TCP port and read the file bytes from the sender. The
 * IRC server never carries those bytes.
 */
static void testDccHandshakeAllowsPeerToPeerFileBytes()
{
    TestServerProcess server;
    int aliceSocketFd = -1;
    int bobSocketFd = -1;
    std::string alicePrefix;
    std::string bobPrefix;

    if (!connectTwoRegisteredClients(
            server,
            aliceSocketFd,
            bobSocketFd,
            alicePrefix,
            bobPrefix,
            "Server should start for DCC peer-to-peer file tests"
        ))
    {
        return;
    }

    (void)bobPrefix;

    int dccPort = 0;
    const int listenFd = createLoopbackListener(dccPort);

    if (listenFd == -1 || dccPort <= 0)
    {
        reportFailure(
            "DCC: sender should open a file socket",
            "listening TCP port",
            "bind/listen failed"
        );
        closeSocket(aliceSocketFd);
        closeSocket(bobSocketFd);
        return;
    }

    const std::string fileContents = "hello";
    std::ostringstream portStream;

    portStream << dccPort;

    const std::string dccPayload = wrapCtcp(
        "DCC SEND hello.txt "
        + formatDccAddress(htonl(INADDR_LOOPBACK))
        + " "
        + portStream.str()
        + " 5"
    );

    sendAll(aliceSocketFd, "PRIVMSG bob :" + dccPayload + "\r\n");

    const std::string relayedMessage = receiveAvailableData(bobSocketFd, 500);
    const std::string expectedRelay =
        ":" + alicePrefix + " PRIVMSG bob :" + dccPayload + "\r\n";

    expectEqual(
        relayedMessage,
        expectedRelay,
        "DCC: the receiver must get the listening address and port unchanged"
    );

    if (relayedMessage != expectedRelay)
    {
        ::close(listenFd);
        closeSocket(aliceSocketFd);
        closeSocket(bobSocketFd);
        return;
    }

    const int receiverFd = connectToLoopback(dccPort);
    const int senderFd = acceptWithTimeout(listenFd, 1000);

    if (receiverFd == -1 || senderFd == -1)
    {
        reportFailure(
            "DCC: receiver should connect to the advertised file port",
            "accepted DCC TCP connection",
            "connect/accept failed"
        );
        closeSocket(receiverFd);
        closeSocket(senderFd);
        ::close(listenFd);
        closeSocket(aliceSocketFd);
        closeSocket(bobSocketFd);
        return;
    }

    expectTrue(
        sendAllBytes(senderFd, fileContents),
        "DCC: sender should write file bytes on the DCC socket",
        "successful send",
        "send failed"
    );
    expectEqual(
        receiveExactBytes(receiverFd, fileContents.size(), 1000),
        fileContents,
        "DCC: file bytes should travel directly between clients"
    );

    closeSocket(receiverFd);
    closeSocket(senderFd);
    ::close(listenFd);
    closeSocket(aliceSocketFd);
    closeSocket(bobSocketFd);
}

/**
 * @brief Bonus — channel CTCP (for example ACTION) is still a PRIVMSG and
 * must keep the SOH markers when forwarded to other members.
 */
static void testChannelCtcpIsRelayedToMembers()
{
    TestServerProcess server;
    int aliceSocketFd = -1;
    int bobSocketFd = -1;
    std::string alicePrefix;
    std::string bobPrefix;

    if (!connectTwoRegisteredClients(
            server,
            aliceSocketFd,
            bobSocketFd,
            alicePrefix,
            bobPrefix,
            "Server should start for channel CTCP tests"
        ))
    {
        return;
    }

    (void)bobPrefix;

    sendAll(aliceSocketFd, "JOIN #files\r\n");
    discardPendingData(aliceSocketFd);
    sendAll(bobSocketFd, "JOIN #files\r\n");
    discardPendingData(aliceSocketFd);
    discardPendingData(bobSocketFd);

    const std::string actionPayload = wrapCtcp("ACTION envia un archivo");

    sendAll(aliceSocketFd, "PRIVMSG #files :" + actionPayload + "\r\n");

    expectEqual(
        receiveAvailableData(bobSocketFd, 500),
        ":" + alicePrefix + " PRIVMSG #files :" + actionPayload + "\r\n",
        "DCC: channel CTCP payloads must keep their SOH delimiters"
    );
    expectEqual(
        receiveAvailableData(aliceSocketFd, 200),
        "",
        "DCC: channel CTCP must not echo to the sender"
    );

    closeSocket(aliceSocketFd);
    closeSocket(bobSocketFd);
}

int main()
{
    testDccSendCtcpIsRelayedExactly();
    testDccSendQuotedFilenameIsPreserved();
    testDccChatCtcpIsRelayedExactly();
    testFragmentedDccSendIsReassembled();
    testDccErrorsAndNoticeReject();
    testNoticeRelaysDccReject();
    testDccHandshakeAllowsPeerToPeerFileBytes();
    testChannelCtcpIsRelayedToMembers();

    return finishSuite("DCC file-transfer bonus");
}
