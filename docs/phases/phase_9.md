# Phase 9 — Client registration

## Goal

Implement the process through which a TCP connection becomes a registered IRC client.

To complete registration, the client must have correctly sent:

1. `PASS`
2. `NICK`
3. `USER`

The server must store the state of each step and check after each command whether registration can already be completed.

---

## Required state in `Client`

Each client should store, at a minimum:

```cpp
bool passwordAccepted;
bool nicknameReceived;
bool usernameReceived;
bool registered;

std::string nickname;
std::string username;
std::string realName;
```

Each variable represents an independent part of registration:

- `passwordAccepted`: the password sent through `PASS` is correct.
- `nicknameReceived`: the client has a valid and available nickname.
- `usernameReceived`: the client has correctly sent `USER`.
- `registered`: the registration process has already been completed.

These states should not be replaced by a single boolean such as `authenticated`, because IRC registration depends on several different conditions.

---

## General registration flow

```text
Client connected
    ↓
correct PASS
    ↓
valid and available NICK
    ↓
valid USER
    ↓
tryRegisterClient()
    ↓
Client registered
    ↓
Welcome message
```

Although the commands are normally sent in the order `PASS`, `NICK` and `USER`, the server should check the state after each one.

The function that completes registration can have a structure similar to:

```cpp
void Server::tryRegisterClient(Client &client);
```

It must be called after correctly processing:

- `PASS`
- `NICK`
- `USER`

---

# `PASS` command

## Format

```irc
PASS secret
```

## Responsibility

The `PASS` command lets you check that the client knows the password the server was started with.

## Required validations

The `PASS` handler must check:

1. That a password has been provided.
2. That the client is not registered yet.
3. That the password matches the server password.

## Expected behaviour

If no parameter is provided:

```irc
PASS
```

The server must reply:

```irc
461 PASS :Not enough parameters
```

If the client is already registered:

```irc
462 :You may not reregister
```

If the password does not match:

```irc
464 :Password incorrect
```

If the password is correct:

```cpp
client.setPasswordAccepted(true);
```

Then it must be checked whether the client can already register:

```cpp
tryRegisterClient(client);
```

## Important consideration

An incorrect password must never be marked as accepted:

```cpp
client.setPasswordAccepted(false);
```

The client will not be able to complete registration while `passwordAccepted` is `false`.

---

# `NICK` command

## Format

```irc
NICK roxana
```

## Responsibility

The `NICK` command assigns a visible nickname to the client.

It can also be used after registration to change the nickname.

## Required validations

The `NICK` handler must check:

1. That a parameter exists.
2. That the nickname is not empty.
3. That its format is valid.
4. That it is not being used by another client.
5. If the client already had a nickname, update the global index correctly.
6. If the client was already registered, communicate the change to related clients.

## Missing nickname

If the client sends:

```irc
NICK
```

The server must reply:

```irc
431 :No nickname given
```

## Invalid format

If the nickname contains disallowed characters:

```irc
NICK rox@na
```

The server must reply:

```irc
432 rox@na :Erroneous nickname
```

## Nickname in use

If another client already uses the nickname:

```irc
433 roxana :Nickname is already in use
```

The client’s previous nickname must not be modified if the new nickname is rejected.

---

## Nickname validation

It is useful to centralize this check:

```cpp
bool Server::isValidNickname(const std::string &nickname) const;
```

As an initial criterion, you can require:

- That it is not empty.
- That it does not start with a number.
- That it does not contain spaces.
- That it does not contain `:`.
- That it does not contain `,`.
- That it does not contain `*`.
- That it does not contain `?`.
- That it does not contain `!`.
- That it does not contain `@`.
- That it does not contain control characters.

It is not necessary to implement the entire historical IRC specification if the subject does not require it, but the validation must be consistent.

---

# Global nickname index

The server needs to quickly locate which client uses a nickname.

A possible structure is:

```cpp
std::map<std::string, Client *> clientsByNickname;
```

The stored relationship will be:

```text
nickname → Client
```

For example:

```text
"roxana" → Client*
"alice"  → Client*
"bob"    → Client*
```

This avoids traversing every client each time a `NICK`, `PRIVMSG`, `KICK`, `INVITE` or other command that needs to look up users is processed.

---

## Nickname comparison

IRC normally treats nicknames without distinguishing between uppercase and lowercase.

For example, these names should be considered equivalent:

```text
roxana
Roxana
ROXANA
```

To achieve this, a normalized key can be generated:

```cpp
std::string Server::normalizeNickname(
    const std::string &nickname
) const;
```

The normalized key is used in the map:

```cpp
clientsByNickname[normalizeNickname(nickname)] = &client;
```

The `Client` object can keep the original nickname to display it in messages.

---

## Initial nickname assignment

When the nickname is valid and available:

1. It is stored on the client.
2. It is added to the global index.
3. `nicknameReceived` is marked.
4. Completing registration is attempted.

Flow:

```text
Validate nickname
    ↓
Check availability
    ↓
Store nickname
    ↓
Add it to clientsByNickname
    ↓
Mark nicknameReceived
    ↓
tryRegisterClient()
```

---

## Nickname change

A registered client can change its nickname:

```irc
NICK newRoxana
```

The update must be performed consistently:

1. Temporarily store the previous nickname.
2. Check that the new nickname is valid.
3. Check that the new nickname is available.
4. Remove the previous nickname from the index.
5. Store the new nickname on the client.
6. Add the new nickname to the index.
7. Notify the change to the clients that share channels with it.

The message must use the previous nickname in the prefix:

```irc
:roxana!username@hostname NICK :newRoxana
```

The previous nickname must not be removed from the index until it is confirmed that the new nickname can be used.

---

## Cleaning up the nickname on disconnect

When a client disconnects, its nickname must be removed from the global index:

```cpp
clientsByNickname.erase(normalizeNickname(client.getNickname()));
```

If it is not removed, the server could permanently consider occupied a nickname belonging to a client that no longer exists.

---

# `USER` command

## Format

```irc
USER roxana 0 * :Roxana Example
```

Its parameters represent:

```text
USER <username> <mode> <unused> :<realname>
```

For this project, it is normally enough to store:

- Username.
- Real name.

The intermediate fields can be validated and discarded, or kept if they are useful.

---

## Required validations

The `USER` handler must check:

1. That the client is not registered yet.
2. That there are enough parameters.
3. That the username is not empty.
4. That the real name exists.

If parameters are missing:

```irc
461 USER :Not enough parameters
```

If the client is already registered:

```irc
462 :You may not reregister
```

---

## Data that must be stored

For this message:

```irc
USER roxana 0 * :Roxana Example
```

The client should store:

```text
username = "roxana"
realName = "Roxana Example"
```

The final parameter may contain spaces because the parser must already have reconstructed it as a single parameter.

After storing the data:

```cpp
client.setUsernameReceived(true);
tryRegisterClient(client);
```

---

# `tryRegisterClient()` function

## Responsibility

This function centralizes the check of the registration requirements.

```cpp
void Server::tryRegisterClient(Client &client);
```

It must register the client only when all of these conditions are met:

```text
passwordAccepted == true
nicknameReceived == true
usernameReceived == true
registered == false
```

Possible logic is:

```cpp
void Server::tryRegisterClient(Client &client)
{
    if (client.isRegistered())
        return;

    if (!client.isPasswordAccepted())
        return;

    if (!client.hasNickname())
        return;

    if (!client.hasUsername())
        return;

    client.setRegistered(true);
    sendWelcomeMessages(client);
}
```

This function must be idempotent: calling it several times must not register the client again or repeat the welcome message.

---

# Welcome message

When registration is complete, the server must send the welcome message exactly once.

Minimum reply:

```irc
:server.name 001 roxana :Welcome to the IRC Network roxana
```

The client’s full prefix can also be included:

```irc
:server.name 001 roxana :Welcome to the IRC Network roxana!username@hostname
```

It is useful to centralize the send:

```cpp
void Server::sendWelcomeMessages(Client &client);
```

The reply must be appended to the output buffer through the system implemented in the previous phases.

For example:

```cpp
queueNumericReply(
    client,
    1,
    "Welcome to the IRC Network " + buildClientPrefix(client)
);
```

`send()` must not be called directly from the handler.

---

## Avoid duplicate welcome messages

The first check of `tryRegisterClient()` must be:

```cpp
if (client.isRegistered())
    return;
```

This prevents the server from sending `001` again if the client later uses:

```irc
NICK newName
```

A nickname change after registration is not a new registration.

---

# Restricting commands before registration

Before completing registration, the client should only be able to use the commands needed to connect, such as:

- `CAP`
- `PASS`
- `NICK`
- `USER`
- `PING`
- `QUIT`

If it tries to run a command that requires registration:

```irc
JOIN #general
```

The server must reply:

```irc
451 :You have not registered
```

This check can be centralized in the dispatcher or at the beginning of the protected handlers.

---

# Separation of responsibilities

The logic should be divided approximately like this:

```text
CommandDispatcher
    ├── identifies PASS, NICK or USER
    └── calls the corresponding handler

Server
    ├── handlePass()
    ├── handleNick()
    ├── handleUser()
    ├── tryRegisterClient()
    ├── sendWelcomeMessages()
    ├── isValidNickname()
    ├── isNicknameAvailable()
    └── normalizeNickname()

Client
    ├── stores nickname
    ├── stores username
    ├── stores real name
    └── stores registration state
```

The handlers must validate and modify the state, but the final decision to register the client must stay centralized in `tryRegisterClient()`.

---

# Required numeric errors

| Code | Name | Situation |
|---:|---|---|
| `431` | `ERR_NONICKNAMEGIVEN` | `NICK` does not contain a nickname |
| `432` | `ERR_ERRONEUSNICKNAME` | The nickname format is not valid |
| `433` | `ERR_NICKNAMEINUSE` | The nickname is already taken |
| `451` | `ERR_NOTREGISTERED` | A protected command is used before registration |
| `461` | `ERR_NEEDMOREPARAMS` | Parameters are missing in `PASS` or `USER` |
| `462` | `ERR_ALREADYREGISTERED` | `PASS` or `USER` is repeated after registration |
| `464` | `ERR_PASSWDMISMATCH` | The password is incorrect |
| `001` | `RPL_WELCOME` | Registration has completed correctly |

These replies must be built using the centralized message system from phase 8.

---

# Recommended test cases

## Correct registration

Input:

```irc
PASS secret
NICK roxana
USER roxana 0 * :Roxana Example
```

Expected result:

```irc
:server.name 001 roxana :Welcome to the IRC Network roxana!roxana@hostname
```

The client must be marked as registered.

---

## Different order of `NICK` and `USER`

Input:

```irc
PASS secret
USER roxana 0 * :Roxana Example
NICK roxana
```

Expected result:

- Registration is completed when `NICK` is received.
- The `001` message is sent only once.

---

## Incorrect password

Input:

```irc
PASS incorrect
NICK roxana
USER roxana 0 * :Roxana Example
```

Expected result:

```irc
464 :Password incorrect
```

The client must not register.

---

## `PASS` without a parameter

Input:

```irc
PASS
```

Expected result:

```irc
461 PASS :Not enough parameters
```

---

## `NICK` without a parameter

Input:

```irc
NICK
```

Expected result:

```irc
431 :No nickname given
```

---

## Invalid nickname

Input:

```irc
NICK rox@na
```

Expected result:

```irc
432 rox@na :Erroneous nickname
```

---

## Nickname in use

First client:

```irc
NICK roxana
```

Second client:

```irc
NICK roxana
```

Expected result for the second client:

```irc
433 roxana :Nickname is already in use
```

---

## Protected command before registration

Input:

```irc
JOIN #general
```

Expected result:

```irc
451 :You have not registered
```

---

## Attempt to repeat `USER`

After completing registration:

```irc
USER other 0 * :Other name
```

Expected result:

```irc
462 :You may not reregister
```

The client’s original data must not be modified.

---

## Nickname change after registration

Input:

```irc
NICK newRoxana
```

Expected result:

```irc
:roxana!username@hostname NICK :newRoxana
```

In addition:

- The previous nickname must become available.
- The new nickname must appear in the global index.
- The `001` message must not be sent again.

---

## Disconnection and nickname release

After disconnecting a client:

- Its nickname must be removed from the global index.
- Another client must be able to use that nickname.
- No invalid pointers must remain in `clientsByNickname`.

---

# Expected result of the phase

At the end of this phase, the server must be able to:

- Validate the server password.
- Assign valid and unique nicknames.
- Store the username and the real name.
- Keep a global nickname index.
- Detect when every registration requirement is met.
- Register the client only once.
- Send the `001` reply correctly.
- Reject protected commands before registration.
- Allow nickname changes after registration.
- Release the nickname when the client disconnects.
- Reply with the appropriate numeric errors.
