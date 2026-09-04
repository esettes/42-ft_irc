#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <cstddef>
#include <utility>
#include <set>

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
 * @param host The client's hostname if available, otherwise its IP address.
 * @param passwordAccepted A flag indicating whether the client has successfully provided the correct server password.
 * @param nicknameReceived A flag indicating whether the client has provided a nickname.
 * @param usernameReceived A flag indicating whether the client has provided a username.
 * @param registered A flag indicating whether the client has completed the registration process.
 * @param joinedChannels Normalized channel keys for membership lookups.
 * @param isOperator Whether the client has global operator status (legacy; per-channel ops live on Channel).
 * @param disconnectRequested Whether the client is waiting to be disconnected.
 * @param disconnectReason The reason associated with the first disconnection request.
 */
class Client
{
    friend class Server;

    private:
        int socketFd;
        std::string inputBuffer;
        std::string outputBuffer;
        std::string nickname;
        std::string username;
        std::string realname;
        std::string host;
        bool passwordAccepted;
        bool nicknameReceived;
        bool usernameReceived;
        bool registered;
        std::set<std::string> joinedChannels;
        bool isOperator;
        bool disconnectRequested;
        std::string disconnectReason;

        Client(const Client &other);
        Client &operator=(const Client &other);

        void appendToOutputBuffer(const std::string &data);
        void removeSentOutput(std::size_t sentByteCount);

    public:
        enum LineReadStatus
        {
            LINE_INCOMPLETE = 0,
            LINE_COMPLETE = 1,
            LINE_TOO_LONG = -1
        };

        Client(int socketFd, const std::string &host);
        ~Client();
        
        int getSocketFd() const;
        bool isVirtual() const;

        void appendToInputBuffer(const std::string &data);
        const std::string &getInputBuffer() const;
        const std::string &getOutputBuffer() const;

        bool isReadyToRegister() const;
        bool isRegistered() const;
        bool isPasswordAccepted() const;
        bool isNicknameReceived() const;
        bool isUsernameReceived() const;
        void setPasswordAccepted(bool accepted);
        void setNicknameReceived(bool received);
        void setUsernameReceived(bool received);
        void setRegistered(bool registered);
        void setIsOperator(bool isOperator);
        bool getIsOperator() const;

        void requestDisconnect();
        void requestDisconnect(const std::string &reason);
        bool isDisconnectRequested() const;
        const std::string &getDisconnectReason() const;

        void joinChannel(const std::string &channelName);
        void leaveChannel(const std::string &channelName);
        bool isInChannel(const std::string &channelName) const;
        const std::set<std::string> &getJoinedChannels() const;

        void setNickname(const std::string &nickname);
        const std::string &getNickname() const;
        void setUsername(const std::string &username);
        const std::string &getUsername() const;
        void setRealname(const std::string &realname);
        const std::string &getRealname() const;
        const std::string &getHost() const;

        LineReadStatus extractNextLine(std::string &completeLine);

};

#endif
