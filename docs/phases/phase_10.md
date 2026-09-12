# Phase 10 — Auxiliary connection commands

## Goal

Implement the auxiliary commands needed to keep, negotiate and close an IRC connection correctly:

- `PING` / `PONG`
- `QUIT`
- `CAP`

These commands make it easier for real IRC clients, such as Irssi, to connect and work correctly.

---

## 1. `PING` command

Clients use `PING` to check that the connection is still active.

Received example:

```irc
PING :token
```

The server must reply using the same token:

```irc
PONG :token
```

### Required checks

- Verify that a parameter has been received.
- Keep the received token.
- Allow `PING` before completing registration.
- Append the reply to the output buffer.
- Enable `POLLOUT` to send the reply in a non-blocking way.

If the parameter is missing, this can be sent:

```irc
:server.name 409 nickname :No origin specified
```

`send()` must not be called directly from the handler if the server already has a centralized write system.

---

## 2. `PONG` command

The server must generate the reply through a handler similar to:

```cpp
void Server::handlePing(Client &client, const Command &command);
```

The flow must be:

```text
PING received
    ↓
build PONG
    ↓
append it to outputBuffer
    ↓
enable POLLOUT
    ↓
send the pending data
```

---

## 3. `QUIT` command

`QUIT` lets a client close its connection voluntarily.

Examples:

```irc
QUIT
```

```irc
QUIT :Leaving
```

If no reason is provided, a default one can be used:

```text
Client Quit
```

### Leave notification

Users who share any channel with the client must receive:

```irc
:nickname!username@hostname QUIT :Leaving
```

Each user must receive the notification only once, even if they share several channels with the client.

To avoid duplicates, this can be used:

```cpp
std::set<Client *> recipients;
```

---

## 4. Complete client cleanup

After receiving `QUIT`, cleanup must be performed in this order:

1. Store the prefix and the leave reason.
2. Obtain the notification recipients.
3. Send the `QUIT` message.
4. Remove the client from all of its channels.
5. Remove it from the operator lists.
6. Remove it from the invite lists.
7. Delete the channels that have become empty.
8. Remove its nickname from the global index.
9. Remove its descriptor from the structure used by `poll()`.
10. Close the descriptor.
11. Destroy or delete the `Client` object.

If there is a global index such as:

```cpp
std::map<std::string, Client *> clientsByNickname;
```

the corresponding entry must be removed. Otherwise the nickname would stay occupied after the disconnection.

---

## 5. Centralizing disconnections

The same cleanup will be needed when:

- The client sends `QUIT`.
- `recv()` returns `0`.
- A fatal read error occurs.
- A fatal write error occurs.
- The client loses the connection.
- The server kicks the client.

It is useful to centralize the process:

```cpp
void Server::disconnectClient(int clientFileDescriptor,
                              const std::string &reason);
```

The `QUIT` handler should only obtain the reason and request the disconnection:

```cpp
void Server::handleQuit(Client &client, const Command &command);
```

This avoids duplicating the cleanup logic.

---

## 6. Avoid invalidating iterators

The client must not be removed from a collection while that same collection is being traversed if the operation can invalidate the iterators.

A safe strategy is:

1. Copy the client’s channel list.
2. Traverse the copy.
3. Remove the client from each channel.
4. Delete empty channels afterwards.

The `Client` object must also not be accessed after removing it from the main collection or destroying it.

---

## 7. `CAP` command

`CAP` is used to negotiate capabilities between the client and the server.

A real client may send:

```irc
CAP LS 302
```

Even if the server does not implement extra capabilities, it must reply so the client does not stay waiting.

A minimal reply would be:

```irc
:server.name CAP * LS :
```

The client can finish the negotiation by sending:

```irc
CAP END
```

### Minimum subcommands

#### `CAP LS`

Reports the available capabilities:

```irc
:server.name CAP * LS :
```

#### `CAP END`

Finishes capability negotiation.

It does not need to produce a reply, but it must allow registration to continue.

---

## 8. Capability negotiation state

Each client can store:

```cpp
bool capNegotiationActive;
```

On receiving:

```irc
CAP LS 302
```

negotiation is enabled:

```text
capNegotiationActive = true
```

On receiving:

```irc
CAP END
```

it is finished:

```text
capNegotiationActive = false
```

After `CAP END`, registration must be checked again:

```cpp
tryRegisterClient(client);
```

If `CAP` negotiation temporarily blocks registration, the requirements will be:

```text
password accepted
    +
valid and available nickname
    +
USER received
    +
CAP negotiation finished
```

A simpler version can also be implemented in which `CAP` does not block registration, but the server must at least reply to `CAP LS`.

---

## 9. Registering the handlers

The commands must be added to the dispatch system:

```text
PING → handlePing()
QUIT → handleQuit()
CAP  → handleCap()
```

The following commands must be allowed before the client completes registration:

- `PING`
- `QUIT`
- `CAP`

Therefore they must not reply with:

```irc
451 ERR_NOTREGISTERED
```

---

## 10. Recommended tests

### Test `PING`

Input:

```irc
PING :12345
```

Expected result:

```irc
PONG :12345
```

### Test `PING` without a parameter

Input:

```irc
PING
```

Expected result:

```irc
:server.name 409 nickname :No origin specified
```

### Test `QUIT`

Input:

```irc
QUIT :Goodbye
```

Expected result:

- Related users receive the `QUIT` message.
- The client disappears from all of its channels.
- The nickname becomes available again.
- The descriptor disappears from `poll()`.
- The connection is closed correctly.
- The `Client` object is no longer stored in the server.

### Test `CAP`

Input:

```irc
CAP LS 302
```

Expected result:

```irc
:server.name CAP * LS :
```

Then:

```irc
CAP END
```

The client must be able to complete registration normally.

---

## Expected result

At the end of this phase, the server must be able to:

- Keep connections alive through `PING` and `PONG`.
- Process voluntary closes through `QUIT`.
- Notify the leave to the affected users.
- Avoid duplicate notifications.
- Remove every reference of a disconnected client.
- Release its nickname correctly.
- Delete empty channels.
- Reply minimally to `CAP` negotiation.
- Allow real IRC clients to connect.
- Reuse the same process for voluntary and unexpected disconnections.
