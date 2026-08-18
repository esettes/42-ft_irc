#ifndef CHANNEL_HPP
# define CHANNEL_HPP

#include <string>
#include <set>

class Client;

/**
 * @file Channel.hpp
 * @brief Declares the model that stores the state, membership, privileges,
 * invitations, and modes of an IRC channel.
 */
class Channel
{
    private:
        std::string name;
        std::string topic;
        
        std::set<Client *> members;
        std::set<Client *> operators;
        std::set<Client *> invitedClients;

        bool inviteOnly;
        bool topicRestricted;
        bool keyEnabled;
        bool limitEnabled;

        std::string channelKey;
        std::size_t userLimit;

    public:
        explicit Channel(const std::string &channelName);
        ~Channel();
        const std::string &getName() const;
        const std::string &getTopic() const;
        void setTopic(const std::string &newTopic);
        void addMember(Client *client);
        void removeMember(Client *client);
        bool hasMember(const Client *client) const;
        std::size_t getMemberCount() const;
        bool isEmpty() const;
        const std::set<Client *> &getMembers() const;
};

#endif