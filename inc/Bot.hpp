// Copyright 2026 @esettes, @danielfdez17
#ifndef INC_BOT_HPP_
#define INC_BOT_HPP_

#include <ctime>
#include <string>

class Server;
class Client;
class Channel;

/**
 * @file Bot.hpp
 * @brief Built-in IRC helper bot implemented as a virtual user without a
 *        socket, so it does not consume poll() descriptors.
 *
 * The bot registers a reserved nickname, sits in a home channel, answers
 * private commands, and can be invited into other channels.
 */
class Bot {
 private:
        Server &server;
        Client *user;
        std::time_t startedAt;

        Bot(const Bot &other);
        Bot &operator=(const Bot &other);

        void registerIdentity();
        void joinHomeChannel();
        void joinChannel(Channel &channel, bool greet);
        void broadcastJoin(Channel &channel);
        void sendPrivateReply(Client &recipient, const std::string &text);
        void sendChannelReply(Channel &channel, const std::string &text);
        void sendNotice(Client &recipient, const std::string &text);
        void sendText(
            Client *recipient,
            Channel *channel,
            const std::string &command,
            const std::string &text);

        std::string trim(const std::string &text) const;
        std::string toLowerAscii(const std::string &text) const;
        bool consumeBotMention(std::string &text) const;
        bool extractCommand(
            const std::string &text,
            bool fromChannel,
            std::string &command,
            std::string &argument) const;
        bool handleCtcp(Client &sender, const std::string &text);
        std::string runCommand(
            Client &sender,
            const std::string &command,
            const std::string &argument);
        std::string formatNow(const char *format) const;
        std::string formatUptime() const;
        std::string clipTrailing(
            const std::string &command,
            const std::string &target,
            const std::string &text) const;

 public:
        static const char *NICKNAME;
        static const char *USERNAME;
        static const char *REALNAME;
        static const char *HOST;
        static const char *HOME_CHANNEL;
        static const char *HOME_TOPIC;

        explicit Bot(Server &server);
        ~Bot();

        void start();
        void stop();

        bool owns(const Client &client) const;
        Client *getClient() const;

        void handleDirectMessage(Client &sender, const std::string &text);
        void handleChannelMessage(
            Client &sender,
            Channel &channel,
            const std::string &text);
        void handleInvite(Channel &channel);
        void handleKick(const std::string &channelName);
};

#endif  // INC_BOT_HPP_
