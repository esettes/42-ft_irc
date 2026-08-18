#ifndef SERVER_HPP
# define SERVER_HPP

#include "Client.hpp"
#include "IrcMessage.hpp"
#include "IrcCasemap.hpp"
#include "Channel.hpp"
#include "CommandDispatcher.hpp"
#include "NumericReplies.hpp"
#include "SignalHandler.hpp"

#include <map>
#include <poll.h>
#include <cerrno>
#include <string>
#include <vector>
#include <cstddef>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>

/**
 * @file Server.hpp
 * @brief Declares the main IRC server that accepts clients, processes messages, and manages channels.
 * 
 * @param port The TCP port on which the server listens for incoming connections.
 * @param password The server password required for clients to register.
 * @param listenSocket The file descriptor of the listening socket.
 * @param dispatcher The CommandDispatcher instance responsible for routing IRC commands to handlers.
 * @param pollFds A vector of pollfd structures used for monitoring multiple file descriptors for events.
 * @param clients A mapping of client socket file descriptors to their corresponding Client objects.
 * @param channels A mapping of normalized channel names to their corresponding Channel objects.
 * @param clientsByNickname Client objects for quick lookup. Allows detecting duplicates and searching for users.
 */
class Server
{
    private:
        int port;
        std::string password;
        std::string serverName;
        int listenSocket;
        CommandDispatcher dispatcher;

        std::vector<struct pollfd> pollFds;
        std::map<int, Client *> clients;
        std::map<std::string, Client *> clientsByNickname;
        std::map<std::string, Channel> channels;

        void createListeningSocket();
        void registerListeningSocket();

        void acceptClient();
        void configureSocketAsNonBlocking(int socketFd);

        bool receiveClientData(std::size_t descriptorIndex);
        void removeClient(std::size_t descriptorIndex);
        void removeNicknameIndexEntry(const Client &client);

        bool processClientBuffer(Client &client);

        void tryRegisterClient(Client &client);
        void sendWelcomeMessages(Client &client);

        bool flushClientOutput(int socketFd);
        void updateClientPollEvents(int socketFd);
        std::string resolveClientHost(
            const struct sockaddr_storage &clientAddress,
            socklen_t clientAddressLength
        ) const;

        std::string getReplyTarget(const Client &client) const;
        Server(const Server &other);
        Server &operator=(const Server &other);

        void closeAllFds();
        void closeFd(int &fd);

        void displayStartupInformation() const;

    public:
        Server(int port, const std::string &password);
        ~Server();

        void run();
        void stop();

        int getPort() const;
        const std::string &getServerName() const;
        bool isPasswordCorrect(const std::string &providedPassword) const;
        std::string getServerPrefix() const;
        std::string getClientPrefix(const Client &client) const;
        std::string normalizeNickname(const std::string &nickname) const;
        bool assignNickname(Client &client, const std::string &nickname);
        bool isValidNickname(const std::string &nickname) const;
        std::string normalizeChannelName(const std::string &channelName) const;
        Client *findClientByNickname(const std::string &nickname);
        Channel *findChannel(const std::string &channelName);
        std::string buildNumericReply(
            int numericCode,
            const Client &client,
            const std::string &trailingMessage
        ) const;
        std::string buildNumericReply(
            int numericCode,
            const Client &client,
            const std::string &parameter,
            const std::string &trailingMessage
        ) const;
        std::string buildNumericReply(
            int numericCode,
            const Client &client,
            const std::vector<std::string> &parameters,
            const std::string &trailingMessage
        ) const;
        std::string buildNumericReply(
            int numericCode,
            const Client &client,
            const std::vector<std::string> &parameters
        ) const;

        void queueMessage(Client &client, const std::string &message);
        void queueMessageToRelatedClients(
            Client &sourceClient,
            const std::string &message
        );
        void queueNumericReply(
            Client &client,
            int numericCode,
            const std::string &trailingMessage
        );
        void queueNumericReply(
            Client &client,
            int numericCode,
            const std::string &parameter,
            const std::string &trailingMessage
        );
        void queueNumericReply(
            Client &client,
            int numericCode,
            const std::vector<std::string> &parameters,
            const std::string &trailingMessage
        );
        void queueNumericReply(
            Client &client,
            int numericCode,
            const std::vector<std::string> &parameters
        );

        /* For send parsed commands to the dispatcher */
        void dispatchCommand(Client &client, const IrcMessage &message);
};

#endif
