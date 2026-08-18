#include "Channel.hpp"

Channel::Channel(const std::string &channelName)
    : name(channelName),
      topic(""),
      members(),
      operators(),
      invitedClients(),
      inviteOnly(false),
      topicRestricted(false),
      keyEnabled(false),
      limitEnabled(false),
      channelKey(""),
      userLimit(0)
{
}

Channel::~Channel()
{
}

const std::string &Channel::getName() const
{
    return name;
}

const std::string &Channel::getTopic() const
{
    return topic;
}

void Channel::setTopic(const std::string &newTopic)
{
    topic = newTopic;
}
