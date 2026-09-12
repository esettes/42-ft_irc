# Phase 11 — Channel model

## Goal

Implement the `Channel` class, responsible for representing the state of an IRC channel.

This phase prepares the structure needed to later implement:

- `JOIN`
- `PART`
- `PRIVMSG`
- `TOPIC`
- `INVITE`
- `KICK`
- `MODE`
- `QUIT`

In this phase the data model and the basic operations on channel members and modes must mainly be defined.

---

## Channel structure

Each channel must store:

```text
Channel
├── name
├── topic
├── members
├── operators
├── invited clients
├── invite-only mode
├── topic-restricted mode
├── key
└── user limit
```

A possible initial declaration would be:

```cpp
class Client;

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

    // {...}
};

#endif
```

The forward declaration:

```cpp
class Client;
```

lets you store pointers to clients without including the full `Client` definition inside `Channel.hpp`.

---

## Channel constructor

The constructor must store the name and initialize every mode as disabled:

```cpp
Channel::Channel(const std::string &channelName)
    : name(channelName),
      topic(""),
      inviteOnly(false),
      topicRestricted(false),
      keyEnabled(false),
      limitEnabled(false),
      channelKey(""),
      userLimit(0)
{
}
```

The channel starts:

- Without a topic.
- Without members.
- Without operators.
- Without invitations.
- Without a password.
- Without a user limit.
- With every mode disabled.

---

## Channel name

The channel must store its full name:

```cpp
std::string name;
```

Usual valid examples:

```text
#general
#programming
#irc
```

The name must be used as a unique identifier inside the server.

Full name validation can be performed in the `JOIN` command, before creating the channel.

---

## Channel topic

The topic is stored in:

```cpp
std::string topic;
```

It may be empty if none has been defined yet.

Required operations:

```cpp
const std::string &Channel::getTopic() const;
void Channel::setTopic(const std::string &newTopic);
```

Mode `+t` will later determine whether only operators can change it.

---

## Channel members

Clients connected to the channel can be stored through:

```cpp
std::set<Client *> members;
```

Using `std::set` prevents the same client from appearing more than once.

Basic operations:

```cpp
void Channel::addMember(Client *client);
void Channel::removeMember(Client *client);
bool Channel::hasMember(const Client *client) const;
std::size_t Channel::getMemberCount() const;
bool Channel::isEmpty() const;
```

Implementation example:

```cpp
void Channel::addMember(Client *client)
{
    if (client != NULL)
        members.insert(client);
}

void Channel::removeMember(Client *client)
{
    members.erase(client);
}

bool Channel::hasMember(const Client *client) const
{
    return members.find(const_cast<Client *>(client)) != members.end();
}

std::size_t Channel::getMemberCount() const
{
    return members.size();
}

bool Channel::isEmpty() const
{
    return members.empty();
}
```

The channel must not delete the `Client` objects.

The pointers only represent relationships with clients managed by the server.

---

## Channel operators

Operators can be stored in another set:

```cpp
std::set<Client *> operators;
```

An operator must always also be a member of the channel.

Before adding an operator it must be checked:

```cpp
void Channel::addOperator(Client *client)
{
    if (client != NULL && hasMember(client))
        operators.insert(client);
}
```

There must also be operations to query and remove privileges:

```cpp
bool Channel::hasOperator(const Client *client) const;
void Channel::removeOperator(Client *client);
```

When a client leaves the channel, it must also be removed from the operator set.

```cpp
void Channel::removeMember(Client *client)
{
    operators.erase(client);
    invitedClients.erase(client);
    members.erase(client);
}
```

---

## First channel operator

The first user who enters a new channel must automatically become an operator.

The `JOIN` flow will be:

```text
The client requests JOIN
        ↓
The server looks up the channel
        ↓
If it does not exist, it creates the channel
        ↓
It adds the client as a member
        ↓
If they were the first member, it makes them an operator
```

Example:

```cpp
bool channelWasEmpty = channel.isEmpty();

channel.addMember(&client);

if (channelWasEmpty)
    channel.addOperator(&client);
```

It is not a good idea to check whether the channel is empty after adding the client, because at that moment it already contains a member.

---

## Invited clients

Invited clients are stored in:

```cpp
std::set<Client *> invitedClients;
```

Required operations:

```cpp
void Channel::inviteClient(Client *client);
void Channel::removeInvitation(Client *client);
bool Channel::hasInvitation(const Client *client) const;
```

This collection will be used by:

```text
INVITE
JOIN
MODE +i
```

When an invited client enters the channel correctly, their invitation must be consumed:

```cpp
channel.removeInvitation(&client);
```

An invitation does not automatically make the client a member. It only lets them bypass the `+i` restriction.

---

## Mandatory modes

The channel must store the state of the modes required by the project:

| Mode | Internal state | Function |
|---|---|---|
| `+i` | `inviteOnly` | Only invited clients can enter |
| `+t` | `topicRestricted` | Only operators can change the topic |
| `+k` | `keyEnabled` and `channelKey` | Requires a password to enter |
| `+l` | `limitEnabled` and `userLimit` | Limits the number of members |
| `+o` | `operators` | Grants or removes operator privileges |

---

## Invite mode: `+i`

State:

```cpp
bool inviteOnly;
```

Methods:

```cpp
bool Channel::isInviteOnly() const
{
    return inviteOnly;
}

void Channel::setInviteOnly(bool enabled)
{
    inviteOnly = enabled;
}
```

When it is enabled, `JOIN` must allow entry only if the client appears in `invitedClients`.

---

## Topic restriction: `+t`

State:

```cpp
bool topicRestricted;
```

Methods:

```cpp
bool Channel::isTopicRestricted() const
{
    return topicRestricted;
}

void Channel::setTopicRestricted(bool enabled)
{
    topicRestricted = enabled;
}
```

When it is enabled, only an operator can change the channel topic.

When it is disabled, any member can change it.

---

## Channel password: `+k`

States:

```cpp
bool keyEnabled;
std::string channelKey;
```

Methods:

```cpp
bool Channel::isKeyEnabled() const
{
    return keyEnabled;
}

const std::string &Channel::getKey() const
{
    return channelKey;
}

void Channel::setKey(const std::string &newKey)
{
    channelKey = newKey;
    keyEnabled = true;
}

void Channel::removeKey()
{
    channelKey.clear();
    keyEnabled = false;
}
```

When enabling `+k`, a password must be provided.

When removing `-k`, the stored password must be deleted to keep a consistent state.

---

## User limit: `+l`

States:

```cpp
bool limitEnabled;
std::size_t userLimit;
```

Methods:

```cpp
bool Channel::isLimitEnabled() const
{
    return limitEnabled;
}

std::size_t Channel::getUserLimit() const
{
    return userLimit;
}

void Channel::setUserLimit(std::size_t newLimit)
{
    userLimit = newLimit;
    limitEnabled = true;
}

void Channel::removeUserLimit()
{
    userLimit = 0;
    limitEnabled = false;
}
```

When it is active, `JOIN` must prevent entry if:

```cpp
channel.getMemberCount() >= channel.getUserLimit()
```

The limit must be a valid number greater than zero.

---

## Ownership of channels

The server must own every channel.

Recommended relationship:

```text
Server
└── channels
    ├── "#general" → Channel
    ├── "#programming" → Channel
    └── "#irc" → Channel
```

A possible structure would be:

```cpp
std::map<std::string, Channel *> channels;
```

The server will be responsible for:

- Creating the channels.
- Looking up channels by name.
- Deleting empty channels.
- Freeing their memory when the server closes.

Lookup example:

```cpp
std::map<std::string, Channel *>::iterator channelIterator;

channelIterator = channels.find(channelName);

if (channelIterator != channels.end())
{
    Channel *channel = channelIterator->second;
}
```

Each client can store references or pointers to the channels it belongs to, but it must not own independent copies of those channels.

---

## Avoid independent copies

There must not be a different copy of the same channel inside each client.

Incorrect design:

```text
Client A → copy of #general
Client B → different copy of #general
Server   → another copy of #general
```

This would cause inconsistent states:

- A user would appear in one copy but not in another.
- The topic could be different.
- The operators might not match.
- The modes could have different values.

Correct design:

```text
Server
└── single Channel object "#general"
    ├── Client A
    ├── Client B
    └── Client C
```

Every client must refer to the same `Channel` object.

---

## Pointer stability

If `Channel` stores pointers to clients:

```cpp
std::set<Client *> members;
```

the `Client` objects must remain at stable memory addresses.

This design is compatible with a structure such as:

```cpp
std::map<int, Client *> clients;
```

The server creates the clients dynamically and their addresses do not change while they stay connected.

Before destroying a client, the server must remove its pointer from:

- The members of all of its channels.
- The operators of all of its channels.
- The invite lists.
- Any other global index.

A pointer to a destroyed client must never remain inside a channel.

---

## Deleting empty channels

When the last member leaves a channel through `PART`, `KICK` or `QUIT`, the server must delete the channel.

Flow:

```text
The client is removed from the channel
        ↓
It is checked whether the channel is empty
        ↓
If it is empty, the server deletes the channel
```

Example:

```cpp
channel->removeMember(&client);

if (channel->isEmpty())
{
    delete channel;
    channels.erase(channelIterator);
}
```

Deletion belongs to the server because it owns the `Channel` object.

---

## Responsibilities of `Channel`

The `Channel` class must take care of:

- Storing the name and the topic.
- Keeping the member list.
- Keeping the operator list.
- Keeping the invite list.
- Querying whether a client is a member.
- Querying whether a client is an operator.
- Adding and removing members.
- Adding and removing operators.
- Adding and consuming invitations.
- Storing the mode state.
- Keeping consistency among its collections.

The `Channel` class should not take care of:

- Reading data from the socket.
- Sending messages directly with `send()`.
- Registering clients.
- Looking up channels globally.
- Creating or destroying `Client` objects.
- Interpreting complete IRC commands.
- Building numeric replies.

Those responsibilities belong to the server, the reply system or the command handlers.

---

## Important invariants

The model must always keep these rules:

1. An operator must also be a member of the channel.
2. A client must not appear twice as a member.
3. A disconnected client must not remain in any collection.
4. If `keyEnabled` is `false`, `channelKey` must be empty.
5. If `limitEnabled` is `false`, `userLimit` must be `0`.
6. An empty channel must be deleted by the server.
7. The `Channel` class must not destroy the `Client` objects.
8. Every client must share the same object for the same channel.

---

## Minimum tests

Before continuing with the channel commands, it is useful to check:

- Creating a channel with every mode disabled.
- Adding a member.
- Trying to add the same member twice.
- Making the first member an operator.
- Checking whether a client is a member.
- Checking whether a client is an operator.
- Adding and removing an invitation.
- Enabling and disabling `+i`.
- Enabling and disabling `+t`.
- Setting and removing a password.
- Setting and removing a limit.
- Removing a member and also removing them from operators and invitees.
- Detecting when the channel becomes empty.
- Deleting the empty channel from the server.

---

## Expected result

At the end of this phase there must be a `Channel` class able to represent correctly:

```text
Name
Topic
Members
Operators
Invitees
Mode +i
Mode +t
Mode +k
Mode +l
```

The server must keep a single instance of each channel and be responsible for its creation and destruction.

The full logic of `JOIN`, `PART`, `KICK`, `INVITE`, `TOPIC` and `MODE` will be implemented on this model in the following phases.
