# Phase 14 — `TOPIC` command

## Goal

Implement the `TOPIC` command, which manages a channel’s topic.

It must allow:

- Querying the current topic.
- Setting a new topic.
- Changing the existing topic.
- Removing the topic.
- Respecting the `+t` mode restriction.
- Notifying changes to every channel member.

---

## Syntax

### Query the topic

```irc
TOPIC #general
```

### Set or change the topic

```irc
TOPIC #general :New channel topic
```

The text after `:` constitutes a single parameter and may contain spaces.

### Remove the topic

```irc
TOPIC #general :
```

An empty final parameter indicates that the channel must be left without a topic.

---

## Required state in `Channel`

Each channel must store:

```cpp
class Channel
{
private:
    std::string topic;
    bool topicRestricted;
};
```

The `topicRestricted` attribute represents mode `+t`:

- `false`: any channel member can change the topic.
- `true`: only channel operators can change it.

An empty string can represent that the channel has no topic set.

---

## Querying the topic

When a client sends:

```irc
TOPIC #general
```

The server must check:

1. That the client is registered.
2. That the channel name has been indicated.
3. That the channel exists.
4. That the client belongs to the channel.

If the channel has a topic set, it must reply with:

```irc
:server.name 332 roxana #general :Current channel topic
```

Code `332` corresponds to:

```text
RPL_TOPIC
```

If the channel has no topic set, it must reply with:

```irc
:server.name 331 roxana #general :No topic is set
```

Code `331` corresponds to:

```text
RPL_NOTOPIC
```

Querying the topic does not require the user to be a channel operator.

---

## Changing the topic

When a client sends:

```irc
TOPIC #general :New topic
```

The server must check:

1. That the client is registered.
2. That the channel name has been indicated.
3. That the channel exists.
4. That the client belongs to the channel.
5. If mode `+t` is enabled, that the client is an operator.

If every check is correct:

1. The topic stored in `Channel` is updated.
2. The message is built with the user’s full prefix.
3. The change is sent to every channel member, including the sender.

Example:

```irc
:roxana!roxana@localhost TOPIC #general :New topic
```

---

## Mode `+t` restriction

Mode `+t` controls who can change the topic:

```text
+t enabled
    ↓
only operators can change the topic
```

```text
+t disabled
    ↓
any channel member can change the topic
```

Mode `+t` only affects changing the topic. A regular member can still query it.

---

## Removing the topic

When this is received:

```irc
TOPIC #general :
```

The server must:

1. Perform the same checks as for changing the topic.
2. Store an empty string as the topic.
3. Notify the change to every member.

Notification example:

```irc
:roxana!roxana@localhost TOPIC #general :
```

After removing it, a new query must produce:

```irc
:server.name 331 roxana #general :No topic is set
```

---

## Relevant errors

### `461 ERR_NEEDMOREPARAMS`

Used when the channel name is not provided:

```irc
TOPIC
```

Reply:

```irc
:server.name 461 roxana TOPIC :Not enough parameters
```

### `403 ERR_NOSUCHCHANNEL`

Used when the channel does not exist:

```irc
TOPIC #nonexistent
```

Reply:

```irc
:server.name 403 roxana #nonexistent :No such channel
```

### `442 ERR_NOTONCHANNEL`

Used when the user does not belong to the channel:

```irc
:server.name 442 roxana #general :You're not on that channel
```

### `482 ERR_CHANOPRIVSNEEDED`

Used when:

- Mode `+t` is enabled.
- The user tries to change the topic.
- The user is not an operator.

Reply:

```irc
:server.name 482 roxana #general :You're not channel operator
```

### `451 ERR_NOTREGISTERED`

Used when an unregistered client tries to run the command:

```irc
:server.name 451 * :You have not registered
```

---

## General flow

```text
TOPIC received
    ↓
Is the client registered?
    ├── no → 451 ERR_NOTREGISTERED
    └── yes
         ↓
Was a channel indicated?
    ├── no → 461 ERR_NEEDMOREPARAMS
    └── yes
         ↓
Does the channel exist?
    ├── no → 403 ERR_NOSUCHCHANNEL
    └── yes
         ↓
Does the user belong to the channel?
    ├── no → 442 ERR_NOTONCHANNEL
    └── yes
         ↓
Was a new topic provided?
    ├── no → reply with 331 or 332
    └── yes
         ↓
Is mode +t enabled?
    ├── no → update the topic
    └── yes
         ↓
Is the user an operator?
    ├── no → 482 ERR_CHANOPRIVSNEEDED
    └── yes → update the topic
         ↓
Notify every member
```

---

## Expected result

At the end of this phase, the server must be able to:

- Query a channel’s topic.
- Report when a channel has no topic.
- Set and change the topic.
- Remove the topic through an empty final parameter.
- Apply the `+t` restriction correctly.
- Reject changes made by users without permissions.
- Notify changes to every channel member.
- Return consistent numeric replies for any error.
