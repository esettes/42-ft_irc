# Phase 3 — Main loop with a single `poll()`

## Goal of the phase

Implement the server’s main event loop using a single central `poll()` call.

At the end of this phase, the server must be able to:

* Accept several clients simultaneously.
* Keep every socket in non-blocking mode.
* Detect when there is data available to read.
* Send data only when the socket allows writing.
* Detect errors and disconnections.
* Close and remove disconnected clients correctly.
* Keep running even if one of the clients disconnects or causes an error.

In this phase it is still not necessary to interpret IRC commands.

---

## 1. Keep a collection of descriptors

The server must store every socket it needs to watch in a collection:

```cpp
std::vector<pollfd> pollDescriptors;
```

Normally the first element will always be the listening socket:

```text
pollDescriptors[0] → listening socket
pollDescriptors[1] → first client
pollDescriptors[2] → second client
pollDescriptors[3] → third client
```

Each `pollfd` structure contains:

* `fd`: socket descriptor.
* `events`: operations we want to watch.
* `revents`: events that actually occurred.

The listening socket must initially watch `POLLIN`:

```cpp
pollfd listeningDescriptor;

listeningDescriptor.fd = listeningSocketFileDescriptor;
listeningDescriptor.events = POLLIN;
listeningDescriptor.revents = 0;
```

On the listening socket, `POLLIN` means there is at least one pending connection that can be accepted through `accept()`.

---

## 2. Create a single main loop

The server must run a loop while it stays active:

```text
while server is active
    call poll()
    process listening socket
    process client sockets
```

There must be a single central `poll()` call in charge of watching:

* The listening socket.
* Reading from clients.
* Writing to clients.
* Errors.
* Disconnections.

A separate `poll()` must not be created for each operation.

---

## 3. Check the result of `poll()`

The `poll()` function can return:

* A value greater than `0`: there are descriptors with pending events.
* `0`: the wait time ended without any event occurring.
* `-1`: an error occurred.

If `poll()` returns `-1`:

* If `errno == EINTR`, the call was interrupted by a signal and the loop can continue.
* For other errors, the problem must be reported and the server terminated in a controlled way.

A timeout lets the server periodically regain control even if there is no activity:

```cpp
const int pollTimeoutMilliseconds = 1000;
```

`-1` can also be used to wait indefinitely, as long as signal-based termination is designed correctly.

---

## 4. Accept new clients

When the listening socket has the `POLLIN` event, you must call:

```cpp
accept();
```

After accepting a connection:

1. Obtain the new socket’s descriptor.
2. Configure it as non-blocking through `fcntl()`.
3. Create the client’s internal representation.
4. Add a new `pollfd` to `pollDescriptors`.
5. Configure it initially to watch `POLLIN`.
6. Show a message indicating that the client has connected.

The client descriptor should start with:

```cpp
clientDescriptor.events = POLLIN;
```

If the client’s configuration or registration fails after `accept()`, the newly created descriptor must be closed to avoid a resource leak.

---

## 5. Receive data from clients

When a client has the `POLLIN` event, it means `recv()` can run without blocking the server.

The result of `recv()` must be interpreted as follows:

### `recv()` returns a value greater than zero

Data has been received.

The bytes must be appended to the client’s input buffer:

```text
client input buffer += received data
```

In this phase it is still not necessary to interpret that data as IRC commands. The goal is to check that the server can receive them without blocking.

### `recv()` returns zero

The client has closed the connection in an orderly way.

The server must:

* Close its descriptor.
* Remove its internal information.
* Remove its corresponding `pollfd`.
* Report the disconnection.

### `recv()` returns `-1`

`errno` must be checked:

* `EAGAIN` or `EWOULDBLOCK`: no more data is available at that moment; it is not a disconnection.
* `EINTR`: the operation was interrupted; it can be tried again in a later iteration.
* Any other error: the client must be removed.

`recv()` must never be run on a client without first detecting `POLLIN` through the central `poll()`.

---

## 6. Send data to clients

Each client should have an output buffer with the data pending to send.

When that buffer is not empty, `POLLOUT` must be enabled:

```cpp
clientDescriptor.events |= POLLOUT;
```

When `poll()` reports `POLLOUT`, `send()` can be called.

`send()` may send fewer bytes than requested. Therefore:

1. Only the bytes that were actually sent are removed from the buffer.
2. The rest remains pending for the next `POLLOUT`.
3. When the buffer becomes empty, `POLLOUT` is disabled.

```cpp
clientDescriptor.events &= ~POLLOUT;
```

`POLLOUT` must not stay enabled permanently. A socket is usually available for writing almost all the time and this would make `poll()` return continuously, causing unnecessary CPU use.

`send()` must never be called if `poll()` has not previously reported `POLLOUT`.

---

## 7. Detect errors and disconnections

For each client the following events must also be checked:

* `POLLERR`: an error occurred on the socket.
* `POLLHUP`: the other end closed the connection.
* `POLLNVAL`: the descriptor is not valid.

On any of these events, the client must be removed safely.

If `POLLIN` and `POLLHUP` appear at the same time, there may be last pending data. A robust implementation processes the read first and then removes the client if it is still connected.

An error on one client must not stop the whole server.

---

## 8. Remove a client correctly

Removing a client implies performing all of these operations:

1. Obtain its descriptor.
2. Close the socket with `close()`.
3. Delete the `Client` object or its associated information.
4. Remove its entry from `pollDescriptors`.
5. Report the disconnection.

Care is needed when removing elements from a `std::vector` while it is being traversed.

When `erase()` is run, later elements change position. To avoid skipping a client:

* The vector can be traversed from back to front.
* Or the index can be controlled manually and not incremented after removing an element.
* Or clients can be marked and removed after processing the events.

The listening socket must not be treated as a client or removed through the normal disconnection logic.

---

## 9. Keep descriptors and clients synchronized

The server needs to be able to find the `Client` object associated with each descriptor.

One possible organization is to keep:

```cpp
std::vector<pollfd> pollDescriptors;
std::map<int, Client> clients;
```

The socket descriptor can be used as the identifier:

```text
descriptor → Client object
```

Every time a client is accepted, it must be added to both collections.

Every time it disconnects, it must be removed from both.

There must not remain:

* A descriptor inside `pollDescriptors` without an associated client.
* A registered client whose descriptor has already been closed.
* A closed descriptor that continues to be watched by `poll()`.

---

## 10. General loop flow

The conceptual behaviour must be:

```text
poll()
│
├── listener contains POLLIN
│   └── accept()
│       ├── configure non-blocking socket
│       ├── register client
│       └── add pollfd
│
├── client contains POLLIN
│   └── recv()
│       ├── data received → store in input buffer
│       ├── result 0 → disconnect
│       └── fatal error → disconnect
│
├── client contains POLLOUT
│   └── send()
│       ├── remove sent bytes
│       └── disable POLLOUT if no data remains
│
└── POLLERR, POLLHUP or POLLNVAL
    └── close and remove client
```

---

## 11. Recommended status messages

During this phase it is useful to show information such as:

```text
[SERVER] Event loop started
[SERVER] Waiting for events
[CLIENT] Connection accepted: fd=4
[CLIENT] Received 18 bytes: fd=4
[CLIENT] Connection closed: fd=4
[CLIENT] Socket error: fd=5
[SERVER] Shutting down
```

These messages make it easier to check that:

* Several clients are accepted.
* Each descriptor is processed correctly.
* Disconnections are detected.
* The server keeps running after removing a client.

---

## 12. Phase tests

### Start the server

```bash
./ircserv 6667 password
```

### Open several clients

From different terminals:

```bash
nc 127.0.0.1 6667
```

It must be possible to open several connections at the same time.

### Send data

Type text from each client and check that the server logs the reception without becoming blocked.

### Disconnect clients

Close one of the connections and check that:

* The server detects the disconnection.
* It closes the descriptor.
* It removes the client.
* The other clients stay connected.
* New clients can be connected afterwards.

---

## Criteria to consider the phase finished

Phase 3 is complete when:

* There is a single central `poll()` call.
* The listening socket is part of `pollDescriptors`.
* New clients are accepted through `POLLIN`.
* Accepted sockets are configured as non-blocking.
* Several clients can stay connected.
* `recv()` is only run after receiving `POLLIN`.
* `send()` is only run after receiving `POLLOUT`.
* `POLLOUT` is only enabled when there is pending data.
* Reads do not block the server.
* `recv() == 0` is detected correctly.
* `POLLERR`, `POLLHUP` and `POLLNVAL` are processed.
* Disconnected clients are closed and removed.
* No closed descriptors remain inside `pollDescriptors`.
* Removing a client does not cause other events to be skipped.
* Disconnecting a client does not stop the server.
* IRC commands are still not interpreted.
