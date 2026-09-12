# Phase 5 — Reconstructing the TCP stream

## Goal

Implement the reconstruction of complete IRC messages from the bytes received through `recv()`.

TCP transmits a **continuous byte stream** and does not preserve the separation between the messages that were sent. That is why a call to `recv()` may return:

- Part of a command.
- A complete command.
- Several commands together.
- One or more complete commands together with part of the next one.

Received data must not be sent directly to the IRC parser.

---

## 1. Add a persistent buffer to each client

Each `Client` object must store the received data that does not yet form a complete command:

```cpp
std::string _inputBuffer;
```

The buffer must belong to the client because each TCP connection has its own data stream.

There must not be a single buffer shared among all clients.

---

## 2. Append received data to the buffer

When `recv()` returns a positive number of bytes, they must be appended to the end of the client’s buffer:

```text
recv()
   ↓
received bytes
   ↓
inputBuffer
```

Conceptually:

```cpp
client.appendInput(receivedData, receivedByteCount);
```

Data that was already stored must not be removed until a complete IRC line has been reconstructed.

---

## 3. Interpret the result of `recv()`

The value returned by `recv()` must be handled as follows:

- `receivedByteCount > 0`: data has been received and must be appended to the buffer.
- `receivedByteCount == 0`: the client has closed the connection.
- `receivedByteCount == -1`: an error has occurred or there is no more data available.

On non-blocking sockets:

- `EAGAIN` and `EWOULDBLOCK` indicate that no more data is currently available. The client must not be disconnected.
- `EINTR` indicates that the call was interrupted by a signal. The operation can be retried.
- Other errors may require closing the client’s connection.

---

## 4. Extract only complete lines

IRC messages normally end with:

```text
\r\n
```

After appending the received bytes, this terminator must be searched for inside `_inputBuffer`.

While there is at least one complete line:

1. Locate the position of `\r\n`.
2. Extract the content before the terminator.
3. Remove the extracted line and its `\r\n` from the buffer.
4. Deliver the complete line to the next server level.
5. Repeat the process in case there are more complete commands.

Data after the last terminator must stay in the buffer for the next call to `recv()`.

---

## 5. Handle fragmented commands

TCP may split a command across several receptions.

Example:

```text
First recv():    "PRIV"
Second recv():   "MSG #general :Hello"
Third recv():    "\r\n"
```

Buffer evolution:

```text
"PRIV"
"PRIVMSG #general :Hello"
"PRIVMSG #general :Hello\r\n"
```

Only after receiving `\r\n` should this be extracted:

```text
PRIVMSG #general :Hello
```

The parser must never receive separately:

```text
PRIV
MSG #general :Hello
```

The subject test deliberately splits a word across several sends to check that the server reconstructs the TCP stream correctly.

---

## 6. Handle several commands in one reception

Several commands can also be received in a single call to `recv()`:

```text
PASS secret\r\nNICK roxana\r\nUSER roxana 0 * :Roxana\r\n
```

The system must extract three independent lines:

```text
PASS secret
NICK roxana
USER roxana 0 * :Roxana
```

It is not enough to look for a single terminator. Extraction must be repeated while the buffer contains complete lines.

---

## 7. Keep incomplete fragments

A reception may contain complete commands and part of the next one:

```text
PASS secret\r\nNICK roxana\r\nUS
```

These must be extracted:

```text
PASS secret
NICK roxana
```

The buffer must keep:

```text
US
```

If later this arrives:

```text
ER roxana 0 * :Roxana\r\n
```

The buffer will then contain:

```text
USER roxana 0 * :Roxana\r\n
```

Then this can be extracted:

```text
USER roxana 0 * :Roxana
```

---

## 8. Separate framing and parsing

In this phase it is still not necessary to interpret the internal structure of IRC commands.

The following flow must be kept:

```text
recv()
   ↓
append bytes to the buffer
   ↓
search for \r\n terminators
   ↓
extract complete lines
   ↓
send each complete line to the parser
```

Responsibilities:

- `recv()` obtains bytes from the socket.
- `Client` keeps the pending bytes.
- The **framing** system reconstructs complete lines.
- The parser later interprets each IRC line.

Main architectural rule:

> `recv()` must not call the parser directly with the bytes it just received.

The parser must only receive complete IRC lines.

---

## 9. Recommended methods for `Client`

The `Client` class can provide the following methods:

```cpp
void appendInput(
    const char *receivedData,
    std::size_t receivedByteCount
);

bool extractNextLine(std::string &line);
```

### `appendInput()` method

It must append to the buffer exactly the number of bytes indicated by `receivedByteCount`.

It must not assume that the received data ends with `'\0'`.

Conceptual example:

```cpp
void Client::appendInput(
    const char *receivedData,
    std::size_t receivedByteCount
)
{
    _inputBuffer.append(receivedData, receivedByteCount);
}
```

### `extractNextLine()` method

It must:

- Search for the next `\r\n`.
- Return `false` if a complete line does not exist yet.
- Store the extracted line in the `line` parameter.
- Remove the line and its terminator from the buffer.
- Return `true` when a line has been extracted correctly.

Conceptual example:

```cpp
bool Client::extractNextLine(std::string &line)
{
    const std::size_t lineEndingPosition =
        _inputBuffer.find("\r\n");

    if (lineEndingPosition == std::string::npos)
        return false;

    line = _inputBuffer.substr(0, lineEndingPosition);

    _inputBuffer.erase(
        0,
        lineEndingPosition + 2
    );

    return true;
}
```

Conceptual use:

```cpp
std::string line;

while (client.extractNextLine(line))
{
    processCompleteLine(client, line);
}
```

---

## 10. Control line size

A client could send data indefinitely without including any terminator:

```text
AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA...
```

If no limit is set, `_inputBuffer` could grow indefinitely and consume all of the server’s memory.

A traditional IRC message can occupy at most:

```text
512 bytes including \r\n
```

Therefore the content before the terminator can occupy at most:

```text
510 bytes
```

A policy for excessive input must be defined, for example:

- Reject the line.
- Clear the buffer.
- Disconnect the client.

For a first implementation, disconnecting the client that sends excessive input is a simple and safe solution.

A general limit for the pending buffer can also be set as extra protection.

---

## 11. Required tests

### Test 1 — Complete command

Send:

```text
NICK roxana\r\n
```

Expected result:

```text
NICK roxana
```

---

### Test 2 — Fragmented command

Send separately:

```text
"NI"
"CK ro"
"xana\r"
"\n"
```

Expected result:

```text
NICK roxana
```

A single line must be extracted and only after receiving the final `\n`.

---

### Test 3 — Several commands together

Send:

```text
PASS secret\r\nNICK roxana\r\nUSER roxana 0 * :Roxana\r\n
```

Expected result:

```text
PASS secret
NICK roxana
USER roxana 0 * :Roxana
```

---

### Test 4 — Complete commands and pending fragment

Send:

```text
PASS secret\r\nNICK roxana\r\nUS
```

These must be extracted:

```text
PASS secret
NICK roxana
```

The buffer must keep:

```text
US
```

Then send:

```text
ER roxana 0 * :Roxana\r\n
```

This must be extracted:

```text
USER roxana 0 * :Roxana
```

---

### Test 5 — Disconnected client

If `recv()` returns `0`, the server must:

1. Close the client’s file descriptor.
2. Remove it from the client collection.
3. Remove its corresponding entry from `poll()`.
4. Free any associated resource.

---

### Test 6 — Input without a terminator

Send a large amount of data without `\r\n`.

The server must prevent the buffer from growing indefinitely and apply the policy defined for excessive input.

---

## Completion criterion

The phase will be complete when:

- Each client has its own persistent input buffer.
- Received bytes are appended to the corresponding buffer.
- Fragmented commands are reconstructed correctly.
- Several commands received together can be extracted.
- Incomplete fragments remain stored.
- Extraction is repeated while complete lines exist.
- The parser only receives complete IRC lines.
- Disconnection and `recv()` errors are handled correctly.
- The buffer cannot grow indefinitely.
