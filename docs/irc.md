An IRC server needs to make two distinct layers work:

- The TCP network layer: accept and keep connections.
- The IRC layer: interpret commands and manage users and channels.

## 1. Starting the server

The executable is normally started like this:

```bash
./ircserv <port> <password>
```

For example:

```bash
./ircserv 6667 secret
```

The server must:

- Validate the port.
- Store the password.
- Create the main socket.
- Bind it to the port.
- Start listening for connections.
- Enter an event loop.

## 2. Creating the listening socket

The server needs a main TCP socket:

```cpp
int serverSocketFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
```

Then it must:

- Configure the socket as non-blocking.
- Associate it with an address and a port through bind().
- Put it into listening mode through listen().

Conceptually:

`socket() → fcntl() → bind() → listen()`

This socket does not represent a client. Its only job is to receive new connections.

## 3. Waiting for activity with poll()

The server cannot stay blocked serving a single client. It must be able to manage several clients at the same time.

That is what `poll()` is for:

```cpp
poll(socketDescriptors, descriptorCount, -1);
```

`poll()` reports which sockets have activity:

- The main socket has activity: there is a new connection.
- A client socket has activity: that client has sent data.
- A socket has an error or has been closed: the client must be disconnected.
- A client can receive data: there are pending messages to send.

The overall structure:

```text
while (server is running)
    poll()
    accept new connections
    read client data
    process complete IRC commands
    send pending responses
    disconnect invalid or closed clients
```

## 4. Accepting clients

When the main socket has activity, the server calls:

```cpp
int clientSocketFileDescriptor = accept(...);
```

The returned value identifies the specific connection with that client.

For example:

```text
Socket 3 → server listening socket
Socket 4 → first client
Socket 5 → second client
Socket 6 → third client
```

Each client needs its own state:

```cpp
class Client
{
private:
    int socketFileDescriptor;
    std::string nickname;
    std::string username;
    std::string inputBuffer;
    std::string outputBuffer;
    bool passwordAccepted;
    bool registered;
};
```

The descriptor identifies the connection, but it does not contain all of the user’s IRC information. That is why you need a `Client` object.

## 5. Receiving data

When `poll()` reports that a client has sent something, `recv()` is used:

```cpp
char buffer[4096];

ssize_t receivedBytes = recv(
    clientSocketFileDescriptor,
    buffer,
    sizeof(buffer),
    0
);
```

But there is a fundamental point: TCP does not preserve message boundaries.

The client may send:

`NICK roxana\r\nUSER roxana 0 * :Roxana\r\n`

And `recv()` might deliver it in different ways:

```text
First recv:   NICK ro
Second recv:  xana\r\nUSER rox
Third recv:   ana 0 * :Roxana\r\n
```

That is why each client needs an `inputBuffer`.

Received data is appended:

```cpp
client.getInputBuffer().append(buffer, receivedBytes);
```

An instruction is processed only when the IRC terminator appears:

`\r\n`

## 6. Parsing IRC commands

When you have a complete line:

`PRIVMSG #general :Hello everyone\r\n`

The parser must split it into something similar to:

```text
Command: PRIVMSG
Parameters:
    #general
    Hello everyone
```

A useful structure could be:

```cpp
class Message
{
public:
    std::string prefix;
    std::string command;
    std::vector<std::string> parameters;
};
```

The last parameter may start with `:` and contain spaces:

`PRIVMSG #general :This message contains spaces`

Here `This message contains spaces` is a single parameter.

## 7. Registering the client

A TCP connection does not automatically turn the client into a registered IRC user.

Normally the client must send:

```text
PASS secret
NICK roxana
USER roxana 0 * :Roxana
```

The server must verify:

- That the password is correct.
- That the nickname is valid.
- That the nickname is not already taken.
- That it has received NICK.
- That it has received USER.

When all conditions are met:

```cpp
client.setRegistered(true);
```

Then the welcome message is sent, normally with IRC numeric replies:

`:irc.local 001 roxana :Welcome to the IRC Network roxana`

That is why Irssi may show:

```text
Connection established
Not connected to server
```

The TCP connection has been established, but IRC registration has not completed correctly.

## 8. Executing commands

The server needs to associate each command with its behaviour.

At a minimum:

- `PASS`: check the password.
- `NICK`: assign or change the nickname.
- `USER`: store the user data.
- `PING/PONG`: keep the connection alive.
- `QUIT`: disconnect the user.
- `JOIN`: enter a channel.
- `PART`: leave a channel.
- `PRIVMSG`: send messages.
- `KICK`: remove a user.
- `INVITE`: invite a user.
- `TOPIC`: query or change the topic.
- `MODE`: configure channel modes.

A dispatcher can make that decision:

```cpp
if (message.getCommand() == "NICK")
    handleNick(client, message);
else if (message.getCommand() == "JOIN")
    handleJoin(client, message);
else if (message.getCommand() == "PRIVMSG")
    handlePrivmsg(client, message);
```

Later that if-chain can be replaced by a map of functions.

## 9. Managing channels

The server needs to store the existing channels:

```cpp
class Channel
{
private:
    std::string name;
    std::string topic;
    std::set<Client *> members;
    std::set<Client *> operators;
    std::set<Client *> invitedClients;
    std::string key;
    std::size_t userLimit;
    bool inviteOnly;
    bool topicRestricted;
};
```

When someone runs:

`JOIN #general`

The server must:

- Look up the channel.
- Create it if it does not exist.
- Check the password, invitation, and limit.
- Add the client.
- Inform the members.
- Send the topic.
- Send the user list.

## 10. Sending messages

Replies are sent with `send()`:

```cpp
send(
    clientSocketFileDescriptor,
    response.c_str(),
    response.size(),
    0
);
```

However, `send()` may send only part of the message. That is why each client should have an `outputBuffer`.

Example:

```text
Pending message: 120 bytes
send() sends:     70 bytes
Remaining:        50 bytes
```

Those 50 bytes must be kept so they can be sent later.

All IRC messages must end with:

`\r\n`

For example:

```cpp
std::string response =
    ":irc.local 001 roxana :Welcome to the IRC Network\r\n";
```

## 11. Numeric replies and errors

IRC uses numeric codes for many replies:

```text
001 → welcome
331 → the channel has no topic
332 → channel topic
353 → user list
366 → end of the user list
401 → no such nickname
403 → no such channel
431 → nickname missing
433 → nickname already in use
461 → missing parameters
464 → incorrect password
```

The server must build them with the correct format. It is not enough to send simply:

`Error: nickname already in use`

Irssi expects replies that are compatible with the IRC protocol.

## 12. Disconnection and cleanup

A client may disconnect because:

- It runs QUIT.
- It closes Irssi.
- `recv()` returns 0.
- A socket error occurs.
- The server rejects the password.
- Its connection is no longer valid.

When disconnecting it, the server must:

- Notify the affected users.
- Remove it from every channel.
- Delete empty channels when appropriate.
- Remove its descriptor from poll().
- Close the socket.
- Destroy the Client object.

It is important not to leave pointers to the client inside any channel.
