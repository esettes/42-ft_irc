#include "Server.hpp"

#include <stdexcept>

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

Server::Server(int port, const std::string &password)
    : port(port), password(password), listenSocket(-1), dispatcher(*this)
{
    //createListeningSocket();
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
        const int pollTimeout = -1; // Esperar indefinidamente
        int pollResult = poll(pollFds.data(), pollFds.size(), pollTimeout);
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
        while (i < pollFds.size())
        {
            const int socketFd = pollFds[i].fd;
            const short returnedEvents = pollFds[i].revents;
            if (returnedEvents == 0)
            {
                // No hay eventos para este socket, pasar al siguiente
                ++i;
                continue;
            }
            if (socketFd == listenSocket)
            {
                if ((returnedEvents & POLLIN) != 0)
                    acceptClient(); // 

                if ((returnedEvents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
                    throw std::runtime_error("Listening socket stopped working.");

                ++i;
                continue ;
            }
            if ((returnedEvents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
                disconnectClient(socketFd);
                continue;

            if ((returnedEvents & POLLIN) != 0)
                receiveFromClient(socketFd);

            if (clients.find(socketFd) == clients.end())
                continue;

            if ((returnedEvents & POLLOUT) != 0)
                flushClientOutput(socketFd);

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
