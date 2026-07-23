#ifndef SIGNAL_HANDLER_HPP
#define SIGNAL_HANDLER_HPP

#include <signal.h>

class SignalHandler
{
    public:
        static void signalHandler(int sig);
        static bool isShutdownRequested();

    private:
        static volatile sig_atomic_t shutdownRequested;
        
        SignalHandler();
        SignalHandler(const SignalHandler &other);
        SignalHandler &operator=(const SignalHandler &other);
};

#endif
