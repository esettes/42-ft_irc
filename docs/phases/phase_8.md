# Phase 8 — Centralized IRC reply system

## Goal

Implement a common system to build and send IRC replies consistently.

The different command handlers must not manually build complete messages or call `send()` directly. Their responsibility will be to decide which reply is appropriate and append it to the client’s output buffer.

The recommended flow is:

```text
command handler
        ↓
construction of the IRC reply
        ↓
append "\r\n"
        ↓
store in outputBuffer
        ↓
enable POLLOUT
        ↓
send from the poll() loop
```

This phase builds on the output buffer implemented in phase 7.

---

## 1. General format of an IRC message

An IRC message can have the following structure:

```text
[:prefix] COMMAND [parameters] [:trailing]\r\n
```

Example of a numeric reply sent by the server:

```text
:server.name 001 roxana :Welcome to the IRC Network\r\n
```

Its parts are:

```text
:server.name
```

Prefix of the server that originates the message.

```text
001
```

Numeric code of the reply.

```text
roxana
```

Nickname of the client the reply is directed to.

```text
:Welcome to the IRC Network
```

Last parameter or `trailing`. It may contain spaces because it starts with `:`.

---

## 2. Messages originated by clients

When a command performed by a client must be sent to other users, the message must include the client’s full prefix.

Usual format:

```text
:nickname!username@hostname COMMAND parameters :trailing\r\n
```

Example:

```text
:roxana!username@hostname PRIVMSG #general :Hello\r\n
```

The prefix lets you identify who originated the message.

Its components are:

- `nickname`: the client’s current nickname.
- `username`: username received through `USER`.
- `hostname`: address or name associated with the connection.

---

## 3. Centralize message construction

Prefix and reply construction must not be repeated in every handler.

Instead of doing this:

```cpp
client.queueOutput(
    ":" + serverName + " 461 " + client.getNickname()
    + " PRIVMSG :Not enough parameters\r\n"
);
```

in each command, reusable functions such as these should be available:

```cpp
std::string buildNumericReply(
    int numericCode,
    const Client &client,
    const std::string &parameters,
    const std::string &message
) const;
```

```cpp
std::string buildClientPrefix(const Client &client) const;
```

```cpp
void sendReply(
    Client &client,
    int numericCode,
    const std::string &parameters,
    const std::string &message
);
```

```cpp
void queueMessage(
    Client &client,
    const std::string &message
);
```

The exact names and responsibilities can be adapted to the project architecture.

---

## 4. Building numeric replies

The `buildNumericReply()` function must create replies with a uniform format.

Conceptual example:

```cpp
std::string Server::buildNumericReply(
    int numericCode,
    const Client &client,
    const std::string &parameters,
    const std::string &message
) const;
```

It must take care of:

1. Adding the server prefix.
2. Converting the numeric code to three digits.
3. Adding the target.
4. Adding the reply-specific parameters.
5. Adding the final message preceded by `:`.
6. Ending the reply with `\r\n`.

Resulting format:

```text
:<server-name> <numeric-code> <target> [parameters] :<message>\r\n
```

For example:

```text
:irc.example.net 433 roxana roxy :Nickname is already in use\r\n
```

The numeric code must always keep three digits:

```text
001
421
433
464
```

It must not be sent as:

```text
1
21
33
64
```

---

## 5. Target of a numeric reply

Normally a numeric reply uses the client’s nickname as the target:

```text
:server.name 421 roxana TEST :Unknown command\r\n
```

However, the client may not have a valid nickname yet. In that case `*` can be used as the target:

```text
:server.name 431 * :No nickname given\r\n
```

It is useful to centralize this decision:

```text
if the client has a nickname
    use its nickname
if it does not have a nickname yet
    use "*"
```

This avoids each handler having to check it separately.

---

## 6. Building the client prefix

The `buildClientPrefix()` function must generate a user’s prefix:

```text
:nickname!username@hostname
```

Example:

```text
:roxana!username@127.0.0.1
```

This prefix will be used in messages such as:

- `PRIVMSG`
- `JOIN`
- `PART`
- `QUIT`
- `NICK`
- `KICK`
- `INVITE`
- `TOPIC`

Example:

```text
:roxana!username@127.0.0.1 JOIN #general\r\n
```

Centralizing this prefix is important because any future change in its format will only have to be made in one place.

---

## 7. Append messages to the output buffer

The `queueMessage()` function must append the message to the client’s `outputBuffer`.

Conceptual example:

```cpp
void Server::queueMessage(
    Client &client,
    const std::string &message
);
```

Its responsibilities should be:

1. Receive the already built IRC message.
2. Check or guarantee that it ends with `\r\n`.
3. Append it to the client’s output buffer.
4. Enable `POLLOUT` for its socket.

It must not assume that the message will be sent immediately.

```text
queueMessage()
      ↓
outputBuffer
      ↓
POLLOUT
      ↓
send()
```

The actual write must still be performed from the event loop when `poll()` reports that the socket allows writing.

---

## 8. Difference between `sendReply()` and `queueMessage()`

A possible separation of responsibilities is:

### `buildNumericReply()`

Builds the text of a numeric reply, but does not modify the client.

```text
reply data
        ↓
std::string with IRC format
```

### `buildClientPrefix()`

Builds the prefix corresponding to a client.

```text
Client
  ↓
:nickname!username@hostname
```

### `sendReply()`

Requests the creation and queueing of a numeric reply.

```text
code + parameters + message
              ↓
      buildNumericReply()
              ↓
        queueMessage()
```

Even if it is called `sendReply()`, it should not call `send()` directly if the server uses non-blocking writes.

A more explicit name could also be:

```cpp
queueNumericReply();
```

### `queueMessage()`

Appends any IRC message to the client’s output buffer.

It can be used both for numeric replies and for normal messages:

```text
001
433
PRIVMSG
JOIN
QUIT
PING
```

---

## 9. Correct message termination

Every IRC message sent by the server must end with:

```text
\r\n
```

Only this must not be used:

```text
\n
```

Correct example:

```cpp
":server.name 001 roxana :Welcome to the IRC Network\r\n"
```

A single responsibility should be decided:

- The construction functions append `\r\n`.
- Or `queueMessage()` appends `\r\n`.

The two strategies must not be mixed, because messages with duplicated terminators could be produced:

```text
\r\n\r\n
```

A simple option is that every `build...()` function returns complete messages, including `\r\n`.

---

# Basic error replies

## 10. `431 ERR_NONICKNAMEGIVEN`

Used when the `NICK` command does not include a nickname.

Received command:

```text
NICK
```

Reply:

```text
:server.name 431 * :No nickname given\r\n
```

Condition:

```text
the command is NICK
    +
it does not contain the nickname parameter
```

---

## 11. `432 ERR_ERRONEUSNICKNAME`

Used when the requested nickname has an invalid format.

Example:

```text
NICK invalid@name
```

Reply:

```text
:server.name 432 * invalid@name :Erroneous nickname\r\n
```

At a minimum, it must be validated that:

- It is not empty.
- It does not start with a number.
- It does not contain spaces.
- It does not contain characters incompatible with the chosen nickname rules.
- It does not exceed the limit defined by the server.

Nickname validation should be centralized in a function such as:

```cpp
bool isValidNickname(const std::string &nickname) const;
```

---

## 12. `433 ERR_NICKNAMEINUSE`

Used when another client is already using the requested nickname.

Received command:

```text
NICK roxana
```

Reply:

```text
:server.name 433 * roxana :Nickname is already in use\r\n
```

If the client already had a previous nickname, it can be used as the target:

```text
:server.name 433 previousNick roxana :Nickname is already in use\r\n
```

The lookup must be consistent with the nickname comparison used by the server.

---

## 13. `451 ERR_NOTREGISTERED`

Used when a client tries to run a command that requires registration before completing the connection process.

Example:

```text
PRIVMSG #general :Hello
```

Reply:

```text
:server.name 451 * :You have not registered\r\n
```

The server must check whether the client has completed:

```text
valid PASS
    +
valid NICK
    +
USER received
```

This check should be done before executing commands that require a registered client.

---

## 14. `461 ERR_NEEDMOREPARAMS`

Used when a command does not contain every mandatory parameter.

Example:

```text
PRIVMSG
```

Reply:

```text
:server.name 461 roxana PRIVMSG :Not enough parameters\r\n
```

Other examples that may produce this error:

```text
PASS
USER roxana
JOIN
KICK #general
```

Each handler must check its minimum number of parameters before accessing the parameter vector.

Conceptual example:

```cpp
if (command.getParameters().size() < requiredParameterCount)
{
    queueNumericReply(
        client,
        461,
        command.getName(),
        "Not enough parameters"
    );
    return;
}
```

This also avoids out-of-range accesses.

---

## 15. `462 ERR_ALREADYREGISTERED`

Used when a registered client tries to repeat an operation that only belongs to the registration process.

Example:

```text
USER anotherUser 0 * :Another Name
```

Reply:

```text
:server.name 462 roxana :You may not reregister\r\n
```

It must be used especially when an already registered client tries to send again:

```text
USER
```

It can also be applied to other registration commands according to the server’s decisions.

A client must not necessarily be prevented from changing its nickname through `NICK`, because IRC allows changing it after registration.

---

## 16. `464 ERR_PASSWDMISMATCH`

Used when the password received through `PASS` does not match the server password.

Received command:

```text
PASS wrong-password
```

Reply:

```text
:server.name 464 * :Password incorrect\r\n
```

After this reply, the server must keep the client as unauthenticated.

It must be clearly defined whether:

- Another `PASS` attempt is allowed.
- The client is disconnected immediately.
- The client is disconnected after several attempts.

For a first implementation, you can reply with `464` and keep the connection open to allow another attempt.

---

## 17. `421 ERR_UNKNOWNCOMMAND`

Used when the server receives a command it does not recognize or does not support.

Received command:

```text
TEST something
```

Reply:

```text
:server.name 421 roxana TEST :Unknown command\r\n
```

It must be generated from the system in charge of dispatching commands when the command name is not registered.

Recommended flow:

```text
command received
       ↓
look up handler
       ↓
handler found → execute it
handler not found → send 421
```

The `TEST` parameter lets the client know which command was rejected.

---

# Minimum error table

| Code | Name | When it is used |
|---:|---|---|
| `421` | `ERR_UNKNOWNCOMMAND` | The command does not exist or is not supported |
| `431` | `ERR_NONICKNAMEGIVEN` | `NICK` does not contain a nickname |
| `432` | `ERR_ERRONEUSNICKNAME` | The nickname has an invalid format |
| `433` | `ERR_NICKNAMEINUSE` | The nickname is already being used |
| `451` | `ERR_NOTREGISTERED` | The command requires the client to be registered |
| `461` | `ERR_NEEDMOREPARAMS` | Mandatory parameters are missing |
| `462` | `ERR_ALREADYREGISTERED` | A registered client tries to register again |
| `464` | `ERR_PASSWDMISMATCH` | The received password is incorrect |

---

# Welcome reply

When the client completes registration correctly, the server must send at least the welcome reply:

```text
:server.name 001 roxana :Welcome to the IRC Network roxana\r\n
```

Registration must be completed only once when all conditions are met:

```text
passwordAccepted == true
nicknameReceived == true
usernameReceived == true
registered == false
```

After sending the welcome:

```text
registered = true
```

This avoids sending `001` several times if the client runs one of the related commands again.

---

# Recommended organization

A possible separation is:

```text
Server
├── buildNumericReply()
├── buildClientPrefix()
├── queueNumericReply()
├── queueMessage()
├── updateClientPollEvents()
└── tryRegisterClient()
```

Responsibilities:

| Function | Responsibility |
|---|---|
| `buildNumericReply()` | Build an IRC numeric reply |
| `buildClientPrefix()` | Build `nickname!username@hostname` |
| `queueNumericReply()` | Build and queue a numeric reply |
| `queueMessage()` | Append a message to the output buffer |
| `updateClientPollEvents()` | Enable or disable `POLLOUT` |
| `tryRegisterClient()` | Check registration state and send `001` |

Numeric codes can also be stored in a separate file:

```text
NumericReplies.hpp
```

Example:

```cpp
#ifndef NUMERIC_REPLIES_HPP
#define NUMERIC_REPLIES_HPP

namespace NumericReply
{
    const int RPL_WELCOME = 1;
    const int ERR_UNKNOWNCOMMAND = 421;
    const int ERR_NONICKNAMEGIVEN = 431;
    const int ERR_ERRONEUSNICKNAME = 432;
    const int ERR_NICKNAMEINUSE = 433;
    const int ERR_NOTREGISTERED = 451;
    const int ERR_NEEDMOREPARAMS = 461;
    const int ERR_ALREADYREGISTERED = 462;
    const int ERR_PASSWDMISMATCH = 464;
}

#endif
```

When converting the codes to text, they must be padded with leading zeros:

```text
1 → 001
```

---

# Important architectural rules

## Handlers must not call `send()` directly

Handlers must limit themselves to preparing or requesting the reply:

```text
handleNick()
    ↓
queueNumericReply()
    ↓
outputBuffer
```

The actual write belongs to the non-blocking output system:

```text
poll() detects POLLOUT
        ↓
send()
        ↓
remove only the sent bytes
```

---

## Do not duplicate formats

This must not be built repeatedly:

```text
":" + serverName + " " + code + " " + nickname
```

Nor this:

```text
":" + nickname + "!" + username + "@" + hostname
```

All of that logic must be centralized.

---

## Separate construction and transport

Building a message and sending it are different responsibilities:

```text
construction
    ↓
complete IRC message
    ↓
queueing
    ↓
non-blocking send
```

This separation makes it easier to:

- Reuse formats.
- Test the replies.
- Avoid inconsistent messages.
- Handle partial sends.
- Add new numeric codes.
- Send the same message to several clients.

---

## Do not automatically close on every error

Errors such as the following must normally produce a reply, not cause the server to terminate:

- Unknown command.
- Insufficient parameters.
- Invalid nickname.
- Nickname already in use.
- Client still not registered.
- Incorrect password.

The server must keep running and keep the rest of the clients connected.

Disconnecting a client must be decided explicitly according to the type of error.

---

# Recommended tests

## Nickname not provided

Input:

```text
NICK
```

Expected output:

```text
:server.name 431 * :No nickname given
```

---

## Invalid nickname

Input:

```text
NICK 123roxana
```

Expected output:

```text
:server.name 432 * 123roxana :Erroneous nickname
```

---

## Nickname already in use

First client:

```text
NICK roxana
```

Second client:

```text
NICK roxana
```

Expected output for the second client:

```text
:server.name 433 * roxana :Nickname is already in use
```

---

## Command without enough parameters

Input:

```text
PRIVMSG
```

Expected output:

```text
:server.name 461 roxana PRIVMSG :Not enough parameters
```

---

## Unregistered client

Input before completing `PASS`, `NICK` and `USER`:

```text
PRIVMSG #general :Hello
```

Expected output:

```text
:server.name 451 * :You have not registered
```

---

## Incorrect password

Input:

```text
PASS incorrect
```

Expected output:

```text
:server.name 464 * :Password incorrect
```

---

## Unknown command

Input:

```text
TEST something
```

Expected output:

```text
:server.name 421 roxana TEST :Unknown command
```

---

## Correct registration

Input:

```text
PASS secret
NICK roxana
USER roxana 0 * :Roxana Example
```

Expected output:

```text
:server.name 001 roxana :Welcome to the IRC Network roxana
```

The `001` reply must be sent only once.

---

# Expected result of the phase

At the end of this phase, the server must:

- Build every IRC reply from centralized functions.
- Generate server and client prefixes correctly.
- End every message with `\r\n`.
- Format numeric codes with three digits.
- Use the client’s nickname or `*` when it does not exist yet.
- Append replies to the `outputBuffer`.
- Enable `POLLOUT` when there is pending data.
- Avoid direct `send()` calls from handlers.
- Send the `001` reply when registration is complete.
- Reply with consistent errors to incorrect commands.
- Keep construction, queueing and writing of messages separate.
- Avoid duplicating prefixes and formats in different handlers.
