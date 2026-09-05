// Copyright 2026 @esettes, @danielfdez17
#ifndef SRC_TESTS_PROTOCOLTESTHARNESS_HPP_
#define SRC_TESTS_PROTOCOLTESTHARNESS_HPP_

#include <arpa/inet.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "Console.hpp"

/**
 * @file ProtocolTestHarness.hpp
 * @brief Shared helpers for independent protocol test binaries. Each suite
 *        file includes this header and provides its own main() so branches
 *        can add tests without colliding in ProtocolTests.cpp.
 */

static int g_failures = 0;

inline std::string escapeOutput(const std::string &value) {
    std::string escaped;

    for (std::size_t index = 0; index < value.size(); ++index) {
        const unsigned char character =
            static_cast<unsigned char>(value[index]);

        if (character == '\r')
            escaped += "\\r";
        else if (character == '\n')
            escaped += "\\n";
        else if (character == '\0')
            escaped += "\\0";
        else if (character == '\x01')
            escaped += "\\x01";
        else
            escaped += static_cast<char>(character);
    }

    return escaped;
}

inline void reportFailure(
    const std::string &testName,
    const std::string &expected,
    const std::string &actual
) {
    std::cerr << Console::ERROR << testName << std::endl;
    std::cerr << "  expected: " << escapeOutput(expected) << std::endl;
    std::cerr << "  actual  : " << escapeOutput(actual) << std::endl;
    ++g_failures;
}

inline void expectTrue(
    bool condition,
    const std::string &testName,
    const std::string &expected,
    const std::string &actual
) {
    if (!condition)
        reportFailure(testName, expected, actual);
}

inline void expectEqual(
    const std::string &actual,
    const std::string &expected,
    const std::string &testName
) {
    if (actual != expected)
        reportFailure(testName, expected, actual);
}

inline void expectContains(
    const std::string &actual,
    const std::string &expectedFragment,
    const std::string &testName
) {
    if (actual.find(expectedFragment) == std::string::npos) {
        reportFailure(
            testName,
            "output containing: " + expectedFragment,
            actual);
    }
}

inline int findAvailablePort() {
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
            sizeof(address)) == -1) {
        ::close(socketFd);
        return -1;
    }

    socklen_t addressLength = sizeof(address);

    if (::getsockname(
            socketFd,
            reinterpret_cast<struct sockaddr *>(&address),
            &addressLength) == -1) {
        ::close(socketFd);
        return -1;
    }

    const int availablePort = ntohs(address.sin_port);

    ::close(socketFd);
    return availablePort;
}

inline int connectToServer(int port) {
    const int socketFd = ::socket(AF_INET, SOCK_STREAM, 0);

    if (socketFd == -1)
        return -1;

    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<uint16_t>(port));

    if (::connect(
            socketFd,
            reinterpret_cast<const struct sockaddr *>(&address),
            sizeof(address)) == -1) {
        ::close(socketFd);
        return -1;
    }

    return socketFd;
}

inline bool sendAll(int socketFd, const std::string &data) {
    std::size_t sentByteCount = 0;

    while (sentByteCount < data.size()) {
        const ssize_t result = ::send(
            socketFd,
            data.data() + sentByteCount,
            data.size() - sentByteCount,
            0);

        if (result > 0) {
            sentByteCount += static_cast<std::size_t>(result);
            continue;
        }

        if (result == -1 && errno == EINTR)
            continue;

        return false;
    }

    return true;
}

inline std::string receiveAvailableData(
    int socketFd,
    int timeoutMilliseconds
) {
    std::string response;
    char buffer[4096];

    while (true) {
        struct pollfd descriptor;

        descriptor.fd = socketFd;
        descriptor.events = POLLIN;
        descriptor.revents = 0;

        const int pollResult = ::poll(
            &descriptor,
            1,
            timeoutMilliseconds);

        if (pollResult <= 0)
            break;

        if (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            const ssize_t receivedByteCount = ::recv(
                socketFd,
                buffer,
                sizeof(buffer),
                0);

            if (receivedByteCount > 0) {
                response.append(
                    buffer,
                    static_cast<std::size_t>(receivedByteCount));
            }

            break;
        }

        if (!(descriptor.revents & POLLIN))
            break;

        const ssize_t receivedByteCount = ::recv(
            socketFd,
            buffer,
            sizeof(buffer),
            0);

        if (receivedByteCount <= 0)
            break;

        response.append(
            buffer,
            static_cast<std::size_t>(receivedByteCount));

        timeoutMilliseconds = 50;
    }

    return response;
}

inline bool waitForSocketClosure(
    int socketFd,
    int timeoutMilliseconds
) {
    struct pollfd descriptor;

    descriptor.fd = socketFd;
    descriptor.events = POLLIN;
    descriptor.revents = 0;

    int pollResult;

    do {
        pollResult = ::poll(
            &descriptor,
            1,
            timeoutMilliseconds);
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

    do {
        receivedByteCount = ::recv(
            socketFd,
            &receivedByte,
            sizeof(receivedByte),
            0);
    }
    while (receivedByteCount == -1 && errno == EINTR);

    if (receivedByteCount == 0)
        return true;

    if (receivedByteCount == -1
        && (errno == ECONNRESET || errno == ENOTCONN)) {
        return true;
    }

    return false;
}

class TestServerProcess {
 private:
        pid_t processId;
        int port;

        TestServerProcess(const TestServerProcess &other);
        TestServerProcess &operator=(const TestServerProcess &other);

        bool waitUntilReady() {
            for (int attempt = 0; attempt < 100; ++attempt) {
                const int probeSocket = connectToServer(port);

                if (probeSocket != -1) {
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
              port(findAvailablePort()) {
        }

        ~TestServerProcess() {
            stop();
        }

        bool start(int fileDescriptorLimit = 0) {
            if (port == -1)
                return false;

            processId = ::fork();

            if (processId == -1)
                return false;

            if (processId == 0) {
                if (fileDescriptorLimit > 0) {
                    struct rlimit limit;

                    limit.rlim_cur = static_cast<rlim_t>(fileDescriptorLimit);
                    limit.rlim_max = static_cast<rlim_t>(fileDescriptorLimit);

                    if (::setrlimit(RLIMIT_NOFILE, &limit) == -1)
                        ::_exit(EXIT_FAILURE);
                }

                const int nullFd = ::open("/dev/null", O_WRONLY);

                if (nullFd != -1) {
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
                    static_cast<char *>(NULL));

                ::_exit(EXIT_FAILURE);
            }

            return waitUntilReady();
        }

        void stop() {
            if (processId <= 0)
                return;

            ::kill(processId, SIGINT);

            for (int attempt = 0; attempt < 100; ++attempt) {
                int status = 0;
                const pid_t result = ::waitpid(
                    processId,
                    &status,
                    WNOHANG);

                if (result == processId) {
                    processId = -1;
                    return;
                }

                ::usleep(10000);
            }

            ::kill(processId, SIGKILL);
            ::waitpid(processId, NULL, 0);
            processId = -1;
        }

        bool isRunning() {
            if (processId <= 0)
                return false;

            int status = 0;
            const pid_t result = ::waitpid(
                processId,
                &status,
                WNOHANG);

            if (result == 0)
                return true;

            if (result == processId)
                processId = -1;

            return false;
        }

        int getPort() const {
            return port;
        }
};

inline std::string sendCommandAndReceive(
    int port,
    const std::string &command
) {
    const int socketFd = connectToServer(port);

    if (socketFd == -1)
        return "connection failed";

    if (!sendAll(socketFd, command)) {
        ::close(socketFd);
        return "send failed";
    }

    const std::string response =
        receiveAvailableData(socketFd, 500);

    ::close(socketFd);
    return response;
}

inline std::string registerClient(
    int socketFd,
    const std::string &nickname
) {
    const std::string registration =
        "PASS secret\r\n"
        "NICK " + nickname + "\r\n"
        "USER " + nickname + " 0 * :" + nickname + "\r\n";

    if (!sendAll(socketFd, registration))
        return "send failed";

    return receiveAvailableData(socketFd, 500);
}

inline bool extractClientPrefixFromWelcome(
    const std::string &welcomeResponse,
    const std::string &nickname,
    std::string &clientPrefix
) {
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
        prefixEnd - prefixStart);
    return true;
}

inline void closeSocket(int socketFd) {
    if (socketFd != -1)
        ::close(socketFd);
}

inline std::string sendLineAndReceive(
    int socketFd,
    const std::string &line
) {
    if (!sendAll(socketFd, line))
        return "send failed";

    return receiveAvailableData(socketFd, 500);
}

inline void discardPendingData(int socketFd) {
    receiveAvailableData(socketFd, 200);
}

inline std::size_t countOccurrences(
    const std::string &haystack,
    const std::string &needle
) {
    if (needle.empty())
        return 0;

    std::size_t count = 0;
    std::string::size_type position = 0;

    while (true) {
        position = haystack.find(needle, position);

        if (position == std::string::npos)
            return count;

        ++count;
        position += needle.size();
    }
}

inline bool sendChunks(
    int socketFd,
    const std::string *chunks,
    std::size_t chunkCount
) {
    for (std::size_t index = 0; index < chunkCount; ++index) {
        if (!sendAll(socketFd, chunks[index]))
            return false;

        if (index + 1 < chunkCount)
            ::usleep(20000);
    }

    return true;
}

inline bool everyLineUsesCrlf(const std::string &data) {
    if (data.empty())
        return true;

    if (data[data.size() - 1] != '\n')
        return false;

    for (std::size_t index = 0; index < data.size(); ++index) {
        if (data[index] == '\n'
            && (index == 0 || data[index - 1] != '\r')) {
            return false;
        }
    }

    return true;
}

inline bool startServerOrFail(
    TestServerProcess &server,
    const std::string &testName
) {
    if (server.start())
        return true;

    reportFailure(testName, "running ircserv", "server startup failed");
    return false;
}

inline int finishSuite(const std::string &suiteName) {
    if (g_failures != 0) {
        std::cerr
            << g_failures
            << " " << suiteName << " test(s) failed"
            << std::endl;

        return EXIT_FAILURE;
    }

    std::cout
        << "All " << suiteName << " tests passed"
        << std::endl;

    return EXIT_SUCCESS;
}

#endif  // SRC_TESTS_PROTOCOLTESTHARNESS_HPP_
