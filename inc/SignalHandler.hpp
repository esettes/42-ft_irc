// Copyright 2026 @esettes, @danielfdez17
#ifndef INC_SIGNALHANDLER_HPP_
#define INC_SIGNALHANDLER_HPP_

#include <signal.h>

/**
 * @file SignalHandler.hpp
 * @brief Declares the signal handling utilities used to request orderly server shutdown.
 * 
 * @param shutdownRequested A static flag indicating whether a termination signal has been received.
 */
class SignalHandler {
 private:
        static volatile sig_atomic_t shutdownRequested;

        static void handleTerminationSignal(int signalNumber);

        SignalHandler();
        SignalHandler(const SignalHandler &other);
        SignalHandler &operator=(const SignalHandler &other);

 public:
        static void runSignalHandler();
        static bool isShutdownRequested();
};

#endif  // INC_SIGNALHANDLER_HPP_
