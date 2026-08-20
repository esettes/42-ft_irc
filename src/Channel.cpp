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

/**
 * @brief Adds a non-null client to the channel membership.
 * Repeated insertions have no effect because members is a std::set.
 */
void Channel::addMember(Client *client)
{
    if (client == NULL)
        return;

    members.insert(client);
}

/**
 * @brief Removes a client from every collection that requires channel
 * membership.
 */
void Channel::removeMember(Client *client)
{
    if (client == NULL)
        return;

    operators.erase(client);
    invitedClients.erase(client);
    members.erase(client);
}

/**
 * @brief Checks whether a client belongs to the channel.
 */
bool Channel::hasMember(const Client *client) const
{
    if (client == NULL)
        return false;

    return members.find(const_cast<Client *>(client)) != members.end();
}

/**
 * @brief Returns the number of clients currently in the channel.
 */
std::size_t Channel::getMemberCount() const
{
    return members.size();
}

/**
 * @brief Checks whether the channel currently has no members.
 */
bool Channel::isEmpty() const
{
    return members.empty();
}

/**
 * @brief Returns the channel membership collection for read-only iteration.
 */
const std::set<Client *> &Channel::getMembers() const
{
    return members;
}

/**
 * @brief Grants channel operator privileges to an existing member (MODE +o).
 */
void Channel::addOperator(Client *client)
{
    if (client == NULL || !hasMember(client))
        return;

    operators.insert(client);
}

/**
 * @brief Revokes a client's channel operator privileges without removing
 * channel membership (MODE -o).
 */
void Channel::removeOperator(Client *client)
{
    if (client == NULL)
        return;

    operators.erase(client);
}

/**
 * @brief Checks whether a client has operator privileges in this channel.
 * @param client The client to check.
 * @return true if the client has operator privileges, false otherwise.
 */
bool Channel::hasOperator(const Client *client) const
{
    if (client == NULL)
        return false;

    return operators.find(const_cast<Client *>(client)) != operators.end();
}

/**
 * @brief Adds a non-member client to the channel invitation list(MODE +i).
 */
void Channel::inviteClient(Client *client)
{
    if (client == NULL || hasMember(client))
        return;

    invitedClients.insert(client);
}

/**
 * @brief Removes a client's pending invitation to the channel(MODE -i).
 * Its a pending invitation not a permanent privilege.
 */
void Channel::removeInvitation(Client *client)
{
    if (client == NULL)
        return;

    invitedClients.erase(client);
}

/**
 * @brief Checks whether a client has a pending invitation to the channel.
 */
bool Channel::hasInvitation(const Client *client) const
{
    if (client == NULL)
        return false;

    return invitedClients.find(const_cast<Client *>(client))
        != invitedClients.end();
}

/**
 * @brief Checks whether the channel only accepts invited clients.
 */
bool Channel::isInviteOnly() const
{
    return inviteOnly;
}

/**
 * @brief Enables or disables the channel invite-only mode.
 * 
 * MODE +i: Only invited clients can join the channel.
 * 
 * MODE -i: Any client can join the channel.
 */
void Channel::setInviteOnly(bool enabled)
{
    inviteOnly = enabled;
}

/**
 * @brief Checks whether topic changes are restricted to channel operators.
 */
bool Channel::isTopicRestricted() const
{
    return topicRestricted;
}

/**
 * @brief Enables or disables the channel topic restriction.
 * 
 * MODE +t: Only channel operators can change the topic.
 * 
 * MODE -t: Any channel member can change the topic.
 */
void Channel::setTopicRestricted(bool enabled)
{
    topicRestricted = enabled;
}

/**
 * @brief Checks whether joining the channel requires a key.
 */
bool Channel::isKeyEnabled() const
{
    return keyEnabled;
}

/**
 * @brief Returns the key currently required to join the channel.
 */
const std::string &Channel::getKey() const
{
    return channelKey;
}

/**
 * @brief Sets a non-empty channel key and enables key mode.
 */
void Channel::setKey(const std::string &newKey)
{
    if (newKey.empty())
        return;

    channelKey = newKey;
    keyEnabled = true;
}

/**
 * @brief Removes the stored channel key and disables key mode.
 */
void Channel::removeKey()
{
    channelKey.clear();
    keyEnabled = false;
}
