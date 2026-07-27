#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <cstddef>
#include <utility>

/** @brief Represents an accepted connection by the server */
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

        void removeSentOutput(std::size_t bytesSendt);
};

#endif
