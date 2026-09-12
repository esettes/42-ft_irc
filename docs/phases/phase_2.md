# Phase 2 — Listening socket

## Goal

In this phase only the **IRC server listening socket** must be created and configured.

When running:

```bash
./ircserv 6667 password
```

the server must be listening for TCP connections on port `6667`.

From another terminal:

```bash
nc 127.0.0.1 6667
```

the TCP connection must be able to be established, even if the server still does not interpret commands or process messages.

---

## Scope of this phase

The following must be implemented:

1. Creating the socket with `socket()`.
2. Configuring `SO_REUSEADDR` with `setsockopt()`.
3. Configuring the socket as non-blocking with `fcntl()`.
4. Preparing the server address.
5. Associating the socket with the port with `bind()`.
6. Enabling listening mode with `listen()`.
7. Error handling and correct closing of the descriptor.

The following must **not** be implemented yet:

* `accept()`.
* Client management.
* `poll()`.
* Receiving data with `recv()`.
* Sending data with `send()`.
* IRC message parser.
* Command interpretation.
* User registration.
* Channels.

---

## 1. Create the server socket

A TCP socket over IPv4 must be created:

```cpp
socket(AF_INET, SOCK_STREAM, 0);
```

Meaning of the arguments:

* `AF_INET`: uses IPv4 addresses.
* `SOCK_STREAM`: creates a connection-oriented socket, used by TCP.
* `0`: lets the system automatically select the corresponding protocol, in this case TCP.

The returned value is a **file descriptor** that identifies the socket.

If `socket()` returns `-1`, an error has occurred and the server cannot continue.

The descriptor must be stored as an attribute of the `Server` class, for example:

```cpp
int _serverSocketFileDescriptor;
```

---

## 2. Configure `SO_REUSEADDR`

After creating the socket, the `SO_REUSEADDR` option must be enabled through `setsockopt()`:

```cpp
int optionValue = 1;

setsockopt(
    _serverSocketFileDescriptor,
    SOL_SOCKET,
    SO_REUSEADDR,
    &optionValue,
    sizeof(optionValue)
);
```

This option allows the address and port to be reused shortly after the server has been stopped.

Without this configuration, quickly restarting the program could produce an error such as:

```text
Address already in use
```

If `setsockopt()` returns `-1`, you must:

1. Store or inspect the error through `errno`.
2. Close the created socket.
3. Stop server initialization.

---

## 3. Make the socket non-blocking

The listening socket must be configured as non-blocking through `fcntl()`.

First obtain its current flags:

```cpp
int currentFlags = fcntl(
    _serverSocketFileDescriptor,
    F_GETFL,
    0
);
```

Then add the non-blocking flags:

```cpp
fcntl(
    _serverSocketFileDescriptor,
    F_SETFL,
    currentFlags | O_NONBLOCK
);
```

It is important to keep the previous flags using:

```cpp
currentFlags | O_NONBLOCK
```

The whole descriptor configuration must not be replaced directly.

Even though `accept()` is not used yet in this phase, configuring the socket as non-blocking prepares the server for the future management of multiple clients through `poll()`.

The values returned by both `fcntl()` calls must be checked separately. A result of `-1` indicates an error.

---

## 4. Prepare the server address

To indicate which address and port the socket must listen on, a `sockaddr_in` structure is used:

```cpp
struct sockaddr_in serverAddress;
```

Before filling it, all of its memory must be initialized to zero:

```cpp
std::memset(
    &serverAddress,
    0,
    sizeof(serverAddress)
);
```

Then its fields are configured:

```cpp
serverAddress.sin_family = AF_INET;
serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
serverAddress.sin_port = htons(port);
```

Meaning:

* `sin_family = AF_INET`: the address uses IPv4.
* `INADDR_ANY`: the server accepts connections directed at any of the machine’s network interfaces, including `127.0.0.1`.
* `htons(port)`: converts the port to the byte order used by the network.
* `htonl(INADDR_ANY)`: converts the address to network byte order.

The port must already have been validated during phase 1.

---

## 5. Associate the socket with `bind()`

`bind()` associates the socket with the configured address and port:

```cpp
bind(
    _serverSocketFileDescriptor,
    reinterpret_cast<struct sockaddr *>(&serverAddress),
    sizeof(serverAddress)
);
```

After this call, the socket is associated with the port given when the server was started.

For example:

```bash
./ircserv 6667 password
```

will associate the socket with port `6667`.

If `bind()` returns `-1`, the most common causes are:

* The port is already being used.
* The port is not valid.
* The process does not have permission to use that port.
* The address is misconfigured.

On error, the socket must be closed before finishing initialization.

---

## 6. Enable listening mode with `listen()`

After `bind()` has completed successfully, the socket must be put into listening mode:

```cpp
listen(
    _serverSocketFileDescriptor,
    SOMAXCONN
);
```

`SOMAXCONN` indicates that the maximum pending-connection queue allowed by the system will be used.

From this moment, the operating system can receive TCP connection requests directed at the port.

If `listen()` returns `-1`, the socket must be closed and initialization stopped.

---

## 7. Keep the server running

After running `listen()`, the process must stay active.

If `main()` finishes immediately, the `Server` destructor will close the socket and `nc` will not be able to connect.

In this phase it is still not necessary to accept or process clients, but the server must stay running until it receives the termination signal handled in phase 1.

---

## 8. Handle errors correctly

Every system call must check its return value:

| Function       | Error |
| -------------- | ----: |
| `socket()`     |  `-1` |
| `setsockopt()` |  `-1` |
| `fcntl()`      |  `-1` |
| `bind()`       |  `-1` |
| `listen()`     |  `-1` |

If an error occurs after the socket has been created, its descriptor must be closed:

```cpp
close(_serverSocketFileDescriptor);
```

It is also useful to set it to an invalid value to avoid closing it twice:

```cpp
_serverSocketFileDescriptor = -1;
```

The error message can be built using:

```cpp
std::strerror(errno)
```

Initialization must not continue after one of these operations fails.

---

## Implementation order

The correct order of operations is:

```text
socket()
    ↓
setsockopt(SO_REUSEADDR)
    ↓
fcntl(F_GETFL)
    ↓
fcntl(F_SETFL, O_NONBLOCK)
    ↓
prepare sockaddr_in
    ↓
bind()
    ↓
listen()
```

`bind()` must not be called before creating and configuring the socket, nor `listen()` before `bind()` has completed successfully.

---

## Manual check

### 1. Compile the server

```bash
make
```

### 2. Run it

```bash
./ircserv 6667 password
```

The program must stay active and must not immediately return the terminal prompt.

### 3. Check the port

In another terminal:

```bash
ss -ltnp | grep ':6667'
```

The port must appear in state:

```text
LISTEN
```

### 4. Test the TCP connection

```bash
nc 127.0.0.1 6667
```

`nc` must be able to establish the connection and stay waiting.

In this phase, typing text in `nc` does not have to produce any reply, because data is not received yet and commands are not interpreted.

### 5. Check an occupied port

With a server instance already running, you can try to start another:

```bash
./ircserv 6667 password
```

The second instance must detect the `bind()` error and terminate in a controlled way, showing a message similar to:

```text
bind: Address already in use
```

---

## Criteria to consider the phase finished

Phase 2 is complete when:

* The TCP socket is created correctly.
* `SO_REUSEADDR` is enabled.
* The socket is configured as non-blocking.
* The `sockaddr_in` structure is correctly initialized.
* The socket is associated with the port received as an argument.
* The socket enters the `LISTEN` state.
* `nc 127.0.0.1 6667` can establish a TCP connection.
* Every system-call error is checked.
* The socket is closed correctly when the server is stopped.
* Client management and IRC commands have not been implemented yet.
