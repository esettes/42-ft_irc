#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <cstddef>
#include <utility>

/**
 * @file Client.hpp
 * @brief Declares the Client abstraction that owns per-connection state and buffers.
 * 
 * @param socketFd The file descriptor associated with the client's socket connection.
 * @param inputBuffer A buffer that accumulates incoming data from the client until complete lines can be extracted.
 * @param outputBuffer A buffer that holds outgoing data to be sent to the client.
 * @param nickname The client's chosen nickname, which must be unique across the server.
 * @param username The client's username, provided during registration.
 * @param realname The client's real name, provided during registration.
 * @param passwordAccepted A flag indicating whether the client has successfully provided the correct server password.
 * @param registered A flag indicating whether the client has completed the registration process.
 */
class Client
{
    private:
        int socketFd;
        std::string inputBuffer;
        std::string outputBuffer;
        std::string nickname;
        std::string username;
        std::string realname;
        bool passwordAccepted;
        bool registered;

        Client(const Client &other);
        Client &operator=(const Client &other);

    public:
        explicit Client(int socketFd);
        ~Client();
        
        int getSocketFd() const;

        void appendToInputBuffer(const std::string &data);
        const std::string &getInputBuffer() const;
        void appendToOutputBuffer(const std::string &data);
        const std::string &getOutputBuffer() const;

        bool isReadyToRegister() const;
        bool isRegistered() const;
        bool isPasswordAccepted() const;
        void setPasswordAccepted(bool accepted);
        void setRegistered(bool registered);

        void setNickname(const std::string &nickname);
        const std::string &getNickname() const;
        void setUsername(const std::string &username);
        const std::string &getUsername() const;
        void setRealname(const std::string &realname);
        const std::string &getRealname() const;

        void removeSentOutput(std::size_t sentByteCount);

        bool extractNextLine(std::string &completeLine);
};

#endif
