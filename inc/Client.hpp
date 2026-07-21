#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

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

};

#endif
