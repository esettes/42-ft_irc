# Phase 13 — `PRIVMSG` command

## Goal

Implement sending messages:

- Between two users.
- From a user to a channel.
- Without blocking the server.
- Keeping the sender’s full prefix.

The command has these two main forms:

```text
PRIVMSG roxana :Hello
PRIVMSG #general :Hello everyone
```

---

## 1. Prerequisites

Before processing `PRIVMSG`, the server must check that the client is registered.

If they have not yet completed `PASS`, `NICK` and `USER`, it must reply:

```text
451 ERR_NOTREGISTERED
```

The command also depends on already having:

- Global client lookup by nickname.
- The channel model.
- Each channel’s member list.
- The centralized IRC reply system.
- Each client’s non-blocking output buffer.

---

## 2. Command parameters

A private message needs two pieces of data:

1. The target.
2. The message text.

For example:

```text
PRIVMSG roxana :Hello
```

The parser should produce something equivalent to:

```text
commandName = "PRIVMSG"
parameters[0] = "roxana"
parameters[1] = "Hello"
```

In a message directed at a channel:

```text
PRIVMSG #general :Hello everyone
```

The result should be:

```text
commandName = "PRIVMSG"
parameters[0] = "#general"
parameters[1] = "Hello everyone"
```

The text introduced after `:` must be kept as a single parameter, even if it contains spaces.

---

## 3. Recommended validation order

The `PRIVMSG` handler should perform the checks in this order:

```text
Is the client registered?
    ↓
Does a target exist?
    ↓
Does text exist?
    ↓
Is the target a channel or a user?
    ├── user    → look up nickname
    └── channel → look up channel and check membership
    ↓
build the message with the sender’s prefix
    ↓
append the message to the receivers’ output buffers
```

This order lets you return a single consistent error and stop processing as soon as a problem is detected.

---

## 4. Private messages between users

Received example:

```text
PRIVMSG roxana :Hello
```

The server must:

1. Check that the target parameter exists.
2. Check that the text exists.
3. Globally look up the client whose nickname is `roxana`.
4. Build the message with the sender’s full prefix.
5. Append the message to the target’s output buffer.

Message sent to the receiver:

```text
:alice!alice@localhost PRIVMSG roxana :Hello
```

The prefix must correspond to the user who sent the message:

```text
:nickname!username@hostname
```

The server must not replace that prefix with its own name.

The message is normally only sent to the target. It is not necessary to forward it back to the sender.

---

## 5. Messages directed at channels

Received example:

```text
PRIVMSG #general :Hello everyone
```

The server must:

1. Check that the channel exists.
2. Check that the sender belongs to the channel.
3. Build the message with the sender’s full prefix.
4. Traverse the channel members.
5. Send the message to every member except the sender.

Message received by the other members:

```text
:alice!alice@localhost PRIVMSG #general :Hello everyone
```

The message must not be forwarded back to the sender.

The distribution would be equivalent to:

```text
Channel #general
├── alice    ← sender, does not receive a copy
├── roxana   ← receives the message
├── bob      ← receives the message
└── carol    ← receives the message
```

Every receiver must receive exactly the same message.

---

## 6. Identifying the target type

A simple way to distinguish a channel from a nickname is to check the first character:

```cpp
bool isChannelTarget(const std::string &target)
{
    return !target.empty() && target[0] == '#';
}
```

The handler flow can be split into two functions:

```cpp
void handlePrivateMessage(Client &sender, const Command &command);
void sendMessageToUser(
    Client &sender,
    const std::string &nickname,
    const std::string &message
);
void sendMessageToChannel(
    Client &sender,
    const std::string &channelName,
    const std::string &message
);
```

This avoids concentrating all of the logic in a single function.

---

## 7. Building the message

It is useful to reuse the function in charge of building the client prefix.

Conceptual example:

```cpp
std::string message =
    buildClientPrefix(sender)
    + " PRIVMSG "
    + target
    + " :"
    + messageText
    + "\r\n";
```

The result must respect the IRC format:

```text
:alice!alice@localhost PRIVMSG #general :Hello everyone\r\n
```

The `\r\n` terminator must not be forgotten.

---

## 8. Non-blocking send

`PRIVMSG` must not call `send()` directly from the handler.

The message must be appended to the receiver’s output buffer:

```text
built message
    ↓
it is appended to the receiver’s outputBuffer
    ↓
POLLOUT is enabled
    ↓
poll() reports that the socket allows writing
    ↓
the available bytes are sent
```

For a channel, the same message is appended to each receiving member’s output buffer.

This keeps the non-blocking behaviour implemented in phase 7.

---

## 9. Relevant errors

### `401 ERR_NOSUCHNICK`

Returned when the target nickname does not exist.

Example:

```text
PRIVMSG nonexistentUser :Hello
```

Reply:

```text
:server.name 401 alice nonexistentUser :No such nick
```

---

### `403 ERR_NOSUCHCHANNEL`

Returned when the target channel does not exist.

Example:

```text
PRIVMSG #nonexistent :Hello
```

Reply:

```text
:server.name 403 alice #nonexistent :No such channel
```

---

### `404 ERR_CANNOTSENDTOCHAN`

Returned when the user cannot send messages to the channel.

In this phase, it must be used when the sender does not belong to the channel.

Example:

```text
PRIVMSG #general :Hello
```

Reply:

```text
:server.name 404 alice #general :Cannot send to channel
```

---

### `411 ERR_NORECIPIENT`

Returned when no target has been indicated.

Example:

```text
PRIVMSG
```

Reply:

```text
:server.name 411 alice :No recipient given (PRIVMSG)
```

---

### `412 ERR_NOTEXTTOSEND`

Returned when a target exists, but there is no text to send.

Examples:

```text
PRIVMSG roxana
PRIVMSG roxana :
```

Reply:

```text
:server.name 412 alice :No text to send
```

The trailing of `PRIVMSG roxana :` should also be considered empty.

---

## 10. Error summary

| Situation | Code | Name |
|---|---:|---|
| Unregistered client | `451` | `ERR_NOTREGISTERED` |
| Missing target | `411` | `ERR_NORECIPIENT` |
| Missing text | `412` | `ERR_NOTEXTTOSEND` |
| Nonexistent nickname | `401` | `ERR_NOSUCHNICK` |
| Nonexistent channel | `403` | `ERR_NOSUCHCHANNEL` |
| The user cannot write to the channel | `404` | `ERR_CANNOTSENDTOCHAN` |

---

## 11. Recommended handler structure

```cpp
void CommandDispatcher::handlePrivmsg(
    Client &sender,
    const Command &command
)
{
    if (!sender.isRegistered())
    {
        _server.queueNumericReply(sender, 451);
        return;
    }

    if (command.getParameters().empty())
    {
        _server.queueNoRecipientError(sender, "PRIVMSG");
        return;
    }

    if (command.getParameters().size() < 2
        || command.getParameters()[1].empty())
    {
        _server.queueNoTextToSendError(sender);
        return;
    }

    const std::string &target = command.getParameters()[0];
    const std::string &messageText = command.getParameters()[1];

    if (isChannelTarget(target))
    {
        sendMessageToChannel(sender, target, messageText);
        return;
    }

    sendMessageToUser(sender, target, messageText);
}
```

The concrete names can be adapted to the project architecture.

---

## 12. Minimum test cases

### Correct message between users

```text
PRIVMSG roxana :Hello
```

Expected result:

```text
:alice!alice@localhost PRIVMSG roxana :Hello
```

Only `roxana` receives the message.

---

### Correct message to a channel

```text
PRIVMSG #general :Hello everyone
```

Every member of `#general`, except the sender, receives:

```text
:alice!alice@localhost PRIVMSG #general :Hello everyone
```

---

### Nonexistent nickname

```text
PRIVMSG nobody :Hello
```

Expected result:

```text
401 ERR_NOSUCHNICK
```

---

### Nonexistent channel

```text
PRIVMSG #nonexistent :Hello
```

Expected result:

```text
403 ERR_NOSUCHCHANNEL
```

---

### Sender outside the channel

```text
PRIVMSG #general :Hello
```

If the sender does not belong to `#general`:

```text
404 ERR_CANNOTSENDTOCHAN
```

---

### Missing target

```text
PRIVMSG
```

Expected result:

```text
411 ERR_NORECIPIENT
```

---

### Missing text

```text
PRIVMSG roxana
```

Expected result:

```text
412 ERR_NOTEXTTOSEND
```

---

### Empty text

```text
PRIVMSG roxana :
```

Expected result:

```text
412 ERR_NOTEXTTOSEND
```

---

## 13. Criteria to complete the phase

The phase will be finished when:

- `PRIVMSG` rejects unregistered clients.
- Private messages can be sent between users.
- Messages can be sent to channels.
- Channel messages reach every member except the sender.
- An external user cannot write to a channel they do not belong to.
- Messages keep the sender’s full prefix.
- Nonexistent nicknames and channels produce the corresponding error.
- The absence of a target or text produces the corresponding error.
- Every message ends with `\r\n`.
- Sending uses the non-blocking output buffers.
- The server keeps working correctly after receiving invalid `PRIVMSG` commands.

> The main goal of this phase is to complete the basic communication system required by the project: private messages between users and distribution of messages among the members of a channel.
