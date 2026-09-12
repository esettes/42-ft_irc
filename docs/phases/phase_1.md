# Phase 1 — Server skeleton and lifecycle

The goal of this phase is to prepare how the server starts, stays running and terminates. Connections are not created yet and IRC commands are not processed.

## 1. Minimum structure

```text
ft_irc/
├── include/
│   ├── Server.hpp
│   └── SignalHandler.hpp
├── src/
│   ├── main.cpp
│   ├── Server.cpp
│   └── SignalHandler.cpp
└── Makefile
```

Each file must have a clear responsibility:

- `main.cpp`: validates the arguments and runs the server.
- `Server`: manages the lifecycle and the resources.
- `SignalHandler`: detects shutdown requests.
- `Makefile`: compiles the project.

## 2. Validate the number of arguments

The program is run like this:

```bash
./ircserv <port> <password>
```

Therefore `main()` must receive exactly three elements:

```cpp
if (argumentCount != 3)
{
    printUsage(argumentValues[0]);
    return 1;
}
```

`argumentValues[0]` is the program name.

## 3. Validate the port

The port must:

- Not be empty.
- Contain only digits.
- Convert without overflow.
- Be between `1` and `65535`.

It is not a good idea to use only `atoi()`, because it partially accepts invalid text:

```text
atoi("12abc") → 12
```

It is better to check the characters and use `strtol()`.

## 4. Validate the password

The password cannot be empty:

```cpp
if (passwordArgument.empty())
{
    throw std::invalid_argument(
        "password cannot be empty"
    );
}
```

You do not need to impose a minimum length or special characters yet.

## 5. Responsibility of `main()`

`main()` should be limited to:

```text
Validate arguments
    → install signals
    → construct Server
    → run Server
    → catch exceptions
```

It must not take care of:

- Processing IRC commands.
- Creating clients.
- Directly running the `poll()` logic.
- Manually closing `Server`’s internal resources.

## 6. Initial `Server` state

At a minimum, `Server` must store:

```cpp
int _port;
std::string _password;
int _listeningSocketFileDescriptor;
```

The constructor must use an initialization list:

```cpp
Server::Server(
    int port,
    const std::string &password
)
    : _port(port),
      _password(password),
      _listeningSocketFileDescriptor(-1)
{
}
```

The server stores its own copy of the password.

## 7. Descriptors initialized to `-1`

A valid descriptor can be `0`, so you must not use it to represent “no socket”.

```cpp
const int INVALID_FILE_DESCRIPTOR = -1;
```

The usual evolution will be:

```text
Before socket():    -1
After socket():      3
After close():      -1
```

This helps prevent duplicate closes.

## 8. Prevent copying `Server`

Copying a `Server` could make two objects believe they own the same socket.

In C++98 this is prevented by declaring as private:

```cpp
Server(const Server &other);
Server &operator=(const Server &other);
```

They must not be implemented or used.

## 9. Handle signals

You must control:

- `SIGINT`: normally received with `Ctrl+C`.
- `SIGTERM`: requests process termination.
- `SIGPIPE`: must be ignored so a send to a disconnected client does not terminate the whole server.

Installation can be done from:

```cpp
SignalHandler::install();
```

## 10. Use a shutdown flag

The signal handler must only modify a flag:

```cpp
static volatile sig_atomic_t _shutdownRequested;
```

The handler must be minimal:

```cpp
void SignalHandler::handleTerminationSignal(
    int signalNumber
)
{
    static_cast<void>(signalNumber);
    _shutdownRequested = 1;
}
```

It must not:

- Close sockets.
- Throw exceptions.
- Use `std::cout`.
- Run complex logic.

## 11. Temporary execution cycle

Since there is still no listening socket, you can temporarily use:

```cpp
void Server::run()
{
    while (!SignalHandler::isShutdownRequested())
    {
        const int pollResult = ::poll(
            NULL,
            0,
            1000
        );

        if (pollResult == -1 && errno != EINTR)
        {
            throw createSystemError(
                "poll",
                errno
            );
        }
    }
}
```

`poll(NULL, 0, 1000)` waits one second without continuously consuming CPU.

## 12. Handle `EINTR` correctly

When a signal interrupts `poll()`, it may return:

```text
-1
```

with:

```cpp
errno == EINTR
```

This is not a real failure. After the interruption, the loop checks the flag again and terminates.

## 13. Cleanup through the destructor

The `Server` destructor must start cleanup:

```cpp
Server::~Server()
{
    closeAllFileDescriptors();
}
```

It will run automatically:

- When `run()` finishes normally.
- When an exception is thrown after constructing `Server`.
- When the scope where the object was created is left.

You must not call manually:

```cpp
server.~Server();
```

## 14. Close each descriptor only once

Before closing a descriptor, check that it is not `-1`:

```cpp
void Server::closeFileDescriptor(int &fileDescriptor)
{
    if (fileDescriptor == -1)
        return;

    const int descriptorToClose = fileDescriptor;

    fileDescriptor = -1;

    if (::close(descriptorToClose) == -1)
    {
        // Report warning without throwing
    }
}
```

The parameter is a reference because the original attribute is also modified.

The sequence is:

```text
Save the descriptor
    → mark the attribute as invalid
    → call close()
    → do not close it again
```

## 15. Do not throw exceptions from the destructor

If `close()` fails during destruction, show a warning, but do not throw another exception.

An exception thrown while another is already being processed could terminate the program through:

```cpp
std::terminate();
```

## 16. Ownership of descriptors

Define from now on who is responsible for closing them:

| Resource | Owner | Who closes it |
|---|---|---|
| Listening socket | `Server` | `Server` |
| Client socket | `Server` | `Server` |
| `Client` object | `Server` | `Server` |
| `pollfd` entry | Not an owner | Nobody |
| Descriptor stored in `Client` | Only identifies the connection | `Server` |

Removing a `pollfd` from a vector does not close the socket.

The future disconnection sequence will be:

```text
Detect disconnection
    → remove descriptor from poll
    → close descriptor
    → delete Client
```

## 17. Compilation

The project must compile with:

```makefile
CXXFLAGS = -Wall -Wextra -Werror -std=c++98
```

Minimum files:

```makefile
SOURCES = src/main.cpp \
          src/Server.cpp \
          src/SignalHandler.cpp
```

## 18. Required tests

### Incorrect arguments

```bash
./ircserv
./ircserv 6667
./ircserv 6667 password extra
```

### Invalid ports

```bash
./ircserv abc password
./ircserv 12abc password
./ircserv -1 password
./ircserv 0 password
./ircserv 65536 password
```

### Empty password

```bash
./ircserv 6667 ""
```

### Clean termination

```bash
./ircserv 6667 password
```

Then press:

```text
Ctrl+C
```

### Test `SIGTERM`

In one terminal:

```bash
./ircserv 6667 password
```

In another:

```bash
pgrep ircserv
kill -TERM <process_id>
```

### Memory and descriptors

```bash
valgrind \
    --leak-check=full \
    --show-leak-kinds=all \
    --track-fds=yes \
    ./ircserv 6667 password
```

## Phase 1 finished

You can consider this phase complete when:

- Exactly the port and the password are received.
- The port is correctly validated.
- The password is not empty.
- `Server` keeps a valid state.
- Descriptors start at `-1`.
- `Server` cannot be copied.
- `SIGINT` and `SIGTERM` request shutdown.
- `SIGPIPE` is ignored.
- The handler only modifies a flag.
- `run()` does not consume CPU unnecessarily.
- The destructor cleans up the resources.
- No descriptor is closed twice.
- There are no leaks belonging to the program.
- It compiles correctly in C++98.

The next phase will be to create the listening socket:

```text
socket()
    → setsockopt()
    → non-blocking mode
    → bind()
    → listen()
    → add it to poll()
```
