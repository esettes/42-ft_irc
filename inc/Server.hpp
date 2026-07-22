#ifndef SERVER_HPP
#define SERVER_HPP

#include "Client.hpp"
#include "IrcMessage.hpp"
#include "Channel.hpp"

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
        std::map<std::string, Channel> channels;

        void createListeningSocket();
        void acceptClient();
        void receiveFromClient(int socketFd);
        void processClientBuffer(Client &client);
        void dispatchCommand(Client &client, const IrcMessage &msg);
        void disconnectClient(int socketFd);

        Server(const Server &other);
        Server &operator=(const Server &other);

    public:
        Server(int port, const std::string &password);
        ~Server();

        void run();
};

#endif
