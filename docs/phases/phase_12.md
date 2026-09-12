# Phase 12 — Implementing `JOIN`

## Goal

Implement the `JOIN` command, which lets a registered client enter a channel.

This phase connects the client model with the channel model developed earlier. The server must:

- Create channels when they do not exist yet.
- Check access restrictions.
- Add clients to channels.
- Assign operators.
- Notify the entry to the members.
- Send the topic and the channel user list.

---

## Command syntax

The basic form is:

```text
JOIN #general
```

If the channel requires a password through mode `+k`:

```text
JOIN #general password
```

Entering several channels can also be accepted:

```text
JOIN #general,#programming generalKey,programmingKey
```

To start, entry into a single channel can be implemented first and the handler later expanded to accept comma-separated lists.

---

## Initial validations

Before trying to add the client, the server must check:

1. That the client is registered.
2. That the command contains a channel name.
3. That the channel name has a valid format.
4. That the client does not already belong to the channel.
5. That the channel’s access restrictions are met.

Example of initial validation:

```text
Is the client registered?
    ↓
Was a channel name received?
    ↓
Is the channel name valid?
    ↓
Does the client already belong to the channel?
```

If the client already belongs to the channel, the server can silently ignore the command to avoid adding them twice.

---

## Channel name validation

At a minimum, the name should:

- Start with `#`.
- Contain at least one character after the prefix.
- Not contain spaces.
- Not contain commas.
- Not contain control characters.
- Not exceed the length limit decided by the server.

Valid examples:

```text
#general
#programming
#cpp
```

Invalid examples:

```text
general
#
#channel with spaces
#channel,other
```

It is useful to centralize this check in a function:

```cpp
bool isValidChannelName(const std::string &channelName);
```

---

## Main `JOIN` flow

```text
Is the client registered?
    ↓
Is the channel name valid?
    ↓
Does the channel exist?
 ├── No
 │    ↓
 │   Create the channel
 │    ↓
 │   Add the client
 │    ↓
 │   Make them an operator
 │
 └── Yes
      ↓
     Check restrictions
      ├── mode +i
      ├── mode +k
      └── mode +l
           ↓
         Add the client
           ↓
         Notify JOIN
           ↓
         Send topic
           ↓
         Send member list
```

---

## Creating a channel

If the requested channel does not exist, the server must:

1. Create a new `Channel` object.
2. Store it in the global channel collection.
3. Add the client as a member.
4. Make the client a channel operator.

Conceptual example:

```cpp
std::map<std::string, Channel> channels;
```

The server must own the channels. Clients should only store a reference, identifier or name of the channels they belong to.

The first user of a new channel must automatically become an operator:

```text
JOIN #general
```

Result:

```text
Members: roxana
Operators: roxana
```

In the `NAMES` reply, an operator is normally represented with `@`:

```text
@roxana
```

---

## Checking restrictions

If the channel already exists, its modes must be checked before adding the client.

The recommended order is:

1. Check whether the client is already inside.
2. Check the user limit `+l`.
3. Check the invite mode `+i`.
4. Check the password `+k`.

No channel state must be modified until every validation has finished correctly.

---

## Mode `+i` — Invite-only channel

When the channel has mode `+i` enabled, only clients included in its invite list can enter.

```text
MODE #general +i
```

If the client is not invited:

```text
473 ERR_INVITEONLYCHAN
```

Approximate format:

```text
:server.name 473 roxana #general :Cannot join channel (+i)
```

If entry succeeds, the client should be removed from the invite list, because the invitation has already been consumed.

---

## Mode `+k` — Password-protected channel

When the channel has mode `+k` enabled, the client must provide the correct password:

```text
JOIN #general password
```

The server must check:

- That a password was provided.
- That it matches the channel password exactly.

If it is missing or incorrect:

```text
475 ERR_BADCHANNELKEY
```

Approximate format:

```text
:server.name 475 roxana #general :Cannot join channel (+k)
```

---

## Mode `+l` — User limit

When the channel has mode `+l` enabled, the server must check that there is still available space.

Example:

```text
MODE #general +l 10
```

The check must be performed before adding the client:

```text
number of members >= limit
```

If the channel is full:

```text
471 ERR_CHANNELISFULL
```

Approximate format:

```text
:server.name 471 roxana #general :Cannot join channel (+l)
```

---

## Adding the client

When every validation has succeeded:

1. Add the client to the channel’s member collection.
2. Register the channel among the client’s channels.
3. Remove the client from the invite list, if they were included.
4. Assign them operator status if they are the first member.

It is important to keep both sides of the relationship synchronized:

```text
Channel → contains the Client
Client  → knows the Channel
```

It must not happen that the channel contains the client but the client does not have the channel registered, or vice versa.

It is useful to centralize this operation:

```cpp
void Server::addClientToChannel(
    Client &client,
    Channel &channel
);
```

---

## Entry notification

After adding the client, the `JOIN` message must be sent to every channel member, including the client itself.

Format:

```text
:nickname!username@hostname JOIN :#general
```

Example:

```text
:roxana!roxana@127.0.0.1 JOIN :#general
```

The notification must use the client’s full prefix:

```cpp
std::string buildClientPrefix(const Client &client);
```

And it should be sent through a broadcast function:

```cpp
void Server::broadcastToChannel(
    const Channel &channel,
    const std::string &message
);
```

The client must be added to the channel before performing the broadcast so they also receive confirmation of their own `JOIN`.

---

## Sending the topic

After confirming entry, the server must report the current topic.

### Channel without a topic

If the channel has no topic:

```text
331 RPL_NOTOPIC
```

Example:

```text
:server.name 331 roxana #general :No topic is set
```

### Channel with a topic

If the channel has a topic:

```text
332 RPL_TOPIC
```

Example:

```text
:server.name 332 roxana #general :Server general channel
```

---

## Sending the member list

After the topic, the server must send the member list through two replies.

### `353 RPL_NAMREPLY`

Contains the users present on the channel:

```text
:server.name 353 roxana = #general :@roxana user2 user3
```

Operators must appear with the `@` prefix:

```text
@roxana
```

Regular users appear without a prefix:

```text
user2
```

### `366 RPL_ENDOFNAMES`

Indicates that the list has ended:

```text
:server.name 366 roxana #general :End of /NAMES list
```

The complete order must be:

```text
JOIN
331 or 332
353
366
```

Complete example:

```text
:roxana!roxana@127.0.0.1 JOIN :#general
:server.name 331 roxana #general :No topic is set
:server.name 353 roxana = #general :@roxana
:server.name 366 roxana #general :End of /NAMES list
```

---

## Required numeric replies

| Code | Name | Situation |
|---:|---|---|
| `331` | `RPL_NOTOPIC` | The channel has no topic |
| `332` | `RPL_TOPIC` | The channel has a topic |
| `353` | `RPL_NAMREPLY` | Channel member list |
| `366` | `RPL_ENDOFNAMES` | End of the member list |
| `403` | `ERR_NOSUCHCHANNEL` | The channel name cannot be used |
| `451` | `ERR_NOTREGISTERED` | The client is not registered yet |
| `461` | `ERR_NEEDMOREPARAMS` | No channel was provided |
| `471` | `ERR_CHANNELISFULL` | The channel has reached its limit |
| `473` | `ERR_INVITEONLYCHAN` | The channel requires an invitation |
| `475` | `ERR_BADCHANNELKEY` | The password is incorrect |

---

## Recommended handler structure

The handler can be split into small operations:

```cpp
void CommandDispatcher::handleJoin(
    Client &client,
    const IrcMessage &message
);

bool Server::canJoinChannel(
    const Client &client,
    const Channel &channel,
    const std::string &providedKey
);

void Server::addClientToChannel(
    Client &client,
    Channel &channel
);

void Server::sendChannelTopic(
    Client &client,
    const Channel &channel
);

void Server::sendChannelNames(
    Client &client,
    const Channel &channel
);
```

Handler responsibilities:

```text
handleJoin()
    ├── validate registration and parameters
    ├── validate name
    ├── locate or create channel
    ├── check restrictions
    ├── add client
    ├── emit JOIN
    ├── send topic
    └── send NAMES
```

---

## Recommended implementation of `PART`

Even though `PART` is not one of the subject’s central commands, implementing it in this phase makes tests considerably easier.

Syntax:

```text
PART #general
```

With an optional message:

```text
PART #general :See you later
```

The server must:

1. Check that the client is registered.
2. Check that the channel exists.
3. Check that the client belongs to the channel.
4. Notify the leave to the members.
5. Remove the client from the member list.
6. Remove them from the operator list.
7. Remove the channel from the client’s collection.
8. Delete the channel from the server if it becomes empty.

Leave message:

```text
:roxana!roxana@127.0.0.1 PART #general :See you later
```

The notification must be sent before removing the client, so they can also receive their own `PART` message.

Useful errors for `PART`:

| Code | Name | Situation |
|---:|---|---|
| `403` | `ERR_NOSUCHCHANNEL` | The channel does not exist |
| `442` | `ERR_NOTONCHANNEL` | The client does not belong to the channel |
| `461` | `ERR_NEEDMOREPARAMS` | No channel was provided |

---

## Minimum test cases

### Create a new channel

```text
JOIN #general
```

Must:

- Create `#general`.
- Add the client.
- Make them an operator.
- Emit the `JOIN` message.
- Send `331` or `332`.
- Send `353`.
- Send `366`.

### Enter an existing channel

```text
JOIN #general
```

Must:

- Keep the previous members.
- Add the new client.
- Notify the `JOIN` to every member.
- Send the new client the topic and the name list.

### Enter the same channel twice

```text
JOIN #general
JOIN #general
```

The client must not appear duplicated.

### Full channel

```text
MODE #general +l 1
JOIN #general
```

The second client must receive:

```text
471 ERR_CHANNELISFULL
```

### Invite-only channel

```text
MODE #general +i
JOIN #general
```

A client who is not invited must receive:

```text
473 ERR_INVITEONLYCHAN
```

### Incorrect password

```text
MODE #general +k secret
JOIN #general incorrect
```

Must reply:

```text
475 ERR_BADCHANNELKEY
```

### Correct password

```text
JOIN #general secret
```

The client must enter normally.

### Leave the channel

```text
PART #general :See you later
```

Must:

- Notify the `PART`.
- Remove the client from the channel.
- Remove their operator status.
- Delete the channel if it becomes empty.

---

## Expected result of the phase

At the end of this phase, the server must be able to:

- Create channels dynamically.
- Add clients to existing channels.
- Make the first member an operator.
- Apply modes `+i`, `+k` and `+l` correctly.
- Keep the relationship between clients and channels synchronized.
- Notify entries to every member.
- Send the channel topic.
- Send the member list with their prefixes.
- Avoid duplicate members.
- Handle leaving through `PART`.
- Delete channels that become empty.
