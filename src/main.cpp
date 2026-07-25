#include "Server.hpp"
#include "SignalHandler.hpp"

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

static int parsePort(const std::string &arg)
{
    char *remainChars;
    long port;

    errno = 0;
    remainChars = NULL;
    if (arg.empty())
            throw std::invalid_argument("port cannot be empty");
    for (std::size_t i = 0; i < arg.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(arg[i]);

        if (std::isdigit(c) == 0){
            throw std::invalid_argument(
                "port must contain only digits");
        }
    }

    port = std::strtol(arg.c_str(), &remainChars, 10);

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
            
        SignalHandler::runSignalHandler();

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
