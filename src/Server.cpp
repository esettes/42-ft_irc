#include "Server.hpp"
#include "Console.hpp"
#include "MessageParser.hpp"
#include <netdb.h>


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
    return ":" + serverName;
}

std::string Server::getClientPrefix(const Client &client) const
{
    return ":" + client.getNickname() + "!" + client.getUsername() + "@" + client.getHost();
}

std::string Server::normalizeNickname(const std::string &nickname) const
{
    return IrcCasemap::normalize(nickname);
}

std::string Server::normalizeChannelName(const std::string &channelName) const
{
    return IrcCasemap::normalize(channelName);
}

Client *Server::findClientByNickname(const std::string &nickname)
{
    const std::string normalizedNickname = normalizeNickname(nickname);

    if (normalizedNickname.empty())
        return NULL;

    std::map<int, Client *>::iterator clientIterator = clients.begin();

    while (clientIterator != clients.end())
    {
        Client *client = clientIterator->second;

        if (client->isNicknameReceived()
            && normalizeNickname(client->getNickname()) == normalizedNickname)
        {
            return client;
        }
        ++clientIterator;
    }

    return NULL;
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

std::string Server::getReplyTarget(const Client &client) const
{
    if (client.getNickname().empty())
        return "*";
    return client.getNickname();
}

std::string Server::buildNumericReply(
    int numericCode,
    const Client &client,
    const std::string &trailingMessage
) const
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
    const std::string &trailingMessage
) const
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
    const std::string &trailingMessage
) const
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
        serverName,
        true
    );

    return reply.serialize();
}

std::string Server::buildNumericReply(
    int numericCode,
    const Client &client,
    const std::vector<std::string> &parameters
) const
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
        serverName,
        false
    );

    return reply.serialize();
}

bool Server::receiveClientData(std::size_t descriptorIndex)
{
    const int clientSocketFd = pollFds[descriptorIndex].fd;

    char receiveBuffer[RECEIVE_BUFFER_SIZE];

    const ssize_t receivedBytes = ::recv(clientSocketFd, receiveBuffer, sizeof(receiveBuffer), 0);

    if (receivedBytes > 0)
    {
        std::map<int, Client *>::iterator clientIterator = clients.find(clientSocketFd);
        if (clientIterator == clients.end())
        {
            throw std::logic_error("received data from an unregistered client");
        }

        Client *client = clientIterator->second;
        const std::string receivedData(receiveBuffer, static_cast<std::size_t>(receivedBytes));

        if (client->getInputBuffer().size() + receivedData.size() > IRC_MAX_INPUT_BUFFER_SIZE)
        {
            std::cerr << Console::CLIENT << " Input buffer limit exceeded ("
                << IRC_MAX_INPUT_BUFFER_SIZE << " bytes): fd=" << clientSocketFd
                << std::endl;
            return false;
        }

        client->appendToInputBuffer(receivedData);

        /* Temporary phase 3 echo test. */
        //queueMessage(*client, receivedData);

        std::cout << Console::CLIENT << " Received " << receivedBytes << " bytes: fd=" << clientSocketFd 
            << ", buffered=" << client->getInputBuffer().size() << std::endl;

        return processClientBuffer(*client);
    }

    if (receivedBytes == 0)
        return false;

    const int receiveErrno = errno;

    // no hay datos disponibles o interrumpido por señal
    if (receiveErrno == EAGAIN || receiveErrno == EWOULDBLOCK || receiveErrno == EINTR)
        return true;

    std::cerr << Console::CLIENT << " Receive error: fd=" << clientSocketFd
        << ", error=" << std::strerror(receiveErrno) << std::endl;

    return false;
}

void Server::removeClient(std::size_t descriptorIndex)
{
    const int clientSocketFd = pollFds[descriptorIndex].fd;
    std::map<int, Client *>::iterator clientIterator = clients.find(clientSocketFd);

    if (clientIterator != clients.end())
    {
        delete clientIterator->second;
        clients.erase(clientIterator);
    }
    if (::close(clientSocketFd) == -1)
    {
        const int closeErrno = errno;

        std::cerr << Console::CLIENT << " Close error: fd=" << clientSocketFd
            << ", error=" << std::strerror(closeErrno) << std::endl;
    }

    pollFds.erase(pollFds.begin() + descriptorIndex);

    std::cout << Console::CLIENT << " Connection closed: fd=" << clientSocketFd << std::endl;
}

void Server::dispatchCommand(Client &client, const IrcMessage &msg)
{
    dispatcher.execute(client, msg);
    tryRegisterClient(client);
}

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
    const std::string clientIdentity = getClientPrefix(client).substr(1);

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

void Server::run()
{
    if (pollFds.empty())
        throw std::logic_error("no file descriptors registered");

    std::cout << Console::SERVER << " Event loop started" << std::endl;

    while (!SignalHandler::isShutdownRequested())
    {
        const int pollResult = ::poll(&pollFds[0], static_cast<nfds_t>(pollFds.size()), POLL_TIMEOUT_MS);

        if (pollResult == -1)
        {
            if (errno == EINTR)
                continue;

            throw createSystemError("poll", errno);
        }

        if (pollResult == 0)
            continue;
        
        const short listeningEvents = pollFds[0].revents; // eventos que ocurrieron en listenSocket

        if (listeningEvents & POLLNVAL)
        {
            throw std::runtime_error("listening socket descriptor is invalid");
        }

        if (listeningEvents & (POLLERR | POLLHUP))
        {
            throw std::runtime_error("listening socket reported an error");
        }

        if (listeningEvents & POLLIN) // descriptor tiene datos para leer (nuevo cliente)
            acceptClient();

        std::size_t i = 1;

        while (i < pollFds.size())
        {
            const short clientEvents = pollFds[i].revents;

            const int clientSocketFd = pollFds[i].fd;

            if (clientEvents & POLLNVAL)
            {
                std::cerr << Console::CLIENT << " Invalid descriptor: fd=" << clientSocketFd << std::endl;
                removeClient(i);
                continue;
            }

            bool connectedClient = true;

            if (clientEvents & POLLIN)
            {
                connectedClient = receiveClientData(i);
            }
            if (connectedClient && (clientEvents & POLLOUT))
            {
                connectedClient = flushClientOutput(clientSocketFd);
            }
            if (!connectedClient || (clientEvents & (POLLERR | POLLHUP)))
            {
                removeClient(i);
                continue;
            }
            ++i;
        }

        i = 1;
        while (i < pollFds.size())
        {
            const int clientSocketFd = pollFds[i].fd;
            std::map<int, Client *>::iterator clientIterator = clients.find(clientSocketFd);

            if (clientIterator != clients.end()
                && clientIterator->second->isDisconnectRequested())
            {
                std::cerr << Console::CLIENT
                    << " Disconnecting abusive or slow client: fd="
                    << clientSocketFd << std::endl;
                removeClient(i);
                continue;
            }
            ++i;
        }

        std::cout << Console::SERVER << " Ready descriptors: " << pollResult << std::endl;
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

bool Server::flushClientOutput(int socketFd)
{
    std::map<int, Client *>::iterator clientIterator = clients.find(socketFd);

    if (clientIterator == clients.end())
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

    const ssize_t bytes = ::send(socketFd, pendingOutput.c_str(), bytesToSend,0);

    if (bytes > 0)
    {
        client->removeSentOutput(static_cast<std::size_t>(bytes));

        updateClientPollEvents(socketFd);

        std::cout << Console::CLIENT << " Sent " << bytes << " bytes: fd=" << socketFd
            << ", pending=" << client->getOutputBuffer().size() << std::endl;

        return true;
    }

    if (bytes == 0)
        return false;

    const int sendErrno = errno;

    if (sendErrno == EAGAIN || sendErrno == EWOULDBLOCK || sendErrno == EINTR)
        return true;

    std::cerr << Console::CLIENT << " Send error: fd=" << socketFd << ", error="
        << std::strerror(sendErrno) << std::endl;

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
        throw std::runtime_error("IRC message exceeds 512 bytes");

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
        delete clientIterator->second;
        ++clientIterator;
    }

    clients.clear();

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
