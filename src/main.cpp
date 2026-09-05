// Copyright 2026 @esettes, @danielfdez17
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "Irc.hpp"
#include "Server.hpp"
#include "SignalHandler.hpp"

static int parsePort(const std::string &arg) {
    char *remainChars;
    uint64_t port;

    errno = 0;
    remainChars = NULL;
    if (arg.empty())
            throw std::invalid_argument(Constants::INVALID_PORT_EMPTY_MSG);
    for (std::size_t i = 0; i < arg.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(arg[i]);

        if (std::isdigit(c) == 0) {
            throw std::invalid_argument(
                Constants::INVALID_PORT_CONTENT_MSG);
        }
    }

    port = std::strtol(arg.c_str(), &remainChars, 10);

    if (errno != 0 || *remainChars != '\0')
        throw std::runtime_error(Constants::INVALID_PORT_MSG);


    if (port < Constants::MIN_PORT || port > Constants::MAX_PORT)
        throw std::runtime_error(Constants::INVALID_PORT_RANGE_MSG);

    return static_cast<int>(port);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
        return EXIT_FAILURE;
    }
    try {
        const int port = parsePort(argv[1]);
        const std::string password = argv[2];
        if (password.empty())
            throw std::runtime_error(Constants::INVALID_PORT_EMPTY_MSG);

        SignalHandler::runSignalHandler();

        // command dispatcher

        Server server(port, password);
        server.run();
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
