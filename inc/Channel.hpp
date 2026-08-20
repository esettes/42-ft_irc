#ifndef CHANNEL_HPP
# define CHANNEL_HPP

#include <string>
#include <set>

class Client;

/**
 * @file Channel.hpp
 * @brief Declares the model that stores the state, membership, privileges,
 * invitations, and modes of an IRC channel.
 * 
 * @param keyEnabled Allow channel protection with password (MODE +k).
 * @param limitEnabled Allow channel user limit (MODE +l).
 * @param topicRestricted Allow only channel operators to set the channel topic (MODE +t).
 * @param inviteOnly Allow only invited clients to join the channel (MODE +i).
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

        void addOperator(Client *client);
        void removeOperator(Client *client);
        bool hasOperator(const Client *client) const;

        void inviteClient(Client *client);
        void removeInvitation(Client *client);
        bool hasInvitation(const Client *client) const;
        bool isInviteOnly() const;
        void setInviteOnly(bool enabled);

        bool isTopicRestricted() const;
        void setTopicRestricted(bool enabled);

        bool isKeyEnabled() const;
        const std::string &getKey() const;
        void setKey(const std::string &newKey);
        void removeKey();
};

#endif