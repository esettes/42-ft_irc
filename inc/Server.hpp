#ifndef SERVER_HPP
# define SERVER_HPP

#include "Client.hpp"
#include "IrcMessage.hpp"
#include "Channel.hpp"
#include "CommandDispatcher.hpp"

#include <map>
#include <poll.h>
#include <cerrno>
#include <string>
#include <vector>


class Server
{
    private:
        int port;
        std::string password;
        int listenSocket;
        CommandDispatcher dispatcher;

        std::vector<struct pollfd> pollFds;
        std::map<int, Client> clients;
        std::map<std::string, Channel> channels;

        void createListeningSocket();
        void acceptClient();
        void receiveFromClient(int socketFd);
        void processClientBuffer(Client &client);
       
        void disconnectClient(int socketFd);

        void tryRegisterClient(Client &client);
        void sendWelcomeMessages(Client &client);

        void flushClientOutput(int socketFd);
        void updateClientPollEvents(int socketFd);

        Client *findClientByNickname(const std::string &nickname);

        std::string getReplyTarget(const Client &client) const;
        std::string getClientPrefix(const Client &client) const;
        Server(const Server &other);
        Server &operator=(const Server &other);

        void closeAllFds();
        void closeFd(int &fd);

        void queueNumericReply(
            Client &client,
            const std::string &numericCode,
            const std::vector<std::string> &parameters
        );

    public:
        Server(int port, const std::string &password);
        ~Server();

        void run();
        void stop();

        int getPort() const;
        

        /* For send parsed commands to the dispatcher */
        void dispatchCommand(Client &client, const IrcMessage &message);
};

#endif
