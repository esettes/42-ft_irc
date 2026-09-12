The IRC server will roughly follow this process:

```text
socket()
   ↓
bind()
   ↓
listen()
   ↓
poll()
   ↓
accept()
   ↓
recv() / send()
   ↓
close()
```

`socket()`

Creates the server’s main socket:

```cpp
socket(AF_INET, SOCK_STREAM, 0);
```

- `AF_INET`: use IPv4 addresses.
- `SOCK_STREAM`: use TCP.
- `0`: automatically select the corresponding protocol.

`bind()`

Associates the socket with an address and a port:

`0.0.0.0:6667`

It is like saying:

This program will be responsible for connections that arrive on port 6667.

`listen()`

Puts the socket into listening mode. From that moment it can receive connection requests.

`accept()`

Accepts a pending connection.

Very important: `accept()` creates a new socket for that client.

```text
Server socket
    │
    ├── client 1 socket
    ├── client 2 socket
    └── client 3 socket
```

The main socket keeps listening. The sockets created by `accept()` are used to talk to each client.

`recv()`

Receives bytes sent by a client.

The data may represent complete IRC commands, several commands, or only a fragment.

`send()`

Sends bytes to the client.

For example:

`:server 001 roxana :Welcome to the IRC server\r\n`

`close()`

Closes a socket when the client disconnects or an error occurs.

---

For TCP everything is bytes:

`4e 49 43 4b 20 72 6f 78 61 6e 61 0d 0a`

It is your server that interprets those bytes as:

`NICK roxana\r\n`

It also does not encrypt the information. A password sent over plain TCP is not encrypted; TLS would be needed for that.

---

TCP creates a reliable, ordered byte stream between Irssi and `ft_irc`. The IRC server must turn that byte stream into complete IRC commands and reply to them correctly.
