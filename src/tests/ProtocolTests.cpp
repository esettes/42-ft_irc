#include "Client.hpp"
#include "IrcCasemap.hpp"
#include "IrcMessage.hpp"
#include "NumericReplies.hpp"
#include "Console.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

static int g_failures = 0;

static std::string escapeOutput(const std::string &value)
{
    std::string escaped;

    for (std::size_t index = 0; index < value.size(); ++index)
    {
        const unsigned char character =
            static_cast<unsigned char>(value[index]);

        if (character == '\r')
            escaped += "\\r";
        else if (character == '\n')
            escaped += "\\n";
        else if (character == '\0')
            escaped += "\\0";
        else
            escaped += static_cast<char>(character);
    }

    return escaped;
}

static void reportFailure(
    const std::string &testName,
    const std::string &expected,
    const std::string &actual
)
{
    std::cerr << Console::ERROR << testName << std::endl;
    std::cerr << "  expected: " << escapeOutput(expected) << std::endl;
    std::cerr << "  actual  : " << escapeOutput(actual) << std::endl;
    ++g_failures;
}

static void expectTrue(
    bool condition,
    const std::string &testName,
    const std::string &expected,
    const std::string &actual
)
{
    if (!condition)
        reportFailure(testName, expected, actual);
}

static void expectEqual(
    const std::string &actual,
    const std::string &expected,
    const std::string &testName
)
{
    if (actual != expected)
        reportFailure(testName, expected, actual);
}

static void expectContains(
    const std::string &actual,
    const std::string &expectedFragment,
    const std::string &testName
)
{
    if (actual.find(expectedFragment) == std::string::npos)
    {
        reportFailure(
            testName,
            "output containing: " + expectedFragment,
            actual
        );
    }
}

static void expectSerializationFailure(
    const IrcMessage &message,
    const std::string &testName
)
{
    try
    {
        const std::string serializedMessage = message.serialize();

        reportFailure(
            testName,
            "std::runtime_error",
            serializedMessage
        );
    }
    catch (const std::runtime_error &)
    {
    }
}

static int findAvailablePort()
{
    const int socketFd = ::socket(AF_INET, SOCK_STREAM, 0);

    if (socketFd == -1)
        return -1;

    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(0);

    if (::bind(
            socketFd,
            reinterpret_cast<const struct sockaddr *>(&address),
            sizeof(address)
        ) == -1)
    {
        ::close(socketFd);
        return -1;
    }

    socklen_t addressLength = sizeof(address);

    if (::getsockname(
            socketFd,
            reinterpret_cast<struct sockaddr *>(&address),
            &addressLength
        ) == -1)
    {
        ::close(socketFd);
        return -1;
    }

    const int availablePort = ntohs(address.sin_port);

    ::close(socketFd);
    return availablePort;
}

static int connectToServer(int port)
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

static bool sendAll(int socketFd, const std::string &data)
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

static std::string receiveAvailableData(
    int socketFd,
    int timeoutMilliseconds
)
{
    std::string response;
    char buffer[4096];

    while (true)
    {
        struct pollfd descriptor;

        descriptor.fd = socketFd;
        descriptor.events = POLLIN;
        descriptor.revents = 0;

        const int pollResult = ::poll(
            &descriptor,
            1,
            timeoutMilliseconds
        );

        if (pollResult <= 0)
            break;

        if (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL))
        {
            const ssize_t receivedByteCount = ::recv(
                socketFd,
                buffer,
                sizeof(buffer),
                0
            );

            if (receivedByteCount > 0)
            {
                response.append(
                    buffer,
                    static_cast<std::size_t>(receivedByteCount)
                );
            }

            break;
        }

        if (!(descriptor.revents & POLLIN))
            break;

        const ssize_t receivedByteCount = ::recv(
            socketFd,
            buffer,
            sizeof(buffer),
            0
        );

        if (receivedByteCount <= 0)
            break;

        response.append(
            buffer,
            static_cast<std::size_t>(receivedByteCount)
        );

        timeoutMilliseconds = 50;
    }

    return response;
}

/**
 * @brief Waits for the remote peer to close a connected socket and returns
 * true when EOF, a hangup, or a connection reset is detected.
 */
static bool waitForSocketClosure(
    int socketFd,
    int timeoutMilliseconds
)
{
    struct pollfd descriptor;

    descriptor.fd = socketFd;
    descriptor.events = POLLIN;
    descriptor.revents = 0;

    int pollResult;

    do
    {
        pollResult = ::poll(
            &descriptor,
            1,
            timeoutMilliseconds
        );
    }
    while (pollResult == -1 && errno == EINTR);

    if (pollResult <= 0)
        return false;

    if (descriptor.revents & POLLNVAL)
        return false;

    if (descriptor.revents & (POLLHUP | POLLERR))
        return true;

    if (!(descriptor.revents & POLLIN))
        return false;

    char receivedByte;
    ssize_t receivedByteCount;

    do
    {
        receivedByteCount = ::recv(
            socketFd,
            &receivedByte,
            sizeof(receivedByte),
            0
        );
    }
    while (receivedByteCount == -1 && errno == EINTR);

    if (receivedByteCount == 0)
        return true;

    if (receivedByteCount == -1
        && (errno == ECONNRESET || errno == ENOTCONN))
    {
        return true;
    }

    return false;
}

class TestServerProcess
{
    private:
        pid_t processId;
        int port;

        TestServerProcess(const TestServerProcess &other);
        TestServerProcess &operator=(const TestServerProcess &other);

        bool waitUntilReady()
        {
            for (int attempt = 0; attempt < 100; ++attempt)
            {
                const int probeSocket = connectToServer(port);

                if (probeSocket != -1)
                {
                    ::close(probeSocket);
                    return true;
                }

                if (!isRunning())
                    return false;

                ::usleep(20000);
            }

            return false;
        }

    public:
        TestServerProcess()
            : processId(-1),
              port(findAvailablePort())
        {
        }

        ~TestServerProcess()
        {
            stop();
        }

        bool start()
        {
            if (port == -1)
                return false;

            processId = ::fork();

            if (processId == -1)
                return false;

            if (processId == 0)
            {
                const int nullFd = ::open("/dev/null", O_WRONLY);

                if (nullFd != -1)
                {
                    ::dup2(nullFd, STDOUT_FILENO);
                    ::dup2(nullFd, STDERR_FILENO);
                    ::close(nullFd);
                }

                std::ostringstream portStream;
                portStream << port;

                ::execl(
                    "./ircserv",
                    "./ircserv",
                    portStream.str().c_str(),
                    "secret",
                    static_cast<char *>(NULL)
                );

                ::_exit(EXIT_FAILURE);
            }

            return waitUntilReady();
        }

        void stop()
        {
            if (processId <= 0)
                return;

            ::kill(processId, SIGINT);

            for (int attempt = 0; attempt < 100; ++attempt)
            {
                int status = 0;
                const pid_t result = ::waitpid(
                    processId,
                    &status,
                    WNOHANG
                );

                if (result == processId)
                {
                    processId = -1;
                    return;
                }

                ::usleep(10000);
            }

            ::kill(processId, SIGKILL);
            ::waitpid(processId, NULL, 0);
            processId = -1;
        }

        bool isRunning()
        {
            if (processId <= 0)
                return false;

            int status = 0;
            const pid_t result = ::waitpid(
                processId,
                &status,
                WNOHANG
            );

            if (result == 0)
                return true;

            if (result == processId)
                processId = -1;

            return false;
        }

        int getPort() const
        {
            return port;
        }
};

static std::string sendCommandAndReceive(
    int port,
    const std::string &command
)
{
    const int socketFd = connectToServer(port);

    if (socketFd == -1)
        return "connection failed";

    if (!sendAll(socketFd, command))
    {
        ::close(socketFd);
        return "send failed";
    }

    const std::string response =
        receiveAvailableData(socketFd, 500);

    ::close(socketFd);
    return response;
}

static std::string registerClient(
    int socketFd,
    const std::string &nickname
)
{
    const std::string registration =
        "PASS secret\r\n"
        "NICK " + nickname + "\r\n"
        "USER " + nickname + " 0 * :" + nickname + "\r\n";

    if (!sendAll(socketFd, registration))
        return "send failed";

    return receiveAvailableData(socketFd, 500);
}

static void testIrcMessageSerialization()
{
    std::vector<std::string> parameters;

    parameters.push_back("#general");
    parameters.push_back("Hello world");

    const IrcMessage message(
        "PRIVMSG",
        parameters,
        "nick!user@host",
        true
    );

    expectEqual(
        message.serialize(),
        ":nick!user@host PRIVMSG #general :Hello world\r\n",
        "IrcMessage should serialize prefix, command, parameters, trailing and CRLF"
    );
}

static void testGeneratedMessagesRejectControlCharacters()
{
    std::vector<std::string> parameters;

    parameters.push_back("bad\rvalue");

    expectSerializationFailure(
        IrcMessage("NOTICE", parameters, "", true),
        "IrcMessage should reject carriage returns"
    );

    parameters.clear();
    parameters.push_back("bad\nvalue");

    expectSerializationFailure(
        IrcMessage("NOTICE", parameters, "", true),
        "IrcMessage should reject line feeds"
    );

    parameters.clear();
    parameters.push_back(std::string("bad\0value", 9));

    expectSerializationFailure(
        IrcMessage("NOTICE", parameters, "", true),
        "IrcMessage should reject NUL bytes"
    );
}

static void testSerializedMessageLengthLimit()
{
    std::vector<std::string> parameters;

    parameters.push_back(std::string(502, 'A'));

    const IrcMessage maximumLengthMessage(
        "NOTICE",
        parameters,
        "",
        true
    );

    const std::string serializedMessage =
        maximumLengthMessage.serialize();

    expectTrue(
        serializedMessage.size() == IRC_MAX_MESSAGE_LENGTH,
        "IrcMessage should allow exactly 512 bytes",
        "serialized size equal to 512",
        "serialized size equal to requested limit"
    );

    parameters.clear();
    parameters.push_back(std::string(503, 'A'));

    expectSerializationFailure(
        IrcMessage("NOTICE", parameters, "", true),
        "IrcMessage should reject messages longer than 512 bytes"
    );
}

static void testNumericCodeFormatting()
{
    const int requiredCodes[] = {
        1, 2, 3, 4, 5,
        324, 331, 332, 341, 353, 366,
        401, 403, 404, 409, 411, 412, 421,
        431, 432, 433, 441, 442, 443, 451,
        461, 462, 464, 471, 472, 473, 475, 482
    };

    const std::size_t codeCount =
        sizeof(requiredCodes) / sizeof(requiredCodes[0]);

    for (std::size_t index = 0; index < codeCount; ++index)
    {
        const std::string formattedCode =
            NumericReply::formatCode(requiredCodes[index]);

        std::ostringstream expectedCode;
        expectedCode.width(3);
        expectedCode.fill('0');
        expectedCode << requiredCodes[index];

        expectEqual(
            formattedCode,
            expectedCode.str(),
            "Numeric reply code should always contain three digits"
        );
    }
}

static void testRfc1459Casemapping()
{
    expectEqual(
        IrcCasemap::normalize("Roxana"),
        "roxana",
        "Casemapping should normalize ASCII letters"
    );

    expectTrue(
        IrcCasemap::equal("Nick[Test]", "nick{test}"),
        "Casemapping should treat brackets and braces as equivalent",
        "equivalent values",
        "different values"
    );

    expectTrue(
        IrcCasemap::equal("Nick\\Path", "nick|path"),
        "Casemapping should treat backslash and pipe as equivalent",
        "equivalent values",
        "different values"
    );

    expectTrue(
        IrcCasemap::equal("Nick~Name", "nick^name"),
        "RFC1459 casemapping should treat tilde and caret as equivalent",
        "equivalent values",
        "different values"
    );
}

static void testClientLineLengthBoundaries()
{
    Client exactLengthClient(-1, "localhost");

    const std::string exactLengthLine =
        "PASS " + std::string(505, 'A') + "\r\n";

    exactLengthClient.appendToInputBuffer(exactLengthLine);

    std::string completeLine;

    expectTrue(
        exactLengthClient.extractNextLine(completeLine)
            == Client::LINE_COMPLETE,
        "Client should accept a CRLF line of exactly 512 bytes",
        "LINE_COMPLETE",
        "status different from LINE_COMPLETE"
    );

    Client excessiveLengthClient(-1, "localhost");

    const std::string excessiveLengthLine =
        "PASS " + std::string(506, 'A') + "\r\n";

    excessiveLengthClient.appendToInputBuffer(
        excessiveLengthLine
    );

    expectTrue(
        excessiveLengthClient.extractNextLine(completeLine)
            == Client::LINE_TOO_LONG,
        "Client should reject a CRLF line longer than 512 bytes",
        "LINE_TOO_LONG",
        "status different from LINE_TOO_LONG"
    );
}

static void testFragmentedLfBoundary()
{
    Client client(-1, "localhost");
    std::string completeLine;

    client.appendToInputBuffer(
        "PASS " + std::string(506, 'A')
    );

    expectTrue(
        client.extractNextLine(completeLine)
            == Client::LINE_INCOMPLETE,
        "A fragmented 512-byte LF line should remain incomplete before LF arrives",
        "LINE_INCOMPLETE",
        "LINE_TOO_LONG"
    );

    client.appendToInputBuffer("\n");

    expectTrue(
        client.extractNextLine(completeLine)
            == Client::LINE_COMPLETE,
        "A fragmented LF line of exactly 512 bytes should be accepted",
        "LINE_COMPLETE",
        "status different from LINE_COMPLETE"
    );
}

static void testServerPrefixAndUnknownCommand()
{
    TestServerProcess server;

    if (!server.start())
    {
        reportFailure(
            "Server should start for protocol tests",
            "running ircserv",
            "server startup failed"
        );
        return;
    }

    const std::string response = sendCommandAndReceive(
        server.getPort(),
        "UNKNOWN\r\n"
    );

    expectEqual(
        response,
        ":irc.42.local 421 * UNKNOWN :Unknown command\r\n",
        "Unknown commands should return 421 with server prefix and target *"
    );
}

static void testWelcomeNumerics()
{
    TestServerProcess server;

    if (!server.start())
    {
        reportFailure(
            "Server should start for welcome tests",
            "running ircserv",
            "server startup failed"
        );
        return;
    }

    const int socketFd = connectToServer(server.getPort());

    if (socketFd == -1)
    {
        reportFailure(
            "Client should connect for welcome tests",
            "successful connection",
            "connection failed"
        );
        return;
    }

    const std::string response =
        registerClient(socketFd, "roxana");

    expectContains(
        response,
        ":irc.42.local 001 roxana :Welcome to the IRC Network roxana!roxana@",
        "Welcome numeric should contain the server and complete client prefix"
    );

    expectContains(
        response,
        ":irc.42.local 002 roxana ",
        "Registration should send numeric 002"
    );

    expectContains(
        response,
        ":irc.42.local 003 roxana ",
        "Registration should send numeric 003"
    );

    expectContains(
        response,
        ":irc.42.local 004 roxana ",
        "Registration should send numeric 004"
    );

    expectContains(
        response,
        ":irc.42.local 005 roxana ",
        "Registration should send numeric 005"
    );

    ::close(socketFd);
}

/**
 * This function tests:
 * 
 * PASS without parameter → 461.
 * Incorrect password → 464.
 * An incorrect password resets passwordAccepted to false.
 * Without an accepted password, 001 does not appear.
 * PASS after registration → 462.
 */
static void testPassRegistrationRules()
{
    TestServerProcess server;

    if (!server.start())
    {
        reportFailure(
            "Server should start for PASS tests",
            "running ircserv",
            "server startup failed"
        );
        return;
    }

    expectEqual(
        sendCommandAndReceive(
            server.getPort(),
            "PASS\r\n"
        ),
        ":irc.42.local 461 * PASS :Not enough parameters\r\n",
        "PASS without a password should return 461"
    );

    expectEqual(
        sendCommandAndReceive(
            server.getPort(),
            "PASS incorrect\r\n"
        ),
        ":irc.42.local 464 * :Password incorrect\r\n",
        "An incorrect password should return 464"
    );

    const int resetPasswordSocketFd =
        connectToServer(server.getPort());

    if (resetPasswordSocketFd == -1)
    {
        reportFailure(
            "Client should connect for password reset test",
            "successful connection",
            "connection failed"
        );
        return;
    }

    const std::string passwordResetCommands =
        "PASS secret\r\n"
        "PASS incorrect\r\n"
        "NICK resetpass\r\n"
        "USER resetpass 0 * :Reset Pass\r\n";

    if (!sendAll(resetPasswordSocketFd, passwordResetCommands))
    {
        reportFailure(
            "Password reset commands should be sent",
            "successful send",
            "send failed"
        );
        ::close(resetPasswordSocketFd);
        return;
    }

    const std::string passwordResetResponse =
        receiveAvailableData(resetPasswordSocketFd, 500);

    expectContains(
        passwordResetResponse,
        ":irc.42.local 464 * :Password incorrect\r\n",
        "An incorrect PASS should reject the supplied password"
    );

    expectTrue(
        passwordResetResponse.find(" 001 ") == std::string::npos,
        "An incorrect PASS after a correct PASS should prevent registration",
        "output without numeric 001",
        passwordResetResponse
    );

    ::close(resetPasswordSocketFd);

    const int registeredClientSocketFd =
        connectToServer(server.getPort());

    if (registeredClientSocketFd == -1)
    {
        reportFailure(
            "Client should connect for repeated PASS test",
            "successful connection",
            "connection failed"
        );
        return;
    }

    registerClient(registeredClientSocketFd, "passclient");

    if (!sendAll(registeredClientSocketFd, "PASS secret\r\n"))
    {
        reportFailure(
            "Repeated PASS command should be sent",
            "successful send",
            "send failed"
        );
        ::close(registeredClientSocketFd);
        return;
    }

    expectEqual(
        receiveAvailableData(registeredClientSocketFd, 500),
        ":irc.42.local 462 passclient :You may not reregister\r\n",
        "PASS after registration should return 462"
    );

    ::close(registeredClientSocketFd);
}

static void testSpecificMissingParameterNumerics()
{
    TestServerProcess server;

    if (!server.start())
    {
        reportFailure(
            "Server should start for dispatcher tests",
            "running ircserv",
            "server startup failed"
        );
        return;
    }

    expectEqual(
        sendCommandAndReceive(
            server.getPort(),
            "NICK\r\n"
        ),
        ":irc.42.local 431 * :No nickname given\r\n",
        "NICK without a parameter should return 431"
    );

    expectEqual(
        sendCommandAndReceive(
            server.getPort(),
            "PING\r\n"
        ),
        ":irc.42.local 409 * :No origin specified\r\n",
        "PING without a token should return 409"
    );

    expectEqual(
        sendCommandAndReceive(
            server.getPort(),
            "PRIVMSG target :hello\r\n"
        ),
        ":irc.42.local 451 * :You have not registered\r\n",
        "PRIVMSG before registration should return 451"
    );
}

/**
 * @brief Extracts the nick!user@host identity embedded in a welcome (001)
 * reply so later assertions can compare full PRIVMSG prefixes.
 */
static bool extractClientPrefixFromWelcome(
    const std::string &welcomeResponse,
    const std::string &nickname,
    std::string &clientPrefix
)
{
    const std::string prefixMarker = nickname + "!" + nickname + "@";
    const std::string::size_type prefixStart =
        welcomeResponse.find(prefixMarker);

    if (prefixStart == std::string::npos)
        return false;

    const std::string::size_type prefixEnd =
        welcomeResponse.find("\r\n", prefixStart);

    if (prefixEnd == std::string::npos)
        return false;

    clientPrefix = welcomeResponse.substr(
        prefixStart,
        prefixEnd - prefixStart
    );
    return true;
}

/**
 * @brief Phase 13 — verifies every documented PRIVMSG error path:
 * 451 before registration, 411/412 for missing data, 401/403/404 for
 * unreachable targets, and that invalid commands leave the server usable.
 */
static void testPhase13PrivmsgErrors()
{
    TestServerProcess server;

    if (!server.start())
    {
        reportFailure(
            "Server should start for phase 13 PRIVMSG error tests",
            "running ircserv",
            "server startup failed"
        );
        return;
    }

    expectEqual(
        sendCommandAndReceive(
            server.getPort(),
            "PRIVMSG target :hello\r\n"
        ),
        ":irc.42.local 451 * :You have not registered\r\n",
        "Phase 13: PRIVMSG before registration should return 451"
    );

    const int socketFd = connectToServer(server.getPort());

    if (socketFd == -1)
    {
        reportFailure(
            "Client should connect for phase 13 PRIVMSG error tests",
            "successful connection",
            "connection failed"
        );
        return;
    }

    registerClient(socketFd, "alice");

    sendAll(socketFd, "PRIVMSG\r\n");
    expectEqual(
        receiveAvailableData(socketFd, 500),
        ":irc.42.local 411 alice :No recipient given (PRIVMSG)\r\n",
        "Phase 13: PRIVMSG without a recipient should return 411"
    );

    sendAll(socketFd, "PRIVMSG roxana\r\n");
    expectEqual(
        receiveAvailableData(socketFd, 500),
        ":irc.42.local 412 alice :No text to send\r\n",
        "Phase 13: PRIVMSG without text should return 412"
    );

    sendAll(socketFd, "PRIVMSG roxana :\r\n");
    expectEqual(
        receiveAvailableData(socketFd, 500),
        ":irc.42.local 412 alice :No text to send\r\n",
        "Phase 13: PRIVMSG with empty trailing text should return 412"
    );

    sendAll(socketFd, "PRIVMSG nadie :Hola\r\n");
    expectEqual(
        receiveAvailableData(socketFd, 500),
        ":irc.42.local 401 alice nadie :No such nick\r\n",
        "Phase 13: PRIVMSG to an unknown nickname should return 401"
    );

    sendAll(socketFd, "PRIVMSG #inexistente :Hola\r\n");
    expectEqual(
        receiveAvailableData(socketFd, 500),
        ":irc.42.local 403 alice #inexistente :No such channel\r\n",
        "Phase 13: PRIVMSG to an unknown channel should return 403"
    );

    sendAll(socketFd, "JOIN #general\r\n");
    receiveAvailableData(socketFd, 500);

    const int outsiderSocketFd = connectToServer(server.getPort());

    if (outsiderSocketFd == -1)
    {
        reportFailure(
            "Outsider client should connect for phase 13 PRIVMSG tests",
            "successful connection",
            "connection failed"
        );
        ::close(socketFd);
        return;
    }

    registerClient(outsiderSocketFd, "outsider");
    sendAll(outsiderSocketFd, "PRIVMSG #general :Hola\r\n");
    expectEqual(
        receiveAvailableData(outsiderSocketFd, 500),
        ":irc.42.local 404 outsider #general :Cannot send to channel\r\n",
        "Phase 13: PRIVMSG from a non-member should return 404"
    );

    sendAll(socketFd, "PRIVMSG nadie :still alive\r\n");
    expectEqual(
        receiveAvailableData(socketFd, 500),
        ":irc.42.local 401 alice nadie :No such nick\r\n",
        "Phase 13: server should keep answering after invalid PRIVMSG"
    );

    expectTrue(
        server.isRunning(),
        "Phase 13: invalid PRIVMSG commands should not stop the server",
        "ircserv remains running",
        "ircserv exited"
    );

    ::close(outsiderSocketFd);
    ::close(socketFd);
}

/**
 * @brief Phase 13 — verifies successful PRIVMSG delivery between users and
 * inside a channel: full sender prefix, CRLF termination, no echo to the
 * sender, casemapped nick lookup, and preserved trailing spaces.
 */
static void testPhase13PrivmsgDelivery()
{
    TestServerProcess server;

    if (!server.start())
    {
        reportFailure(
            "Server should start for phase 13 PRIVMSG delivery tests",
            "running ircserv",
            "server startup failed"
        );
        return;
    }

    const int aliceSocketFd = connectToServer(server.getPort());
    const int roxanaSocketFd = connectToServer(server.getPort());
    const int bobSocketFd = connectToServer(server.getPort());
    const int carolSocketFd = connectToServer(server.getPort());

    if (aliceSocketFd == -1
        || roxanaSocketFd == -1
        || bobSocketFd == -1
        || carolSocketFd == -1)
    {
        reportFailure(
            "Clients should connect for phase 13 PRIVMSG delivery tests",
            "successful connections",
            "connection failed"
        );

        if (aliceSocketFd != -1)
            ::close(aliceSocketFd);
        if (roxanaSocketFd != -1)
            ::close(roxanaSocketFd);
        if (bobSocketFd != -1)
            ::close(bobSocketFd);
        if (carolSocketFd != -1)
            ::close(carolSocketFd);
        return;
    }

    const std::string aliceWelcome =
        registerClient(aliceSocketFd, "alice");
    registerClient(roxanaSocketFd, "roxana");
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
            "Alice welcome should expose a usable client prefix",
            "welcome containing alice!alice@",
            aliceWelcome
        );
        ::close(aliceSocketFd);
        ::close(roxanaSocketFd);
        ::close(bobSocketFd);
        ::close(carolSocketFd);
        return;
    }

    expectContains(
        alicePrefix,
        "alice!alice@",
        "Phase 13: sender prefix must include nick!user@host"
    );

    sendAll(aliceSocketFd, "PRIVMSG roxana :Hola\r\n");

    const std::string userMessage =
        receiveAvailableData(roxanaSocketFd, 500);
    const std::string expectedUserMessage =
        ":" + alicePrefix + " PRIVMSG roxana :Hola\r\n";

    expectEqual(
        userMessage,
        expectedUserMessage,
        "Phase 13: PRIVMSG to a nickname should reach only the recipient"
    );
    expectTrue(
        userMessage.size() >= 2
            && userMessage[userMessage.size() - 2] == '\r'
            && userMessage[userMessage.size() - 1] == '\n',
        "Phase 13: delivered PRIVMSG must end with CRLF",
        "message ending with \\r\\n",
        escapeOutput(userMessage)
    );
    expectEqual(
        receiveAvailableData(aliceSocketFd, 200),
        "",
        "Phase 13: PRIVMSG to a nickname should not echo to the sender"
    );
    expectEqual(
        receiveAvailableData(bobSocketFd, 200),
        "",
        "Phase 13: PRIVMSG to a nickname should not reach unrelated clients"
    );
    expectEqual(
        receiveAvailableData(carolSocketFd, 200),
        "",
        "Phase 13: PRIVMSG to a nickname should not reach other unrelated clients"
    );

    sendAll(aliceSocketFd, "PRIVMSG ROXANA :Casemap\r\n");
    expectEqual(
        receiveAvailableData(roxanaSocketFd, 500),
        ":" + alicePrefix + " PRIVMSG ROXANA :Casemap\r\n",
        "Phase 13: PRIVMSG should find nicknames with IRC casemapping"
    );

    sendAll(
        aliceSocketFd,
        "PRIVMSG roxana :Hola con espacios y CTCP\r\n"
    );
    expectEqual(
        receiveAvailableData(roxanaSocketFd, 500),
        ":" + alicePrefix
            + " PRIVMSG roxana :Hola con espacios y CTCP\r\n",
        "Phase 13: PRIVMSG trailing text must preserve spaces"
    );

    sendAll(aliceSocketFd, "JOIN #general\r\n");
    receiveAvailableData(aliceSocketFd, 500);
    sendAll(roxanaSocketFd, "JOIN #general\r\n");
    receiveAvailableData(aliceSocketFd, 500);
    receiveAvailableData(roxanaSocketFd, 500);
    sendAll(bobSocketFd, "JOIN #general\r\n");
    receiveAvailableData(aliceSocketFd, 500);
    receiveAvailableData(roxanaSocketFd, 500);
    receiveAvailableData(bobSocketFd, 500);
    sendAll(carolSocketFd, "JOIN #general\r\n");
    receiveAvailableData(aliceSocketFd, 500);
    receiveAvailableData(roxanaSocketFd, 500);
    receiveAvailableData(bobSocketFd, 500);
    receiveAvailableData(carolSocketFd, 500);

    sendAll(aliceSocketFd, "PRIVMSG #general :Hola a todos\r\n");

    const std::string expectedChannelMessage =
        ":" + alicePrefix + " PRIVMSG #general :Hola a todos\r\n";

    expectEqual(
        receiveAvailableData(roxanaSocketFd, 500),
        expectedChannelMessage,
        "Phase 13: channel PRIVMSG should reach roxana"
    );
    expectEqual(
        receiveAvailableData(bobSocketFd, 500),
        expectedChannelMessage,
        "Phase 13: channel PRIVMSG should reach bob"
    );
    expectEqual(
        receiveAvailableData(carolSocketFd, 500),
        expectedChannelMessage,
        "Phase 13: channel PRIVMSG should reach carol"
    );
    expectEqual(
        receiveAvailableData(aliceSocketFd, 200),
        "",
        "Phase 13: channel PRIVMSG should not be echoed to the sender"
    );

    ::close(aliceSocketFd);
    ::close(roxanaSocketFd);
    ::close(bobSocketFd);
    ::close(carolSocketFd);
}

static void testOversizedErrorDoesNotStopServer()
{
    TestServerProcess server;

    if (!server.start())
    {
        reportFailure(
            "Server should start for oversized reply test",
            "running ircserv",
            "server startup failed"
        );
        return;
    }

    const int socketFd = connectToServer(server.getPort());

    if (socketFd == -1)
    {
        reportFailure(
            "Client should connect for oversized reply test",
            "successful connection",
            "connection failed"
        );
        return;
    }

    sendAll(
        socketFd,
        std::string(490, 'X') + "\r\n"
    );

    ::usleep(200000);

    expectTrue(
        server.isRunning(),
        "A valid long command should not terminate the entire server",
        "ircserv remains running",
        "ircserv exited"
    );

    ::close(socketFd);
}

static void testSlowClientDoesNotStopServer()
{
    TestServerProcess server;

    if (!server.start())
    {
        reportFailure(
            "Server should start for output buffer test",
            "running ircserv",
            "server startup failed"
        );
        return;
    }

    const int slowSocket = connectToServer(server.getPort());

    if (slowSocket == -1)
    {
        reportFailure(
            "Slow client should connect",
            "successful connection",
            "connection failed"
        );
        return;
    }

    int receiveBufferSize = 1024;

    ::setsockopt(
        slowSocket,
        SOL_SOCKET,
        SO_RCVBUF,
        &receiveBufferSize,
        sizeof(receiveBufferSize)
    );

    std::string commandBurst;

    for (int commandIndex = 0;
         commandIndex < 50000;
         ++commandIndex)
    {
        commandBurst += "X\r\n";
    }

    sendAll(slowSocket, commandBurst);
    ::usleep(300000);

    expectTrue(
        server.isRunning(),
        "A slow client exceeding the output buffer should not stop the server",
        "ircserv remains running",
        "ircserv exited"
    );

    const std::string probeResponse = sendCommandAndReceive(
        server.getPort(),
        "UNKNOWN\r\n"
    );

    expectEqual(
        probeResponse,
        ":irc.42.local 421 * UNKNOWN :Unknown command\r\n",
        "Server should continue serving other clients after removing a slow client"
    );

    ::close(slowSocket);
}

/**
 * @brief Verifies PING responses, missing-origin errors, PONG acceptance,
 * token preservation, and availability before client registration.
 */
static void testPhase10PingAndPong()
{
    TestServerProcess server;

    if (!server.start())
    {
        reportFailure(
            "Server should start for PING and PONG tests",
            "running ircserv",
            "server startup failed"
        );
        return;
    }

    const int socketFd = connectToServer(server.getPort());

    if (socketFd == -1)
    {
        reportFailure(
            "Client should connect for PING and PONG tests",
            "successful connection",
            "connection failed"
        );
        return;
    }

    const std::string commands =
        "PING :phase10 token\r\n"
        "PING\r\n"
        "PONG :client token\r\n"
        "PING :still-connected\r\n"
        "PONG\r\n";

    if (!sendAll(socketFd, commands))
    {
        reportFailure(
            "PING and PONG commands should be sent",
            "successful send",
            "send failed"
        );
        ::close(socketFd);
        return;
    }

    const std::string response =
        receiveAvailableData(socketFd, 500);

    const std::string expectedResponse =
        "PONG :phase10 token\r\n"
        ":irc.42.local 409 * :No origin specified\r\n"
        "PONG :still-connected\r\n"
        ":irc.42.local 409 * :No origin specified\r\n";

    expectEqual(
        response,
        expectedResponse,
        "PING and PONG should work before registration"
    );

    ::close(socketFd);
}

/**
 * @brief Verifies CAP LS, LIST, REQ and END, case-insensitive subcommands,
 * missing-parameter errors, empty capability lists, NAK responses, and the
 * correct client identifier before and after NICK.
 */
static void testPhase10CapabilityNegotiation()
{
    TestServerProcess server;

    if (!server.start())
    {
        reportFailure(
            "Server should start for CAP tests",
            "running ircserv",
            "server startup failed"
        );
        return;
    }

    const int socketFd = connectToServer(server.getPort());

    if (socketFd == -1)
    {
        reportFailure(
            "Client should connect for CAP tests",
            "successful connection",
            "connection failed"
        );
        return;
    }

    const std::string commands =
        "CAP ls 302\r\n"
        "CAP LIST\r\n"
        "CAP req :multi-prefix sasl\r\n"
        "CAP end\r\n"
        "CAP\r\n"
        "CAP REQ\r\n"
        "CAP REQ :\r\n"
        "NICK capclient\r\n"
        "CAP list\r\n"
        "PING :cap-ended\r\n";

    if (!sendAll(socketFd, commands))
    {
        reportFailure(
            "CAP commands should be sent",
            "successful send",
            "send failed"
        );
        ::close(socketFd);
        return;
    }

    const std::string response =
        receiveAvailableData(socketFd, 500);

    const std::string expectedResponse =
        ":irc.42.local CAP * LS :\r\n"
        ":irc.42.local CAP * LIST :\r\n"
        ":irc.42.local CAP * NAK :multi-prefix sasl\r\n"
        ":irc.42.local 461 * CAP :Not enough parameters\r\n"
        ":irc.42.local 461 * CAP :Not enough parameters\r\n"
        ":irc.42.local 461 * CAP :Not enough parameters\r\n"
        ":irc.42.local CAP capclient LIST :\r\n"
        "PONG :cap-ended\r\n";

    expectEqual(
        response,
        expectedResponse,
        "CAP negotiation commands should produce the expected responses"
    );

    ::close(socketFd);
}

/**
 * @brief Verifies QUIT before registration, QUIT with a reason after
 * registration, TCP connection closure, and nickname release.
 */
static void testPhase10QuitAndNicknameRelease()
{
    TestServerProcess server;

    if (!server.start())
    {
        reportFailure(
            "Server should start for QUIT tests",
            "running ircserv",
            "server startup failed"
        );
        return;
    }

    const int unregisteredSocketFd =
        connectToServer(server.getPort());

    if (unregisteredSocketFd == -1)
    {
        reportFailure(
            "Unregistered client should connect for QUIT test",
            "successful connection",
            "connection failed"
        );
        return;
    }

    if (!sendAll(unregisteredSocketFd, "QUIT\r\n"))
    {
        reportFailure(
            "QUIT without a reason should be sent",
            "successful send",
            "send failed"
        );
        ::close(unregisteredSocketFd);
        return;
    }

    expectTrue(
        waitForSocketClosure(unregisteredSocketFd, 1000),
        "QUIT should close an unregistered client connection",
        "closed TCP connection",
        "connection remained open"
    );

    ::close(unregisteredSocketFd);

    const int registeredSocketFd =
        connectToServer(server.getPort());

    if (registeredSocketFd == -1)
    {
        reportFailure(
            "Registered client should connect for QUIT test",
            "successful connection",
            "connection failed"
        );
        return;
    }

    const std::string registrationResponse =
        registerClient(registeredSocketFd, "quitclient");

    expectContains(
        registrationResponse,
        ":irc.42.local 001 quitclient ",
        "Client should register before testing QUIT with a reason"
    );

    if (!sendAll(
            registeredSocketFd,
            "QUIT :Phase 10 complete\r\n"
        ))
    {
        reportFailure(
            "QUIT with a reason should be sent",
            "successful send",
            "send failed"
        );
        ::close(registeredSocketFd);
        return;
    }

    expectTrue(
        waitForSocketClosure(registeredSocketFd, 1000),
        "QUIT with a reason should close the client connection",
        "closed TCP connection",
        "connection remained open"
    );

    ::close(registeredSocketFd);

    const int reusedNicknameSocketFd =
        connectToServer(server.getPort());

    if (reusedNicknameSocketFd == -1)
    {
        reportFailure(
            "A new client should connect after QUIT",
            "successful connection",
            "connection failed"
        );
        return;
    }

    const std::string reusedNicknameResponse =
        registerClient(reusedNicknameSocketFd, "quitclient");

    expectContains(
        reusedNicknameResponse,
        ":irc.42.local 001 quitclient ",
        "QUIT should release the client's nickname"
    );

    expectTrue(
        reusedNicknameResponse.find(" 433 ")
            == std::string::npos,
        "A nickname should be reusable after QUIT",
        "registration without numeric 433",
        reusedNicknameResponse
    );

    ::close(reusedNicknameSocketFd);
}

int main()
{
    testIrcMessageSerialization();
    testGeneratedMessagesRejectControlCharacters();
    testSerializedMessageLengthLimit();
    testNumericCodeFormatting();
    testRfc1459Casemapping();
    testClientLineLengthBoundaries();
    testFragmentedLfBoundary();
    testServerPrefixAndUnknownCommand();
    testWelcomeNumerics();
    testPassRegistrationRules();
    testSpecificMissingParameterNumerics();
    testPhase13PrivmsgErrors();
    testPhase13PrivmsgDelivery();
    testPhase10PingAndPong();
    testPhase10CapabilityNegotiation();
    testPhase10QuitAndNicknameRelease();
    testOversizedErrorDoesNotStopServer();
    testSlowClientDoesNotStopServer();

    if (g_failures != 0)
    {
        std::cerr
            << g_failures
            << " protocol checklist test(s) failed"
            << std::endl;

        return EXIT_FAILURE;
    }

    std::cout
        << "All point 1 protocol checklist tests passed"
        << std::endl;

    return EXIT_SUCCESS;
}