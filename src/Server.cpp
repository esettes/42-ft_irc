#include "Server.hpp"

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

    std::runtime_error createSystemError(
        const std::string &operation,
        int errorNumber
    )
    {
        return std::runtime_error(
            operation + ": " + std::strerror(errorNumber)
        );
    }
}

Server::Server(int port, const std::string &password)
    : port(port), 
    password(password), 
    listenSocket(INVALID_FD), 
    dispatcher(*this)
{
    if (port < 1 || port > 65535)
        throw std::invalid_argument("invalid server port");

    if (password.empty())
    {
        throw std::invalid_argument(
            "password cannot be empty");
    }
    createListeningSocket();
}

/** @brief Creates the listening socket.
 * Configures the socket to be non-blocking and sets the SO_REUSEADDR option.
 * Describes the server's address and binds the socket to it.
 * Starts listening for incoming connections.
*/
void Server::createListeningSocket()
{
    listenSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_FD)
        throw createSystemError("socket", errno);

    int reuseAddr = 1;
    if (::setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr)) == -1)
    {
        const int errorNumber = errno;

        closeFd(listenSocket);
        throw createSystemError("setsockopt", errorNumber);
    }
    const int currentSocketFlags = ::fcntl(listenSocket, F_GETFL, 0);

    if (currentSocketFlags == -1)
    {
        const int errorNumber = errno;
        closeFd(listenSocket);
        throw createSystemError("fcntl F_GETFL", errorNumber);
    }
    if (::fcntl(listenSocket, F_SETFL, currentSocketFlags | O_NONBLOCK) == -1)
    {
        const int errorNumber = errno;
        closeFd(listenSocket);
        throw createSystemError("fcntl F_SETFL", errorNumber);
    }

    struct sockaddr_in serverAddress;
    std::memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET; // ipv4
    serverAddress.sin_addr.s_addr = htons(INADDR_ANY);  // accepts connections that are directed to any ipv4 interface of the server machine
    serverAddress.sin_port = htons(static_cast<unsigned short>(port));  // listen port
}

void Server::dispatchCommand(Client &client, const IrcMessage &msg)
{
    dispatcher.execute(client, msg);
}

void Server::run()
{
    while (true)
    {
        // Esperar eventos con poll()
        //const int pollTimeout = -1; // Esperar indefinidamente
        int pollResult = poll(pollFds.data(), pollFds.size(), POLL_TIMEOUT_MS);
        if (pollResult < 0)
        {
            if (pollResult == EINTR)
            {
                // Señal interrumpió poll(), continuar esperando
                continue;
            }
            throw std::runtime_error("Error en poll(): ");// + std::strerror(errno));
        }
        // Procesar eventos de clientes
        std::size_t i = 0;
        while (i < pollFds.size())  // num de eventos a vigilar por poll()
        {
            const int socketFd = pollFds[i].fd;
            const short returnedEvents = pollFds[i].revents;    // eventos que ocurrieron en este socket
            if (returnedEvents == 0)
            {
                // No hay eventos para este socket, pasar al siguiente
                ++i;
                continue;
            }
            if (socketFd == listenSocket)
            {
                // if (returnedEvents & POLLIN)    // descriptor tiene datos para leer (nuevo cliente)
                //     acceptClient(); 

                if (returnedEvents & POLLOUT)   // descriptor listo para escribir (no debería ocurrir en listenSocket)
                    throw std::runtime_error("Unexpected POLLOUT event on listening socket.");

                if (returnedEvents & (POLLERR | POLLHUP | POLLNVAL))    // ERROR
                    throw std::runtime_error("Listening socket stopped working.");

                ++i;
                continue ;
            }
            if (returnedEvents & (POLLERR | POLLHUP | POLLNVAL))
            {
                //disconnectClient(socketFd);
                continue;
            }

            // if (returnedEvents & POLLIN)
            //     receiveFromClient(socketFd);

            if (clients.find(socketFd) == clients.end())
                continue;

            // if (returnedEvents & POLLOUT)
            //     flushClientOutput(socketFd);

            if (clients.find(socketFd) == clients.end())
                continue;
            ++i;
        // Aceptar nuevos clientes
        // Leer datos de clientes
        // Procesar comandos de clientes
        // Enviar respuestas a clientes
        }
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
        std::cerr << "Warning: close: " << std::strerror(errno) << std::endl; // los destructores no deben lanzar errores, solo informar
    }
}

void Server::closeAllFds()
{
    // for (std::size_t i = 0; i < pollFds.size(); ++i)
    // {
    //     closeFd(pollFds[i].fd);
    // }
    closeFd(listenSocket);
}

Server::~Server()
{
    closeAllFds();
}

