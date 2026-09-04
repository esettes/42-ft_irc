#include "ProtocolTestHarness.hpp"

#include <vector>

/**
 * @brief Exhausting the process file-descriptor limit must not terminate the
 * server. Already-accepted clients keep being served, and accepting resumes
 * once a descriptor is released.
 */
static void testFileDescriptorExhaustionDoesNotStopServer()
{
    const int reducedFileDescriptorLimit = 32;
    const int extraConnectionCount = 50;

    TestServerProcess server;

    if (!server.start(reducedFileDescriptorLimit))
    {
        reportFailure(
            "Server should start with a reduced file descriptor limit",
            "running ircserv",
            "server startup failed"
        );
        return;
    }

    const int keeperSocketFd = connectToServer(server.getPort());

    if (keeperSocketFd == -1)
    {
        reportFailure(
            "A client should connect before exhausting file descriptors",
            "successful connection",
            "connection failed"
        );
        return;
    }

    registerClient(keeperSocketFd, "keeper");

    std::vector<int> extraSockets;

    for (int connectionIndex = 0;
        connectionIndex < extraConnectionCount;
        ++connectionIndex)
    {
        const int extraSocketFd = connectToServer(server.getPort());

        if (extraSocketFd != -1)
            extraSockets.push_back(extraSocketFd);
    }

    ::usleep(200000);

    expectTrue(
        server.isRunning(),
        "Exceeding the file descriptor limit should not stop the server",
        "ircserv remains running",
        "ircserv exited"
    );

    expectEqual(
        sendLineAndReceive(keeperSocketFd, "PING :still-here\r\n"),
        "PONG :still-here\r\n",
        "An already-connected client should still be served after fd exhaustion"
    );

    for (std::size_t index = 0; index < extraSockets.size(); ++index)
        closeSocket(extraSockets[index]);

    ::usleep(200000);

    const int recoveredSocketFd = connectToServer(server.getPort());

    if (recoveredSocketFd == -1)
    {
        reportFailure(
            "The server should accept a new client after fds are released",
            "successful connection",
            "connection failed"
        );
        closeSocket(keeperSocketFd);
        return;
    }

    expectContains(
        registerClient(recoveredSocketFd, "recovered"),
        "001",
        "A new client should register after the file descriptor limit is no longer exceeded"
    );

    expectTrue(
        server.isRunning(),
        "Releasing connections after fd exhaustion should keep the server running",
        "ircserv remains running",
        "ircserv exited"
    );

    closeSocket(recoveredSocketFd);
    closeSocket(keeperSocketFd);
}

int main()
{
    testFileDescriptorExhaustionDoesNotStopServer();

    return finishSuite("file descriptor limit");
}
