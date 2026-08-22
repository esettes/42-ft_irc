#include "Server.hpp"
#include "Console.hpp"
#include "MessageParser.hpp"
#include <netdb.h>
#include <set>
#include <utility>

/** por el momento debe:
 * 
 * Crea un socket TCP.
 * Activa SO_REUSEADDR.
 * Configura sockets no bloqueantes.
 * Hace bind() y listen().
 * Espera eventos con poll().
 * Acepta varios clientes.
 * Conserva mensajes fragmentados en el buffer.
 * Gestiona envíos parciales.
 * Implementa CAP, PASS, NICK, USER, PING y QUIT.
 * Envía los mensajes de bienvenida cuando termina el registro.
 * 
 */

namespace
{
    const int INVALID_FD = -1;
    const int POLL_TIMEOUT_MS = 1000;
    const std::size_t RECEIVE_BUFFER_SIZE = 4096;
    const std::size_t MAX_SEND_SIZE = 16384;
    const char *DEFAULT_SERVER_NAME = "irc.42.local";
    const char *UNKNOWN_CLIENT_HOST = "unknown";

    std::runtime_error createSystemError(const std::string &operation, int errorNumber)
    {
        return std::runtime_error(operation + ": " + std::strerror(errorNumber));
    }
}

Server::Server(int port, const std::string &password)
    : port(port),
      password(password),
      serverName(DEFAULT_SERVER_NAME),
      listenSocket(INVALID_FD),
      dispatcher(*this)
{
    if (port < 1 || port > 65535)
        throw std::invalid_argument("invalid server port");

    if (password.empty())
        throw std::invalid_argument("password cannot be empty");

    createListeningSocket();
    registerListeningSocket();
    displayStartupInformation();
}

void Server::configureSocketAsNonBlocking(int socketFd)
{
    const int currentFlags = ::fcntl(socketFd, F_GETFL, 0);

    if (currentFlags == -1)
    {
        const int errorNumber = errno;
        throw createSystemError("fcntl F_GETFL", errorNumber);
    }
    if (::fcntl(socketFd, F_SETFL, currentFlags | O_NONBLOCK) == -1)
    {
        const int errorNumber = errno;
        throw createSystemError("fcntl F_SETFL", errorNumber);
    }
}

/** @brief Creates the listening socket.
 * Sets the SO_REUSEADDR option.
 * Describes the server's address and binds the socket to it.
 * Starts listening for incoming connections.
*/
void Server::createListeningSocket()
{
    listenSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_FD)
        throw createSystemError("socket", errno);

    try 
    {
        configureSocketAsNonBlocking(listenSocket);
    }
    catch (...)
    {
        closeFd(listenSocket);
        throw createSystemError("non-blocking configuration failed", errno);
    }

    int reuseAddr = 1;

    if (::setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr)) == -1)
    {
        const int errorNumber = errno;

        closeFd(listenSocket);
        throw createSystemError("setsockopt", errorNumber);
    }
    
    struct sockaddr_in serverAddress;
    std::memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET; // ipv4
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);  // accepts connections that are directed to any ipv4 interface of the server machine
    serverAddress.sin_port = htons(static_cast<unsigned short>(port));  // listen port

    if (::bind(listenSocket,
        reinterpret_cast<const struct sockaddr *>(&serverAddress),  // address cast to sockaddr pointer
        sizeof(serverAddress)) == -1)
    {
        const int errorNumber = errno;

        closeFd(listenSocket);
        throw createSystemError("bind", errorNumber);
    }

    if (::listen(listenSocket,SOMAXCONN) == -1)
    {
        const int errorNumber = errno;

        closeFd(listenSocket);
        throw createSystemError("listen", errorNumber);
    }
}

void Server::acceptClient()
{
    struct sockaddr_storage clientAddress;
    std::memset(&clientAddress, 0, sizeof(clientAddress));
    socklen_t clientAddressLength = sizeof(clientAddress);
    const int clientSocketFd = ::accept(
        listenSocket,
        reinterpret_cast<struct sockaddr *>(&clientAddress),
        &clientAddressLength
    );

    if (clientSocketFd == INVALID_FD)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK|| errno == EINTR)
            return;

        throw createSystemError("accept", errno);
    }

    Client *newClient = NULL;
    bool clientWasRegistered = false;

    try
    {
        configureSocketAsNonBlocking(clientSocketFd);

        const std::string clientHost = resolveClientHost(clientAddress, clientAddressLength);
        newClient = new Client(clientSocketFd, clientHost);

        const std::pair<std::map<int, Client *>::iterator, bool
        > insertionResult = clients.insert(std::make_pair(clientSocketFd, newClient));

        if (!insertionResult.second)
        {
            throw std::logic_error("client descriptor is already registered");
        }

        clientWasRegistered = true;

        pollfd clientDescriptor;

        clientDescriptor.fd = clientSocketFd;
        clientDescriptor.events = POLLIN;
        clientDescriptor.revents = 0;

        pollFds.push_back(clientDescriptor);
    }
    catch (...)
    {
        if (clientWasRegistered)
            clients.erase(clientSocketFd);

        delete newClient;
        ::close(clientSocketFd);
        throw ;
    }

    std::cout << Console::CLIENT << " Connection accepted: fd=" << clientSocketFd
        << ", host=" << newClient->getHost() << std::endl;
}

/**
 * @brief Registers the listening socket with the poll() mechanism.
 * This allows the server to monitor the listening socket for incoming connections.
 * It adds the listening socket to the pollFds vector with the POLLIN event.
 * POLLOUT not activated because the listening socket is not used for writing.
 */
void Server::registerListeningSocket()
{
    pollfd listeningDescriptor;

    listeningDescriptor.fd = listenSocket;
    listeningDescriptor.events = POLLIN;
    listeningDescriptor.revents = 0;

    pollFds.push_back(listeningDescriptor);
}

void Server::displayStartupInformation() const
{
    std::cout
        << "IRC server configuration:" << std::endl
        << "  Server name: " << serverName << std::endl
        << "  Address: 0.0.0.0" << std::endl
        << "  Port: " << port << std::endl
        << "  Protocol: TCP/IPv4" << std::endl
        << "  Socket mode: non-blocking" << std::endl
        << "  Status: listening" << std::endl;
}

std::string Server::resolveClientHost(
    const struct sockaddr_storage &clientAddress,
    socklen_t clientAddressLength
) const
{
    char hostBuffer[NI_MAXHOST];
    const struct sockaddr *addressPtr = reinterpret_cast<const struct sockaddr *>(&clientAddress);

    const int reverseLookupResult = ::getnameinfo(
        addressPtr,
        clientAddressLength,
        hostBuffer,
        sizeof(hostBuffer),
        NULL,
        0,
        NI_NAMEREQD
    );

    if (reverseLookupResult == 0)
        return std::string(hostBuffer);

    const int numericLookupResult = ::getnameinfo(
        addressPtr,
        clientAddressLength,
        hostBuffer,
        sizeof(hostBuffer),
        NULL,
        0,
        NI_NUMERICHOST
    );

    if (numericLookupResult == 0)
        return std::string(hostBuffer);

    return UNKNOWN_CLIENT_HOST;
}

std::string Server::getServerPrefix() const
{
    return serverName;
}

std::string Server::getClientPrefix(const Client &client) const
{
    return client.getNickname() + "!" + client.getUsername() + "@" + client.getHost();
}

std::string Server::normalizeNickname(const std::string &nickname) const
{
    return IrcCasemap::normalize(nickname);
}

/**
 * Nickname format rules:
 * It cannot be empty.
 * The first character must be a letter or a special character allowed by IRC.
 * Subsequent characters can be letters, numbers, special characters, or -.
 * Spaces, :, ,, *, ?, !, @, and control characters are not allowed.
 * Special characters allowed are: []\`_^{|}
 * The maximum length of a nickname is 9 characters.
 * 
 * Strict ASCII validation.
 */
bool Server::isValidNickname(const std::string &nickname) const
{
    if (nickname.empty())
        return false;

    const std::string validSpecialChars = "[]\\`_^{|}";
    const char firstChar = nickname[0];

    const bool firstCharIsLetter =
        (firstChar >= 'A' && firstChar <= 'Z')
        || (firstChar >= 'a' && firstChar <= 'z');

    const bool firstCharIsSpecial =
        validSpecialChars.find(firstChar)
        != std::string::npos;

    if (!firstCharIsLetter && !firstCharIsSpecial)
        return false;

    for (std::string::size_type i = 1; i < nickname.size(); ++i)
    {
        const char currentChar = nickname[i];

        const bool currentCharIsLetter =
            (currentChar >= 'A' && currentChar <= 'Z')
            || (currentChar >= 'a' && currentChar <= 'z');

        const bool currentCharIsDigit =
            currentChar >= '0' && currentChar <= '9';

        const bool currentCharIsSpecial =
            validSpecialChars.find(currentChar)
            != std::string::npos;

        if (!currentCharIsLetter
            && !currentCharIsDigit
            && !currentCharIsSpecial
            && currentChar != '-')
        {
            return false;
        }
    }

    return true;
}

/**
 * The only operation that will simultaneously modify:
 * The nickname stored in Client.
 * The nicknameReceived state.
 * The global index.
 * 
 * Ownership verification takes place before the previous nickname is removed.
 * Therefore, if the new one is taken, the client retains their current nickname unchanged.
 */
bool Server::assignNickname(Client &client, const std::string &nickname)
{
    const std::string normalizedNickname = normalizeNickname(nickname);

    if (normalizedNickname.empty())
        return false;

    std::map<std::string, Client *>::iterator nicknameIt =
        clientsByNickname.find(normalizedNickname);

    if (nicknameIt != clientsByNickname.end()
        && nicknameIt->second != &client)
    {
        return false;
    }

    if (client.isNicknameReceived())
    {
        const std::string previousNormalizedNickname =
            normalizeNickname(client.getNickname());

        if (previousNormalizedNickname != normalizedNickname)
            removeNicknameIndexEntry(client);
    }

    client.setNickname(nickname);
    client.setNicknameReceived(true);
    clientsByNickname[normalizedNickname] = &client;

    return true;
}

std::string Server::normalizeChannelName(const std::string &channelName) const
{
    return IrcCasemap::normalize(channelName);
}

/**
 * @brief Checks whether a channel name follows the format accepted by this
 * server: a '#' prefix, a total length between 2 and 50 bytes, and no spaces,
 * control characters, commas, or colons.
 *
 * @param channelName The channel name to validate.
 * @return true if the channel name is valid, false otherwise.
 */
bool Server::isValidChannelName(const std::string &channelName) const
{
    const std::size_t maximumChannelNameLength = 50;

    if (channelName.size() < 2
        || channelName.size() > maximumChannelNameLength)
    {
        return false;
    }

    if (channelName[0] != '#')
        return false;

    for (std::string::size_type characterIndex = 1;
        characterIndex < channelName.size();
        ++characterIndex)
    {
        const unsigned char currentCharacter =
            static_cast<unsigned char>(channelName[characterIndex]);

        if (currentCharacter <= 32
            || currentCharacter == 127
            || currentCharacter == ','
            || currentCharacter == ':')
        {
            return false;
        }
    }

    return true;
}

Client *Server::findClientByNickname(const std::string &nickname)
{
    const std::string normalizedNickname = normalizeNickname(nickname);

    if (normalizedNickname.empty())
        return NULL;

    std::map<std::string, Client *>::iterator nicknameIterator =
        clientsByNickname.find(normalizedNickname);

    if (nicknameIterator == clientsByNickname.end())
        return NULL;

    return nicknameIterator->second;
}

Channel *Server::findChannel(const std::string &channelName)
{
    const std::string normalizedChannelName = normalizeChannelName(channelName);

    if (normalizedChannelName.empty())
        return NULL;

    std::map<std::string, Channel>::iterator channelIterator =
        channels.find(normalizedChannelName);

    if (channelIterator == channels.end())
        return NULL;

    return &channelIterator->second;
}

/**
 * @brief Returns the channel identified by the supplied name, creating and
 * storing it when it does not already exist. Invalid channel names return
 * NULL without modifying the server state.
 *
 * The channel map uses the normalized name as its key while the Channel
 * object preserves the original spelling used when it was created.
 *
 * @param channelName The original channel name requested by the client.
 * @return A pointer to the existing or newly created channel, or NULL when
 * the supplied name is invalid.
 */
Channel *Server::findOrCreateChannel(const std::string &channelName)
{
    if (!isValidChannelName(channelName))
        return NULL;

    const std::string normalizedChannelName =
        normalizeChannelName(channelName);

    std::map<std::string, Channel>::iterator channelIterator =
        channels.find(normalizedChannelName);

    if (channelIterator != channels.end())
        return &channelIterator->second;

    const std::pair<std::map<std::string, Channel>::iterator, bool>
        insertionResult = channels.insert(
            std::make_pair(
                normalizedChannelName,
                Channel(channelName)
            )
        );

    return &insertionResult.first->second;
}

/**
 * @brief Adds a client to a channel while keeping the membership state stored
 * by Client and Channel synchronized. Duplicate joins have no effect, a
 * pending invitation is consumed after joining, and the first member becomes
 * a channel operator.
 *
 * This function assumes that all channel access restrictions have already
 * been validated by the caller.
 *
 * @param client The client joining the channel.
 * @param channel The channel the client is joining.
 */
void Server::addClientToChannel(Client &client, Channel &channel)
{
    if (channel.hasMember(&client))
        return;

    const bool channelWasEmpty = channel.isEmpty();

    channel.addMember(&client);
    client.joinChannel(channel.getName());
    channel.removeInvitation(&client);

    if (channelWasEmpty)
        channel.addOperator(&client);
}

/**
 * @brief Removes a client from a channel while keeping the membership state
 * stored by Client and Channel synchronized. Channel operator privileges are
 * removed through Channel::removeMember(), and the channel is erased from the
 * server when it has no remaining members.
 *
 * If the channel is erased, every pointer or reference to that Channel object
 * becomes invalid. The caller must not access the channel after this function
 * returns.
 *
 * @param client The client leaving the channel.
 * @param channel The channel the client is leaving.
 */
void Server::removeClientFromChannel(Client &client, Channel &channel)
{
    if (!channel.hasMember(&client))
        return;

    const std::string channelName = channel.getName();

    channel.removeMember(&client);
    client.leaveChannel(channelName);
    removeChannelIfEmpty(channelName);
}

/**
 * @brief Deletes a channel from the server map when it has no members left.
 */
void Server::removeChannelIfEmpty(const std::string &channelName)
{
    Channel *channel = findChannel(channelName);

    if (channel == NULL || !channel->isEmpty())
        return;

    channels.erase(normalizeChannelName(channelName));
}

/**
 * @brief Removes a disconnecting client from every channel membership and
 * invitation list, then deletes channels that become empty.
 */
void Server::detachClientFromChannels(Client &client)
{
    std::vector<std::string> emptyChannelNames;
    std::map<std::string, Channel>::iterator channelIterator =
        channels.begin();

    while (channelIterator != channels.end())
    {
        Channel &channel = channelIterator->second;

        channel.removeInvitation(&client);

        if (channel.hasMember(&client))
        {
            channel.removeMember(&client);
            client.leaveChannel(channel.getName());

            if (channel.isEmpty())
                emptyChannelNames.push_back(channelIterator->first);
        }

        ++channelIterator;
    }

    std::vector<std::string>::const_iterator emptyChannelIterator =
        emptyChannelNames.begin();

    while (emptyChannelIterator != emptyChannelNames.end())
    {
        channels.erase(*emptyChannelIterator);
        ++emptyChannelIterator;
    }
}

/**
 * @brief Checks whether a client satisfies every access restriction of a
 * channel before joining. It validates the user limit, pending invitation,
 * and channel key in that order, queuing the corresponding numeric error
 * when access is denied.
 *
 * This function does not modify channel membership, operator privileges, or
 * invitations.
 *
 * @param client The client requesting access to the channel.
 * @param channel The channel whose access restrictions will be checked.
 * @param providedKey The key supplied in JOIN, or an empty string when none
 * was supplied.
 * @return true when the client may join, false when access is denied.
 */
bool Server::validateChannelJoinAccess(
    Client &client,
    const Channel &channel,
    const std::string &providedKey
)
{
    if (channel.isLimitEnabled()
        && channel.getMemberCount() >= channel.getUserLimit())
    {
        queueNumericReply(
            client,
            NumericReply::ERR_CHANNELISFULL,
            channel.getName(),
            NumericReply::MSG_CHANNELISFULL
        );
        return false;
    }

    if (channel.isInviteOnly()
        && !channel.hasInvitation(&client))
    {
        queueNumericReply(
            client,
            NumericReply::ERR_INVITEONLYCHAN,
            channel.getName(),
            NumericReply::MSG_INVITEONLYCHAN
        );
        return false;
    }

    if (channel.isKeyEnabled()
        && channel.getKey() != providedKey)
    {
        queueNumericReply(
            client,
            NumericReply::ERR_BADCHANNELKEY,
            channel.getName(),
            NumericReply::MSG_BADCHANNELKEY
        );
        return false;
    }

    return true;
}

/**
 * @brief Queues the current topic state of a channel for one client. It sends
 * RPL_NOTOPIC when the channel has no topic and RPL_TOPIC containing the
 * stored topic otherwise.
 *
 * @param client The client that will receive the numeric reply.
 * @param channel The channel whose topic state will be reported.
 */
void Server::sendChannelTopic(Client &client, const Channel &channel)
{
    if (channel.getTopic().empty())
    {
        queueNumericReply(
            client,
            NumericReply::RPL_NOTOPIC,
            channel.getName(),
            NumericReply::MSG_NOTOPIC
        );
        return;
    }

    queueNumericReply(
        client,
        NumericReply::RPL_TOPIC,
        channel.getName(),
        channel.getTopic()
    );
}

/**
 * @brief Queues the channel member list for one client using one or more
 * RPL_NAMREPLY replies, prefixing channel operators with '@', and finishes
 * the sequence with RPL_ENDOFNAMES.
 *
 * Member names are divided between multiple RPL_NAMREPLY messages when
 * necessary so that every serialized IRC message remains within the
 * 512-byte protocol limit.
 *
 * @param client The client that will receive the member list.
 * @param channel The channel whose members will be listed.
 */
void Server::sendChannelNames(Client &client, const Channel &channel)
{
    std::vector<std::string> nameReplyParameters;

    nameReplyParameters.push_back("=");
    nameReplyParameters.push_back(channel.getName());

    const std::string emptyNamesReply = buildNumericReply(
        NumericReply::RPL_NAMREPLY,
        client,
        nameReplyParameters,
        ""
    );

    const std::size_t maximumNamesLength =
        IRC_MAX_MESSAGE_LENGTH - emptyNamesReply.size();

    std::string currentNames;

    const std::set<Client *> &channelMembers = channel.getMembers();

    std::set<Client *>::const_iterator memberIterator =
        channelMembers.begin();

    while (memberIterator != channelMembers.end())
    {
        Client *channelMember = *memberIterator;

        if (channelMember != NULL
            && !channelMember->getNickname().empty())
        {
            std::string displayedNickname;

            if (channel.hasOperator(channelMember))
                displayedNickname += '@';

            displayedNickname += channelMember->getNickname();

            const std::size_t separatorLength =
                currentNames.empty() ? 0 : 1;

            if (!currentNames.empty()
                && currentNames.size()
                    + separatorLength
                    + displayedNickname.size()
                    > maximumNamesLength)
            {
                queueNumericReply(
                    client,
                    NumericReply::RPL_NAMREPLY,
                    nameReplyParameters,
                    currentNames
                );

                currentNames.clear();
            }

            if (!currentNames.empty())
                currentNames += ' ';

            currentNames += displayedNickname;
        }

        ++memberIterator;
    }

    if (!currentNames.empty())
    {
        queueNumericReply(
            client,
            NumericReply::RPL_NAMREPLY,
            nameReplyParameters,
            currentNames
        );
    }

    queueNumericReply(
        client,
        NumericReply::RPL_ENDOFNAMES,
        channel.getName(),
        NumericReply::MSG_ENDOFNAMES
    );
}

std::string Server::getReplyTarget(const Client &client) const
{
    if (client.getNickname().empty())
        return "*";
    return client.getNickname();
}

std::string Server::buildNumericReply(
    int numericCode,
    const Client &client,
    const std::string &trailingMessage) const
{
    return buildNumericReply(
        numericCode,
        client,
        std::vector<std::string>(),
        trailingMessage
    );
}

std::string Server::buildNumericReply(
    int numericCode,
    const Client &client,
    const std::string &parameter,
    const std::string &trailingMessage) const
{
    std::vector<std::string> parameters;

    if (!parameter.empty())
        parameters.push_back(parameter);

    return buildNumericReply(numericCode, client, parameters, trailingMessage);
}

std::string Server::buildNumericReply(
    int numericCode,
    const Client &client,
    const std::vector<std::string> &parameters,
    const std::string &trailingMessage) const
{
    std::vector<std::string> replyParameters;

    replyParameters.push_back(getReplyTarget(client));
    replyParameters.insert(
        replyParameters.end(),
        parameters.begin(),
        parameters.end()
    );
    replyParameters.push_back(trailingMessage);

    const IrcMessage reply(
        NumericReply::formatCode(numericCode),
        replyParameters,
        getServerPrefix(),
        true
    );

    return reply.serialize();
}

std::string Server::buildNumericReply(
    int numericCode,
    const Client &client,
    const std::vector<std::string> &parameters) const
{
    std::vector<std::string> replyParameters;

    replyParameters.push_back(getReplyTarget(client));
    replyParameters.insert(
        replyParameters.end(),
        parameters.begin(),
        parameters.end()
    );

    const IrcMessage reply(
        NumericReply::formatCode(numericCode),
        replyParameters,
        getServerPrefix(),
        false
    );

    return reply.serialize();
}

/**
 * @brief Receives currently available data from client socket.
 *
 * Appends successfully received bytes to the client's persistent input
 * buffer and processes every complete IRC line. Transient socket errors keep
 * the connection active. Input overflow, an orderly peer shutdown or a fatal
 * receive error request a deferred disconnection with a specific reason.
 *
 * @param descriptorIndex The client's current position in pollFds.
 * @return true when the connection may continue, false when the client must
 * be disconnected.
 */
bool Server::receiveClientData(std::size_t descriptorIndex)
{
    const int clientSocketFd = pollFds[descriptorIndex].fd;

    std::map<int, Client *>::iterator clientIterator =
        clients.find(clientSocketFd);

    if (clientIterator == clients.end()
        || clientIterator->second == NULL)
    {
        throw std::logic_error(
            "received data from an unregistered client"
        );
    }

    Client *client = clientIterator->second;
    char receiveBuffer[RECEIVE_BUFFER_SIZE];

    const ssize_t receivedBytes = ::recv(
        clientSocketFd,
        receiveBuffer,
        sizeof(receiveBuffer),
        0
    );

    if (receivedBytes > 0)
    {
        const std::string receivedData(
            receiveBuffer,
            static_cast<std::size_t>(receivedBytes)
        );

        if (client->getInputBuffer().size()
            + receivedData.size()
            > IRC_MAX_INPUT_BUFFER_SIZE)
        {
            std::cerr << Console::CLIENT
                << " Input buffer limit exceeded ("
                << IRC_MAX_INPUT_BUFFER_SIZE
                << " bytes): fd="
                << clientSocketFd << std::endl;

            client->requestDisconnect(
                "Input buffer limit exceeded"
            );
            return false;
        }

        client->appendToInputBuffer(receivedData);

        /* Temporary phase 3 echo test. */
        // queueMessage(*client, receivedData);

        std::cout << Console::CLIENT
            << " Received "
            << receivedBytes
            << " bytes: fd="
            << clientSocketFd
            << ", buffered="
            << client->getInputBuffer().size()
            << std::endl;

        return processClientBuffer(*client);
    }

    if (receivedBytes == 0)
    {
        client->requestDisconnect(
            "Connection closed by peer"
        );
        return false;
    }

    const int receiveErrno = errno;

    /*
     * These errors are temporary and do not mean that the connection has
     * been lost.
     */
    if (receiveErrno == EAGAIN
        || receiveErrno == EWOULDBLOCK
        || receiveErrno == EINTR)
    {
        return true;
    }

    std::cerr << Console::CLIENT
        << " Receive error: fd="
        << clientSocketFd
        << ", error="
        << std::strerror(receiveErrno)
        << std::endl;

    client->requestDisconnect("Receive error");
    return false;
}

/**
 * The pointer check prevents the accidental deletion of an entry 
 * belonging to another client if the state were out of sync.
 */
void Server::removeNicknameIndexEntry(const Client &client)
{
    if (!client.isNicknameReceived())
        return;

    const std::string normalizedNickname =
        normalizeNickname(client.getNickname());

    std::map<std::string, Client *>::iterator nicknameIterator =
        clientsByNickname.find(normalizedNickname);

    if (nicknameIterator != clientsByNickname.end()
        && nicknameIterator->second == &client)
    {
        clientsByNickname.erase(nicknameIterator);
    }
}

/**
 * @brief Performs the cleanup of a client connection.
 * Removes the client from channels, invitations, operator collections,
 * nickname indexes, the main client map and poll, closes its socket exactly
 * once, and finally destroys the Client object.
 *
 * Calling this function again after the client and descriptor have already
 * been removed has no effect.
 *
 * @param clientSocketFd The socket descriptor that identifies the client.
 * @param reason The reason why the connection is being closed.
 */
void Server::disconnectClient(int clientSocketFd, const std::string &reason)
{
    std::map<int, Client *>::iterator clientIterator =
        clients.find(clientSocketFd);

    std::vector<struct pollfd>::iterator descriptorIterator =
        pollFds.begin();

    if (descriptorIterator != pollFds.end())
        ++descriptorIterator;

    while (descriptorIterator != pollFds.end()
        && descriptorIterator->fd != clientSocketFd)
    {
        ++descriptorIterator;
    }

    if (clientIterator == clients.end()
        && descriptorIterator == pollFds.end())
    {
        return;
    }

    std::string disconnectReason = reason.empty()
        ? "Client disconnected"
        : reason;

    Client *client = NULL;

    if (clientIterator != clients.end())
    {
        client = clientIterator->second;

        if (client != NULL
            && client->isDisconnectRequested()
            && !client->getDisconnectReason().empty())
        {
            disconnectReason = client->getDisconnectReason();
        }

        if (client != NULL)
        {
            detachClientFromChannels(*client);
            removeNicknameIndexEntry(*client);
        }

        clients.erase(clientIterator);
    }

    if (descriptorIterator != pollFds.end())
    {
        closeFd(descriptorIterator->fd);
        pollFds.erase(descriptorIterator);
    }
    else
    {
        int unregisteredSocketFd = clientSocketFd;

        closeFd(unregisteredSocketFd);
    }

    delete client;

    std::cout << Console::CLIENT
        << " Connection closed: fd=" << clientSocketFd
        << ", reason=" << disconnectReason << std::endl;
}

void Server::dispatchCommand(Client &client, const IrcMessage &msg)
{
    dispatcher.execute(client, msg);
    tryRegisterClient(client);
}

/**
 * @brief First call with incomplete data, does nothing.
 * First call with complete data, registers and sends welcome message.
 * Subsequent calls with complete data, does nothing.
 */
void Server::tryRegisterClient(Client &client)
{
    if (client.isRegistered())
        return;

    if (!client.isReadyToRegister())
        return;

    client.setRegistered(true);
    sendWelcomeMessages(client);
}

void Server::sendWelcomeMessages(Client &client)
{
    const std::string clientIdentity = getClientPrefix(client);

    queueNumericReply(
        client,
        NumericReply::RPL_WELCOME,
        NumericReply::welcomeMessage(clientIdentity)
    );

    queueNumericReply(
        client,
        NumericReply::RPL_YOURHOST,
        NumericReply::yourHostMessage(serverName)
    );

    queueNumericReply(
        client,
        NumericReply::RPL_CREATED,
        NumericReply::MSG_CREATED
    );

    std::vector<std::string> myInfoParameters;
    myInfoParameters.push_back(serverName);
    myInfoParameters.push_back(NumericReply::SERVER_VERSION);
    myInfoParameters.push_back(NumericReply::AVAILABLE_USER_MODES);
    myInfoParameters.push_back(NumericReply::AVAILABLE_CHANNEL_MODES);
    queueNumericReply(client, NumericReply::RPL_MYINFO, myInfoParameters);

    std::vector<std::string> isupportParameters;
    isupportParameters.push_back(NumericReply::ISUPPORT_CHANTYPES);
    isupportParameters.push_back(NumericReply::ISUPPORT_PREFIX);
    isupportParameters.push_back(NumericReply::ISUPPORT_CHANMODES);
    isupportParameters.push_back(NumericReply::ISUPPORT_CASEMAPPING);
    queueNumericReply(
        client,
        NumericReply::RPL_ISUPPORT,
        isupportParameters,
        NumericReply::MSG_ISUPPORT
    );
}

/**
 * @brief Runs the non-blocking server event loop.
 *
 * Waits for socket events with poll(), accepts new connections, processes
 * readable and writable client sockets, and routes every definitive client
 * removal through disconnectClient(). Removing a descriptor does not advance
 * the current index because the next descriptor moves into that position.
 */
void Server::run()
{
    if (pollFds.empty())
        throw std::logic_error("no file descriptors registered");

    std::cout << Console::SERVER << " Event loop started" << std::endl;

    while (!SignalHandler::isShutdownRequested())
    {
        const int pollResult = ::poll(
            &pollFds[0],
            static_cast<nfds_t>(pollFds.size()),
            POLL_TIMEOUT_MS
        );

        if (pollResult == -1)
        {
            if (errno == EINTR)
                continue;

            throw createSystemError("poll", errno);
        }

        if (pollResult == 0)
            continue;

        const short listeningEvents = pollFds[0].revents;

        if (listeningEvents & POLLNVAL)
            throw std::runtime_error(
                "listening socket descriptor is invalid"
            );

        if (listeningEvents & (POLLERR | POLLHUP))
            throw std::runtime_error(
                "listening socket reported an error"
            );

        if (listeningEvents & POLLIN)
            acceptClient();

        std::size_t descriptorIndex = 1;

        while (descriptorIndex < pollFds.size())
        {
            const short clientEvents =
                pollFds[descriptorIndex].revents;

            const int clientSocketFd =
                pollFds[descriptorIndex].fd;

            if (clientEvents & POLLNVAL)
            {
                std::cerr << Console::CLIENT
                    << " Invalid descriptor: fd="
                    << clientSocketFd << std::endl;

                disconnectClient(
                    clientSocketFd,
                    "Invalid socket descriptor"
                );
                continue;
            }

            bool clientConnected = true;
            std::string disconnectReason = "Connection closed";

            if (clientEvents & POLLIN)
            {
                clientConnected =
                    receiveClientData(descriptorIndex);

                if (!clientConnected)
                {
                    disconnectReason =
                        "Receive failure or connection closed by peer";
                }
            }

            if (clientConnected && (clientEvents & POLLOUT))
            {
                clientConnected =
                    flushClientOutput(clientSocketFd);

                if (!clientConnected)
                    disconnectReason = "Send failure";
            }

            if (clientEvents & POLLERR)
            {
                disconnectReason = "Socket error";
            }
            else if (clientEvents & POLLHUP)
            {
                disconnectReason = "Connection closed by peer";
            }

            if (!clientConnected
                || (clientEvents & (POLLERR | POLLHUP)))
            {
                disconnectClient(
                    clientSocketFd,
                    disconnectReason
                );
                continue;
            }

            ++descriptorIndex;
        }

        descriptorIndex = 1;

        while (descriptorIndex < pollFds.size())
        {
            const int clientSocketFd =
                pollFds[descriptorIndex].fd;

            std::map<int, Client *>::iterator clientIterator =
                clients.find(clientSocketFd);

            if (clientIterator != clients.end()
                && clientIterator->second != NULL
                && clientIterator->second->isDisconnectRequested())
            {
                const std::string disconnectReason =
                    clientIterator->second->getDisconnectReason();

                disconnectClient(
                    clientSocketFd,
                    disconnectReason
                );
                continue;
            }

            ++descriptorIndex;
        }

        std::cout << Console::SERVER
            << " Ready descriptors: "
            << pollResult << std::endl;
    }
}

/**
 *  @brief Updates the poll events for a client's socket.
 * This function is called when the client's output buffer changes.
 * It updates the events for the client's socket in the pollFds vector.
 * 
 * POLLOUT is added if the output buffer is not empty, indicating that there is data to send.
 * 
 * POLLIN is always included, as the server should always be ready to receive data from the client.
 */
void Server::updateClientPollEvents(int socketFd)
{
    std::map<int, Client *>::iterator clientIterator = clients.find(socketFd);

    if (clientIterator == clients.end())
    {
        throw std::logic_error("cannot update events for an unregistered client");
    }

    Client *client = clientIterator->second;

    for (std::size_t i = 1; i < pollFds.size(); ++i)
    {
        if (pollFds[i].fd != socketFd)
            continue;

        pollFds[i].events = POLLIN;

        if (!client->getOutputBuffer().empty())
        {
            pollFds[i].events = static_cast<short>(pollFds[i].events | POLLOUT);
        }

        return;
    }

    throw std::logic_error("client descriptor is not registered in poll");
}

/**
 * @brief Sends pending output through a non-blocking client socket.
 *
 * Sends at most MAX_SEND_SIZE bytes, removes only the successfully transmitted
 * portion and keeps POLLOUT enabled while data remains. Temporary send errors
 * preserve the connection. Zero-byte writes and definitive socket errors
 * request a deferred disconnection with a specific reason.
 *
 * @param socketFd The socket descriptor of the client being written to.
 * @return true when the connection may continue, false when the client must
 * be disconnected.
 */
bool Server::flushClientOutput(int socketFd)
{
    std::map<int, Client *>::iterator clientIterator = clients.find(socketFd);

    if (clientIterator == clients.end()
        || clientIterator->second == NULL)
    {
        throw std::logic_error("cannot send data to an unregistered client");
    }

    Client *client = clientIterator->second;

    const std::string &pendingOutput = client->getOutputBuffer();

    if (pendingOutput.empty())
    {
        updateClientPollEvents(socketFd);
        return true;
    }

    const std::size_t bytesToSend = pendingOutput.size() > MAX_SEND_SIZE
        ? MAX_SEND_SIZE : pendingOutput.size();

    const ssize_t bytes = ::send(socketFd, pendingOutput.c_str(), bytesToSend, 0);

    if (bytes > 0)
    {
        client->removeSentOutput(static_cast<std::size_t>(bytes));

        updateClientPollEvents(socketFd);

        std::cout << Console::CLIENT << " Sent " << bytes << " bytes: fd=" << socketFd
            << ", pending=" << client->getOutputBuffer().size() << std::endl;

        return true;
    }

    if (bytes == 0)
    {
        client->requestDisconnect("Send returned zero bytes");
        return false;
    }

    const int sendErrno = errno;

    /*
     * These errors are temporary. The pending bytes remain in the output
     * buffer and POLLOUT stays enabled for a later retry.
     */
    if (sendErrno == EAGAIN || sendErrno == EWOULDBLOCK || sendErrno == EINTR)
        return true;

    std::cerr << Console::CLIENT << " Send error: fd=" << socketFd << ", error="
        << std::strerror(sendErrno) << std::endl;

    if (sendErrno == EPIPE)
    {
        client->requestDisconnect("Broken pipe");
    }
    else if (sendErrno == ECONNRESET)
    {
        client->requestDisconnect(
            "Connection reset by peer"
        );
    }
    else
    {
        client->requestDisconnect("Send error");
    }
    
    return false;
}

/**
 * @brief Queues a complete IRC message for non-blocking delivery.
 * Appends the message to the client's output buffer and enables POLLOUT.
 * Handlers must use this instead of writing to the buffer or calling send().
 */
void Server::queueMessage(Client &client, const std::string &message)
{
    if (message.empty() || client.isDisconnectRequested())
        return;

    if (message.size() > IRC_MAX_MESSAGE_LENGTH)
    {
        std::cerr << Console::CLIENT << " IRC reply exceeds "
            << IRC_MAX_MESSAGE_LENGTH << " bytes: fd=" << client.getSocketFd()
            << std::endl;
        client.requestDisconnect();
        return;
    }

    if (client.getOutputBuffer().size() + message.size() > IRC_MAX_OUTPUT_BUFFER_SIZE)
    {
        std::cerr << Console::CLIENT << " Output buffer limit exceeded ("
            << IRC_MAX_OUTPUT_BUFFER_SIZE << " bytes): fd=" << client.getSocketFd()
            << std::endl;
        client.requestDisconnect();
        return;
    }

    client.appendToOutputBuffer(message);
    updateClientPollEvents(client.getSocketFd());
}

/**
 * @brief Queues an IRC message once for every current member of a channel.
 * Null member pointers are ignored defensively. Delivery is performed through
 * queueMessage() so each recipient's output buffer and POLLOUT state remain
 * synchronized.
 *
 * @param channel The channel whose members will receive the message.
 * @param message The complete serialized IRC message to queue.
 */
void Server::queueMessageToChannel(
    const Channel &channel,
    const std::string &message
)
{
    const std::set<Client *> &channelMembers = channel.getMembers();

    std::set<Client *>::const_iterator memberIterator =
        channelMembers.begin();

    while (memberIterator != channelMembers.end())
    {
        Client *channelMember = *memberIterator;

        if (channelMember != NULL)
            queueMessage(*channelMember, message);

        ++memberIterator;
    }
}

/**
 * @brief Queues an IRC message for the source client and once for every other
 * client sharing at least one channel with it, avoiding duplicate delivery
 * when multiple channels are shared.
 */
void Server::queueMessageToRelatedClients(
    Client &sourceClient,
    const std::string &message)
{
    queueMessage(sourceClient, message);

    const std::set<std::string> &sourceChannels =
        sourceClient.getJoinedChannels();

    std::map<int, Client *>::iterator clientIterator = clients.begin();

    while (clientIterator != clients.end())
    {
        Client *relatedClient = clientIterator->second;

        if (relatedClient == &sourceClient)
        {
            ++clientIterator;
            continue;
        }

        const std::set<std::string> &relatedClientChannels =
            relatedClient->getJoinedChannels();

        bool sharesChannel = false;
        std::set<std::string>::const_iterator channelIterator =
            sourceChannels.begin();

        while (channelIterator != sourceChannels.end())
        {
            if (relatedClientChannels.find(*channelIterator)
                != relatedClientChannels.end())
            {
                sharesChannel = true;
                break;
            }

            ++channelIterator;
        }

        if (sharesChannel)
            queueMessage(*relatedClient, message);

        ++clientIterator;
    }
}

void Server::queueNumericReply(
    Client &client,
    int numericCode,
    const std::string &trailingMessage
)
{
    queueMessage(client, buildNumericReply(numericCode, client, trailingMessage));
}

void Server::queueNumericReply(
    Client &client,
    int numericCode,
    const std::string &parameter,
    const std::string &trailingMessage
)
{
    queueMessage(
        client,
        buildNumericReply(numericCode, client, parameter, trailingMessage)
    );
}

void Server::queueNumericReply(
    Client &client,
    int numericCode,
    const std::vector<std::string> &parameters,
    const std::string &trailingMessage
)
{
    queueMessage(
        client,
        buildNumericReply(numericCode, client, parameters, trailingMessage)
    );
}

void Server::queueNumericReply(
    Client &client,
    int numericCode,
    const std::vector<std::string> &parameters
)
{
    queueMessage(client, buildNumericReply(numericCode, client, parameters));
}

bool Server::processClientBuffer(Client &client)
{
    std::string completeLine;

    while (true)
    {
        if (client.isDisconnectRequested())
            return false;

        const Client::LineReadStatus status = client.extractNextLine(completeLine);

        if (status == Client::LINE_INCOMPLETE)
            return !client.isDisconnectRequested();

        if (status == Client::LINE_TOO_LONG)
        {
            std::cerr << Console::CLIENT << " IRC line exceeds "
                << IRC_MAX_MESSAGE_LENGTH << " bytes: fd=" << client.getSocketFd()
                << std::endl;
            return false;
        }

        if (completeLine.empty())
            continue;

        std::cout << Console::CLIENT << " Complete line: fd=" << client.getSocketFd()
            << ", line=\"" << completeLine << "\"" << std::endl;

        try
        {
            const IrcMessage message = MessageParser::parse(completeLine);
            dispatchCommand(client, message);
        }
        catch (const std::invalid_argument &error)
        {
            std::cerr << Console::CLIENT << " Parse error: fd=" << client.getSocketFd()
                << ", reason=" << error.what() << std::endl;
        }
        catch (const std::exception &error)
        {
            std::cerr << Console::ERROR << " Command dispatch error: fd="
                << client.getSocketFd() << ", reason=" << error.what() << std::endl;
            client.requestDisconnect();
            return false;
        }

        if (client.isDisconnectRequested())
            return false;
    }
}

void Server::closeFd(int &fd)
{
    if (fd == INVALID_FD)
        return;
    
    const int fdToClose = fd;

    fd = INVALID_FD; // Evitar cerrar el mismo descriptor varias veces
    
    if (::close(fdToClose) == INVALID_FD)
    {
        std::cerr  << Console::WARNING << " close: " << std::strerror(errno) << std::endl; // los destructores no deben lanzar errores, solo informar
    }
}

void Server::closeAllFds()
{
    std::map<int, Client *>::iterator clientIterator = clients.begin();

    while (clientIterator != clients.end())
    {
        removeNicknameIndexEntry(*clientIterator->second);
        delete clientIterator->second;
        ++clientIterator;
    }

    clients.clear();
    clientsByNickname.clear();

    for (std::size_t i = 0; i < pollFds.size(); ++i)
    {
        closeFd(pollFds[i].fd);
    }
    pollFds.clear();
    listenSocket = -1;
}

Server::~Server()
{
    closeAllFds();

    std::cout << Console::SERVER << " All sockets closed" << std::endl;
}

const std::string &Server::getServerName() const
{
    return serverName;
}

bool Server::isPasswordCorrect(const std::string &providedPassword) const
{
    return providedPassword == password;
}