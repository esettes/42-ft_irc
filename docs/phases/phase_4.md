# Phase 4 — Basic client model

## Goal

Create a `Client` class that represents the state of each user connected to the server.

Every time the server accepts a new TCP connection through `accept()`, it must create a `Client` object associated with that connection’s **socket file descriptor**.

In this phase it is still not necessary to fully implement the IRC commands. The goal is to prepare the data model that will later allow managing:

* [x] Receiving and sending information.
* [ ] Password authentication.
* [ ] IRC registration through `NICK` and `USER`.
* [ ] The channels the client belongs to (as in Slack).
* [ ] Safe disconnection and removal of the client.

---

## 1. Create the `Client` class

The class must store at least the following data:

```text
Client
├── socket file descriptor
├── input buffer
├── output buffer
├── nickname
├── username
├── real name
├── password accepted
├── nickname received
├── username received
├── registered
└── channels joined
```

A possible initial declaration would be:

```cpp
class Client
{
    private:
        int _socketFileDescriptor;

        std::string _inputBuffer;
        std::string _outputBuffer;

        std::string _nickname;
        std::string _username;
        std::string _realName;

        bool _passwordAccepted;
        bool _nicknameReceived;
        bool _usernameReceived;
        bool _registered;

        std::set<std::string> _joinedChannels;

    public:
        explicit Client(int socketFileDescriptor);
        ~Client();
};
```

The container used for channels may change later when the `Channel` class is implemented. For now, a `std::set<std::string>` lets you store the names without duplicates.

---

## 2. Store the file descriptor

Each client must know the file descriptor of the socket used to communicate with it:

```cpp
int _socketFileDescriptor;
```

This descriptor lets you identify the connection in:

* The server’s client container.
* The structure used by `poll()`.
* Calls to `recv()`.
* Calls to `send()`.
* Disconnection handling.

A method to query it must be provided:

```cpp
int getSocketFileDescriptor() const;
```

---

## 3. Implement the input buffer

The input buffer stores the bytes received through `recv()`:

```cpp
std::string _inputBuffer;
```

TCP does not guarantee that an IRC command arrives complete in a single call. It may also deliver several commands together.

For example, a client may send:

```text
NICK roxana\r\nUSER roxana 0 * :Roxana\r\n
```

The server might receive that data:

* In a single call to `recv()`.
* Split across several calls.
* Together with later commands.

That is why the data must be appended to the buffer:

```cpp
void appendToInputBuffer(const std::string &receivedData);
```

It will also be necessary to extract only the complete lines ending in `\r\n`:

```cpp
bool extractNextLine(std::string &line);
```

If a complete line does not exist yet, the data must stay stored in the buffer until the next read.

---

## 4. Implement the output buffer

The output buffer stores the replies pending to send:

```cpp
std::string _outputBuffer;
```

You must not assume that `send()` will send every requested byte, especially because the sockets are non-blocking.

Recommended methods:

```cpp
void appendToOutputBuffer(const std::string &message);
const std::string &getOutputBuffer() const;
void removeSentOutput(std::size_t sentByteCount);
bool hasPendingOutput() const;
```

When `send()` manages to send part of the buffer, only the bytes actually sent must be removed.

If data is still pending, the client must keep being watched with the `POLLOUT` event.

---

## 5. Store the IRC identity

The client must store the data received during registration:

```cpp
std::string _nickname;
std::string _username;
std::string _realName;
```

### Nickname

It is received through:

```text
NICK <nickname>
```

The nickname must be valid and cannot be in use by another client.

Recommended methods:

```cpp
const std::string &getNickname() const;
void setNickname(const std::string &nickname);
```

### Username and real name

They are received through:

```text
USER <username> 0 * :<real name>
```

Recommended methods:

```cpp
const std::string &getUsername() const;
void setUsername(const std::string &username);

const std::string &getRealName() const;
void setRealName(const std::string &realName);
```

---

## 6. Represent registration states separately

IRC registration depends on several conditions. That is why it is not a good idea to represent it with a single state such as:

```cpp
bool _authenticated;
```

Instead, they must be stored separately:

```cpp
bool _passwordAccepted;
bool _nicknameReceived;
bool _usernameReceived;
bool _registered;
```

Each state has a concrete responsibility:

| State               | Meaning                                                        |
| ------------------- | -------------------------------------------------------------- |
| `_passwordAccepted` | The client has sent the correct password through `PASS`.       |
| `_nicknameReceived` | The server has accepted a valid and available nickname.        |
| `_usernameReceived` | The server has correctly received the `USER` command.          |
| `_registered`       | The client has completed every IRC registration condition.     |

Recommended methods:

```cpp
bool isPasswordAccepted() const;
void setPasswordAccepted(bool accepted);

bool hasReceivedNickname() const;
void setNicknameReceived(bool received);

bool hasReceivedUsername() const;
void setUsernameReceived(bool received);

bool isRegistered() const;
```

---

## 7. Check when registration is complete

A client will be registered when these conditions are met at the same time:

```text
correct PASS
    +
valid and available NICK
    +
USER received
```

The condition can be represented like this:

```cpp
_passwordAccepted
    && _nicknameReceived
    && _usernameReceived
```

The check must be performed after processing each registration-related command:

* `PASS`
* `NICK`
* `USER`

This is necessary because `NICK` and `USER` may be received in a different order.

For example, both orders are possible:

```text
PASS secret
NICK roxana
USER roxana 0 * :Roxana
```

```text
PASS secret
USER roxana 0 * :Roxana
NICK roxana
```

A method such as this can be added:

```cpp
bool Client::canBeRegistered() const
{
    return _passwordAccepted
        && _nicknameReceived
        && _usernameReceived;
}
```

The server will be able to use it to complete registration:

```cpp
if (!client.isRegistered() && client.canBeRegistered())
{
    client.markAsRegistered();
}
```

It is important to check `isRegistered()` first so registration is not completed and the welcome message is not sent several times.

Recommended method:

```cpp
void markAsRegistered();
```

---

## 8. Initialize the client correctly

The constructor must receive the connection file descriptor and initialize every state:

```cpp
Client::Client(int socketFileDescriptor)
    : _socketFileDescriptor(socketFileDescriptor),
      _inputBuffer(),
      _outputBuffer(),
      _nickname(),
      _username(),
      _realName(),
      _passwordAccepted(false),
      _nicknameReceived(false),
      _usernameReceived(false),
      _registered(false),
      _joinedChannels()
{
}
```

When a client is created:

* It has an active TCP connection.
* It has not accepted the password yet.
* It has not sent a valid nickname yet.
* It has not sent `USER` yet.
* It is not registered yet.
* It does not belong to any channel yet.

---

## 9. Store the client’s channels

The client must know which channels it belongs to:

```cpp
std::set<std::string> _joinedChannels;
```

Recommended methods:

```cpp
void joinChannel(const std::string &channelName);
void leaveChannel(const std::string &channelName);
bool isInChannel(const std::string &channelName) const;
const std::set<std::string> &getJoinedChannels() const;
```

In this phase it is enough to prepare the structure. The full logic of `JOIN`, `PART` and the `Channel` class will be implemented later.

---

## 10. Create a client when accepting a connection

After `accept()` returns a new file descriptor, the server must:

1. Configure the new socket as non-blocking.
2. Create the `Client` object.
3. Store it in the client container.
4. Add its file descriptor to the elements watched by `poll()`.

A usual structure in `Server` is:

```cpp
std::map<int, Client *> _clients;
```

The file descriptor works as the key:

```cpp
Client *newClient = new Client(clientSocketFileDescriptor);
_clients[clientSocketFileDescriptor] = newClient;
```

If the project allows `Client` to be copyable and its destructor to be accessible, storing objects directly can also be considered:

```cpp
std::map<int, Client> _clients;
```

The decision must keep ownership clear: the server is responsible for every client it contains and must free them when they disconnect.

---

## 11. Remove a client correctly

When a client disconnects or a fatal error occurs, the server must:

1. Remove it from the elements watched by `poll()`.
2. Remove it from every channel.
3. Close its socket.
4. Delete its `Client` object.
5. Erase it from the client container.

If pointers are used:

```cpp
std::map<int, Client *>::iterator clientIterator =
    _clients.find(socketFileDescriptor);

if (clientIterator != _clients.end())
{
    delete clientIterator->second;
    _clients.erase(clientIterator);
}
```

It must be decided clearly who closes the socket:

* The `Client` destructor.
* Or the `Server` disconnection method.

Both must not do it, because that would produce a duplicate close of the same file descriptor.

---

## 12. Responsibilities of each class

### `Client`

Must take care of storing and modifying:

* The file descriptor.
* The input and output buffers.
* The IRC identity.
* Registration progress.
* The channels it belongs to.

### `Server`

Must take care of:

* Accepting connections.
* Creating and storing clients.
* Checking that nicknames are not taken.
* Processing received commands.
* Completing registration.
* Adding or removing clients from channels.
* Disconnecting and deleting clients.

The `Client` class represents the state of a connection, but it must not know or control the whole server.

---

## Expected result

At the end of this phase:

* Each accepted connection has its own `Client` object.
* Each client is associated with a file descriptor.
* Received data can be accumulated in an input buffer.
* Pending replies can be kept in an output buffer.
* The client stores nickname, username and real name.
* Registration progress is represented through separate states.
* Registration can be completed independently of the order of `NICK` and `USER`.
* There is a structure to remember the client’s channels.
* The server can correctly remove a disconnected client.

In this phase it is still not necessary to fully implement `PASS`, `NICK`, `USER`, `JOIN` or `PART`. The model those commands will run on must be ready.
