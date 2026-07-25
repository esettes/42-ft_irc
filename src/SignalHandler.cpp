#include "SignalHandler.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

namespace
{
    std::runtime_error createSystemError(
        const std::string &operation,
        int errorNumber)
    {
        return std::runtime_error(
            operation + ": " + std::strerror(errorNumber)
        );
    }
    void installSignalAction(int signalNumber, void (*signalFunction)(int))
    {
        struct sigaction signalAction;

        std::memset(&signalAction, 0, sizeof(signalAction));

        signalAction.sa_handler = signalFunction;
        signalAction.sa_flags = 0;

        if (::sigemptyset(&signalAction.sa_mask) == -1){
            const int errorNumber = errno;

            throw createSystemError("sigemptyset", errorNumber);
        }

        if (::sigaction(signalNumber, &signalAction, NULL) == -1){
            const int errorNumber = errno;

            throw createSystemError("sigaction", errorNumber);
        }
    }
}

volatile sig_atomic_t SignalHandler::shutdownRequested = 0;

void SignalHandler::runSignalHandler()
{
    installSignalAction(SIGINT, handleTerminationSignal);
    installSignalAction(SIGTERM, handleTerminationSignal);
    installSignalAction(SIGPIPE, SIG_IGN);
}

bool SignalHandler::isShutdownRequested()
{
    return shutdownRequested != 0;
}

void SignalHandler::handleTerminationSignal(int signalNumber)
{
    static_cast<void>(signalNumber);

    shutdownRequested = 1;
}