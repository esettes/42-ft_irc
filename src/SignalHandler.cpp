// Copyright 2026 @esettes, @danielfdez17
#include "SignalHandler.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

/**
 * @file SignalHandler.cpp
 * @brief Implements the signal handling utilities used to request orderly server shutdown.
 * 
 * This module provides a static interface for installing signal handlers for SIGINT and SIGTERM,
 * allowing the server to detect shutdown requests and exit gracefully. It also ignores SIGPIPE
 * to prevent crashes when writing to closed sockets.
 */
namespace {
std::runtime_error createSystemError(
        const std::string &operation,
        int errorNumber) {
        return std::runtime_error(
            operation + ": " + std::strerror(errorNumber));
}
void installSignalAction(int signalNumber, void (*signalFunction)(int)) {
        struct sigaction signalAction;

        std::memset(&signalAction, 0, sizeof(signalAction));

        signalAction.sa_handler = signalFunction;
        signalAction.sa_flags = 0;

        if (::sigemptyset(&signalAction.sa_mask) == -1) {
            const int errorNumber = errno;

            throw createSystemError("sigemptyset", errorNumber);
        }

        if (::sigaction(signalNumber, &signalAction, NULL) == -1) {
            const int errorNumber = errno;

            throw createSystemError("sigaction", errorNumber);
        }
}
}  // namespace

volatile sig_atomic_t SignalHandler::shutdownRequested = 0;

void SignalHandler::runSignalHandler() {
    installSignalAction(SIGINT, handleTerminationSignal);
    installSignalAction(SIGTERM, handleTerminationSignal);
    installSignalAction(SIGPIPE, SIG_IGN);
}

bool SignalHandler::isShutdownRequested() {
    return shutdownRequested != 0;
}

void SignalHandler::handleTerminationSignal(int signalNumber) {
    static_cast<void>(signalNumber);

    shutdownRequested = 1;
}
