#ifndef CHANNEL_HPP
# define CHANNEL_HPP

#include <string>
#include <set>

/**
 * @file Channel.hpp
 * @brief Defines the Channel model used to track IRC channel state, modes, and membership.
 * 
 * @param name The name of the channel (e.g., "#general").
 * @param topic The topic of the channel, which can be set by operators.
 * @param key The optional key (password) required to join the channel.
 * @param inviteOnly Whether the channel is invite-only (mode +i).
 * @param topicRestricted Whether only operators can set the topic (mode +t).
 * @param hasUserLimit Whether the channel has a user limit (mode +l).
 * @param userLimit The maximum number of users allowed in the channel if hasUserLimit is true.
 * @param memberFds A set of file descriptors representing the clients who are members of the channel.
 * @param operatorFds A set of file descriptors representing the clients who are operators of the channel.
 * @param invitedFds A set of file descriptors representing the clients who have
 */
class Channel
{
    private:
        std::string name;
        std::string topic;
        std::string key;

        bool inviteOnly;
        bool topicRestricted;
        bool hasUserLimit;
        std::size_t userLimit;

        std::set<int> memberFds;
        std::set<int> operatorFds;
        std::set<int> invitedFds;
};

#endif