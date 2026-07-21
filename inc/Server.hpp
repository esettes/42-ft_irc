#ifndef SERVER_HPP
#define SERVER_HPP

#include "Client.hpp"
#include "IrcMessage.hpp"

#include <map>
#include <poll.h>
#include <string>
#include <vector>

class Server
{
private:
    int port;
    std::string password;
    int listenSocket;

    std::vector<struct pollfd> pollFds;
    std::map<int, Client> clients;
};

#endif
