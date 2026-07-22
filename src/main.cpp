#include "Server.hpp"

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

static int parsePort(const char *arg)
{
    char *remainChars;
    long port;

    errno = 0;
    remainChars = NULL;
    port = std::strtol(arg, &remainChars, 10);

    if (errno != 0 || *remainChars != '\0')
        throw std::runtime_error("Invalid port");

    if (port < 1 || port > 65535)
        throw std::runtime_error("Port must be between 1 and 65535");

    return static_cast<int>(port);
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
        return EXIT_FAILURE;
    }
    try
    {
        const int port = parsePort(argv[1]);
        const std::string password = argv[2];
        if (password.empty())
            throw std::runtime_error("Password cannot be empty");
        Server server(port, password);
        server.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
