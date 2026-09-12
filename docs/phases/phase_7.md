# Phase 7 — Output buffer and non-blocking writes

## Goal

Implement sending IRC replies in a non-blocking way.

A call to `send()` does not guarantee that every requested byte is sent. The operating system may accept only part of it, so each client must keep a buffer with the bytes that are still pending to send.

The general flow will be:

```text
An IRC reply is generated
        ↓
It is appended to the client’s output buffer
        ↓
POLLOUT is enabled
        ↓
poll() reports that the socket allows writing
        ↓
send() tries to send the data
        ↓
Only the sent bytes are removed
        ↓
If data remains, POLLOUT stays enabled
        ↓
If the buffer becomes empty, POLLOUT is disabled
```

---

## 1. Add an output buffer to each client

Each `Client` object must store its own output buffer:

```cpp
class Client
{
private:
    int _socketFileDescriptor;
    std::string _outputBuffer;
};
```

The buffer must belong to the client because each connection may send data at a different speed.

One client may receive every reply immediately, while another may take longer and temporarily accumulate pending data.

---

## 2. Append replies to the buffer

When the server wants to send a reply, it must not assume it can call `send()` directly and send the whole message.

Instead, it must append the reply to the buffer:

```cpp
void Client::appendToOutputBuffer(const std::string &message)
{
    _outputBuffer += message;
}
```

Example:

```cpp
client.appendToOutputBuffer(
    ":server 001 roxana :Welcome to the IRC server\r\n"
);
```

IRC replies must end with `\r\n`.

If the client already has pending information, the new reply is appended to the end of the buffer to preserve message order.

---

## 3. Query the output buffer

The `Client` class must allow querying the pending data:

```cpp
const std::string &Client::getOutputBuffer() const
{
    return _outputBuffer;
}
```

It is also useful to have a function that indicates whether pending data exists:

```cpp
bool Client::hasPendingOutput() const
{
    return !_outputBuffer.empty();
}
```

---

## 4. Enable `POLLOUT` when there is pending data

`POLLOUT` indicates that the socket is ready to accept data through `send()`.

It must only be enabled when the client has pending information:

```cpp
pollFileDescriptor.events = POLLIN;

if (client.hasPendingOutput())
    pollFileDescriptor.events |= POLLOUT;
```

A client’s events will have these functions:

- `POLLIN`: indicates that there is data available to read.
- `POLLOUT`: indicates that sending pending information can be attempted.

Enabling `POLLOUT` does not guarantee that the whole buffer can be sent. It only indicates that it makes sense to try calling `send()`.

---

## 5. Check `POLLOUT` after `poll()`

After running `poll()`, the `revents` field must be checked:

```cpp
if (pollFileDescriptor.revents & POLLOUT)
    handleClientWrite(pollFileDescriptor.fd);
```

The function in charge of writing must try to send the current buffer content:

```cpp
ssize_t sentByteCount = send(
    client.getSocketFileDescriptor(),
    client.getOutputBuffer().data(),
    client.getOutputBuffer().size(),
    MSG_NOSIGNAL
);
```

The value returned by `send()` indicates how many bytes were actually sent.

---

## 6. Handle partial sends

`send()` may send fewer bytes than requested.

Example:

```text
Initial outputBuffer size: 200 bytes
Bytes sent by send():      80 bytes
Bytes still pending:       120 bytes
```

In this case only the first 80 bytes must be removed:

```cpp
void Client::removeSentOutput(std::size_t sentByteCount)
{
    if (sentByteCount >= _outputBuffer.size())
    {
        _outputBuffer.clear();
        return;
    }

    _outputBuffer.erase(0, sentByteCount);
}
```

Use:

```cpp
if (sentByteCount > 0)
{
    client.removeSentOutput(
        static_cast<std::size_t>(sentByteCount)
    );
}
```

The whole buffer must never be emptied without checking how many bytes `send()` actually sent.

---

## 7. Handle the results of `send()`

### Successful send

If `send()` returns a value greater than zero, that number of bytes has been sent correctly:

```cpp
if (sentByteCount > 0)
{
    client.removeSentOutput(
        static_cast<std::size_t>(sentByteCount)
    );

    return;
}
```

### Socket temporarily unavailable

If `send()` returns `-1` and `errno` contains `EAGAIN` or `EWOULDBLOCK`, the socket cannot accept more data at that moment:

```cpp
if (
    sentByteCount == -1
    && (errno == EAGAIN || errno == EWOULDBLOCK)
)
{
    return;
}
```

The connection must not be closed and the buffer must not be modified.

The data will stay stored and the server will try again when `poll()` reports `POLLOUT` again.

### Call interrupted by a signal

If `errno` contains `EINTR`, the call was interrupted by a signal:

```cpp
if (sentByteCount == -1 && errno == EINTR)
    return;
```

Data must not be removed from the buffer either.

### Fatal error

Any other error normally indicates a real problem with the connection:

```cpp
if (sentByteCount == -1)
{
    disconnectClient(
        client.getSocketFileDescriptor()
    );
}
```

Removing the client must be done safely, avoiding invalidating iterators that are still in use.

---

## 8. Disable `POLLOUT` when the buffer becomes empty

When all data has been sent, `POLLOUT` must stop being watched:

```cpp
pollFileDescriptor.events &= ~POLLOUT;
```

Another option is to rebuild the client’s events on each iteration:

```cpp
pollFileDescriptor.events = POLLIN;

if (client.hasPendingOutput())
    pollFileDescriptor.events |= POLLOUT;
```

This avoids keeping `POLLOUT` enabled when there is nothing to send.

A socket is usually available for writing most of the time. If `POLLOUT` stays always enabled, `poll()` may wake up constantly even if there is no pending work.

This can cause:

- Unnecessary CPU use.
- Useless iterations of the main loop.
- A busy-wait loop.

---

## 9. Avoid `SIGPIPE`

If `send()` is called on a socket whose connection has been closed, the process may receive the `SIGPIPE` signal.

On Linux this can be avoided by using `MSG_NOSIGNAL`:

```cpp
ssize_t sentByteCount = send(
    client.getSocketFileDescriptor(),
    client.getOutputBuffer().data(),
    client.getOutputBuffer().size(),
    MSG_NOSIGNAL
);
```

This way `send()` will return an error that can be handled without the server terminating unexpectedly.

---

## 10. Limit the output buffer size

It is recommended to set a maximum size to prevent a slow client from accumulating messages indefinitely:

```cpp
const std::size_t MAXIMUM_OUTPUT_BUFFER_SIZE = 65536;
```

Before appending a reply:

```cpp
if (
    client.getOutputBuffer().size() + message.size()
    > MAXIMUM_OUTPUT_BUFFER_SIZE
)
{
    disconnectClient(
        client.getSocketFileDescriptor()
    );

    return;
}

client.appendToOutputBuffer(message);
```

This protects the server against clients that do not read replies and cause continuous memory growth.

---

## Recommended minimum structure for `Client`

```cpp
class Client
{
private:
    int _socketFileDescriptor;
    std::string _outputBuffer;

public:
    void appendToOutputBuffer(
        const std::string &message
    );

    const std::string &getOutputBuffer() const;

    bool hasPendingOutput() const;

    void removeSentOutput(
        std::size_t sentByteCount
    );
};
```

---

## Server responsibilities

The server must take care of:

1. Generating the IRC reply.
2. Appending it to the client’s output buffer.
3. Enabling `POLLOUT`.
4. Waiting for `poll()` to report that the socket allows writing.
5. Calling `send()`.
6. Removing only the bytes that were actually sent.
7. Keeping `POLLOUT` enabled if data still remains.
8. Disabling `POLLOUT` when the buffer becomes empty.
9. Handling `EAGAIN`, `EWOULDBLOCK` and `EINTR`.
10. Disconnecting the client on fatal errors.
11. Limiting the maximum buffer size.

---

## Expected result of the phase

At the end of this phase, the server must be able to:

- Keep an independent output buffer for each client.
- Append replies to the buffer without blocking the server.
- Send information only when `poll()` reports `POLLOUT`.
- Handle partial sends correctly.
- Keep the bytes that have not been sent yet.
- Retry pending sends.
- Disable `POLLOUT` when the buffer becomes empty.
- Avoid unnecessary CPU use.
- Handle `send()` errors correctly.
- Prevent `SIGPIPE` from closing the server unexpectedly.
- Limit the memory consumed by slow clients.
- Keep the correct order of IRC replies.
