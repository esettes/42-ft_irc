#ifndef CHANNEL_HPP
# define CHANNEL_HPP

#include <string>
#include <set>

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