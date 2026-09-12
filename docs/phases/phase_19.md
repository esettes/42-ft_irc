# Phase 19 — Robustness and adversarial tests

## Goal

In this phase it must be checked that the IRC server behaves correctly when facing incomplete input, invalid commands, partial writes, unexpected disconnections and edge states.

No new main features are added. The goal is to detect:

- Data loss.
- Inconsistent states.
- Invalid memory accesses.
- Memory leaks.
- Unclosed descriptors.
- Errors caused by the real behaviour of TCP.

---

## 1. TCP framing

TCP transmits a continuous byte stream. A call to `recv()` does not guarantee receiving a complete IRC command.

A command may arrive split into several fragments:

```text
"PRIV"
"MSG #general :he"
"llo\r"
"\n"
```

The server must accumulate them until it can reconstruct:

```text
PRIVMSG #general :hello
```

### Required behaviour

For each client:

1. Append the received bytes to their input buffer.
2. Search for complete commands ending in `\n`.
3. Extract and process only the complete commands.
4. Keep any incomplete fragment in the buffer.
5. Continue processing while complete lines remain.
6. Remove the `\r` located before the `\n`, when it exists.

An incomplete fragment must never be sent directly to the parser.

---

## 2. Several commands in the same reception

A single call to `recv()` can also return several complete commands:

```text
"NICK one\r\nUSER one 0 * :One\r\nJOIN #a\r\n"
```

The server must separate and process individually:

```text
NICK one
USER one 0 * :One
JOIN #a
```

It must not process the whole content as if it were a single command.

---

## 3. Line terminators

The following two terminators must be tested:

```text
\r\n
\n
```

The server can be tolerant and accept `\n` as the end of a command.

However, every message sent by the server must obligatorily end with:

```text
\r\n
```

Example:

```text
:irc.server 001 roxana :Welcome to the IRC Network\r\n
```

---

## 4. Partial writes

A call to `send()` may send fewer bytes than requested, especially when:

- The socket is non-blocking.
- The system buffer is full.
- A large amount of information is being sent.
- The client receives data slowly.

Conceptual example:

```cpp
const ssize_t bytesSent = send(
    clientFileDescriptor,
    outputBuffer.data(),
    outputBuffer.size(),
    0
);
```

If `bytesSent` is smaller than `outputBuffer.size()`, the whole message must not be removed.

Only the bytes sent correctly must be removed from the buffer:

```cpp
outputBuffer.erase(0, bytesSent);
```

The remaining bytes must be kept so they can be sent again when `poll()` reports that the socket is ready for writing through `POLLOUT`.

### Cases that must be handled

- `send()` returns a positive number smaller than the requested size.
- `send()` returns `-1` with `errno == EAGAIN`.
- `send()` returns `-1` with `errno == EWOULDBLOCK`.
- `send()` returns `-1` with `errno == EINTR`.
- `send()` returns a fatal error.
- The client disconnects while it still has pending messages.

### Expected result

- Bytes are not lost.
- Fragments are not duplicated.
- Message order is kept.
- Pending messages are deleted when the client is destroyed.
- `POLLOUT` is only requested while pending data exists.

---

## 5. Protocol errors

Incomplete commands, invalid parameters and nonexistent resources must be tested.

### Registration

```text
PASS
PASS incorrectPassword
NICK
NICK existingNick
USER
USER roxana
```

### Channels

```text
JOIN
JOIN invalid
JOIN #missingKey
PART
PART #nonexistent
```

### Messages

```text
PRIVMSG
PRIVMSG roxana
PRIVMSG nobody :hello
PRIVMSG #nonexistent :hello
```

### Operator commands

```text
MODE
MODE #channel
MODE #channel +k
MODE #channel +l
MODE #channel +o
MODE #channel +o nobody
KICK
KICK #channel
KICK #channel nobody
INVITE
INVITE nobody #channel
TOPIC
TOPIC #nonexistent
```

### Expected behaviour

For each invalid command, the server must:

- Not close unexpectedly.
- Not access nonexistent parameters.
- Not modify state partially.
- Send a consistent numeric reply.
- Keep the rest of the clients working.
- Allow the client to continue sending commands.

---

## 6. Problematic disconnections

Disconnections in different states must be tested.

### Test cases

- Disconnect an unregistered client.
- Disconnect a registered user without channels.
- Disconnect a user present in several channels.
- Disconnect an operator.
- Disconnect the only operator of a channel.
- Disconnect the last member of a channel.
- Disconnect a user who appears in invite lists.
- Disconnect a client with pending messages.
- Receive `POLLHUP`.
- Receive `POLLERR`.
- Receive `POLLNVAL`.
- Receive `recv() == 0`.
- Receive a fatal `recv()` error.
- Run the `QUIT` command.
- Close the program while clients are connected.

### Expected behaviour

Every disconnection must go through a single function:

```cpp
void Server::disconnectClient(
    int clientFileDescriptor,
    const std::string &reason
);
```

This function must take care of:

1. Notifying `QUIT` to the affected users.
2. Removing the client from every channel.
3. Removing them from the operator collections.
4. Removing them from the invite lists.
5. Removing their nickname from the global index.
6. Removing their descriptor from `poll()`.
7. Closing the descriptor.
8. Deleting the `Client` object.
9. Deleting the channels that have become empty.

The function must avoid sending the same `QUIT` several times to users who shared more than one channel with the disconnected client.

---

## 7. Channel-related edge cases

It must also be checked that:

- The first user creates the channel and becomes an operator.
- A channel is deleted when its last member leaves.
- A kicked operator no longer appears in `operators`.
- An invited user who disconnects no longer appears in `invited`.
- A user cannot be duplicated in `members`.
- A user cannot be an operator without belonging to the channel.
- The `+l` limit is respected exactly.
- The `+k` key is checked correctly.
- Mode `+i` lets invited users enter.
- The invitation is consumed after a successful `JOIN`.
- `KICK`, `PART`, `QUIT` and an unexpected disconnection keep the same final state.

---

## 8. Protection against messages that are too large

It is useful to limit the size of each client’s input buffer.

If a client sends data indefinitely without any terminator, the buffer must not grow without a limit.

A reasonable maximum size must be defined and:

- Excessively large messages must be rejected.
- The corresponding state must be cleared.
- The client must be disconnected if they are sending invalid data continuously.
- Unlimited memory consumption must be avoided.

The traditional IRC protocol limits each message to `512` bytes including `\r\n`, although the exact behaviour can be adapted to the project requirements.

---

## 9. Tests with several clients

The tests must not be performed with only one client.

Several clients must be connected simultaneously and it must be checked:

- Independent registration.
- Unique nicknames.
- Simultaneous entry into channels.
- Private messages.
- Channel messages.
- Topic changes.
- Invitations.
- Kicks.
- Channel modes.
- Unexpected disconnections.
- Closing one client while another stays connected.

Several terminals can be opened:

```bash
nc 127.0.0.1 6667
```

It is also useful to test the server with the chosen reference client, for example `irssi`.

---

## 10. Checking memory and descriptors

Run the server with Valgrind:

```bash
valgrind --leak-check=full \
    --show-leak-kinds=all \
    --track-origins=yes \
    --track-fds=yes \
    ./ircserv 6667 secret
```

During execution:

1. Connect several clients.
2. Register them.
3. Create several channels.
4. Send messages.
5. Run `JOIN`, `PART`, `KICK`, `INVITE`, `TOPIC` and `MODE`.
6. Disconnect clients in different ways.
7. Close the server.

### Expected result

At the end there should not remain:

- Definitely lost memory blocks.
- Clients that were not destroyed.
- Channels that were not destroyed.
- Pending buffers that were not freed.
- Open client descriptors.
- The listening socket descriptor open.
- Accesses to freed memory.
- Reads or writes outside the limits.

---

## 11. Tests with sanitizers

If the environment allows it, it is also useful to compile temporarily with sanitizers:

```bash
-fsanitize=address,undefined -g3
```

Example:

```bash
c++ -Wall -Wextra -Werror -std=c++98 \
    -fsanitize=address,undefined \
    -g3 \
    src/*.cpp \
    -o ircserv
```

These tools help detect:

- Buffer overflows.
- Use of memory after freeing it.
- Out-of-range accesses.
- Double frees.
- Undefined behaviour.

These options are for debugging and do not have to form part of the project’s final compilation.

---

## 12. Final checklist

Before considering the phase finished, verify that:

- [ ] Fragmented commands are reconstructed correctly.
- [ ] Grouped commands are separated correctly.
- [ ] The chosen terminators are accepted correctly.
- [ ] Every reply uses `\r\n`.
- [ ] Partial writes keep the pending bytes.
- [ ] `EAGAIN`, `EWOULDBLOCK` and `EINTR` are handled correctly.
- [ ] Invalid commands do not cause unexpected closes.
- [ ] Errors produce consistent numeric replies.
- [ ] Every disconnection goes through a single function.
- [ ] No clients remain in channels after disconnecting.
- [ ] No operators remain who are no longer members.
- [ ] No invitations remain to nonexistent clients.
- [ ] Empty channels are deleted.
- [ ] The same `QUIT` is not sent several times to the same user.
- [ ] The buffers have a maximum size.
- [ ] The server works with several simultaneous clients.
- [ ] Valgrind does not detect memory leaks.
- [ ] Valgrind does not detect open descriptors.
- [ ] The sanitizers do not detect invalid accesses.
- [ ] The server keeps working after receiving problematic input.

---

## Result of the phase

At the end of this phase, the server must be able to support real TCP traffic, malformed input, several simultaneous clients and unexpected disconnections without losing data, leaving inconsistent states or producing memory errors.
