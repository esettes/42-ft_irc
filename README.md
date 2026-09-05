*This project has been created as part of the 42 curriculum by danfern3, rstancu*

# **Description**
The project consists of developing an IRC server using C++ 98 standard. This means neither developing and IRC client nor implementing a server-to-server communication.

## **Restrictions**

### Subject
|Program Name|Files to Submit|Makefile|Arguments|External Functions|
|:---:|:---:|:---:|:---:|:---:|
|ircserv|Makefile, \*.{h.hpp},\*.cpp, \*.tpp, \*.ipp, an optional configuration file|NAME, all, clean, fclean, re|port (*listening port*), password(*connection password*)|socket, close, setsockopt, getsockname, getprotobyname, gethostbyname, getaddrinfo, freeaddrinfo, bind, connect, listen, accept, htons, htonl, ntohs, ntohl, inet_addr, inet_ntoa, inet_ntop, send, recv, signal, sigaction, sigemptyset, sigfillset, sigaddset, sigdelset, sigismember, lseek, fstat, fcntl, poll (or equivalent)|

# **Instructions**
## Makefile rules
Run `make help` to know which rules are availabe in this project

There will be listed rules like: `all`, `clean`, `fclean`, and `re`

## Installation
Clone the repository: `git clone https://github.com/esettes/42-ft_irc && cd 42-ft_irc`

## Compilation
Run `make || make all || make ircserv`

## Execution
Execute `make run` to start the server in port `6667` with password `secret` 

# **Resources**
- [Visual guide](https://deepwiki.com/42YerevanProjects/ft_irc) to get a general understanding on how to organize the project.
- [CPP linter](https://github.com/cpplint/cpplint) to follow some of Google's CPP code style. Note: install cpplint via `pipx install cpplint`.
- IRC client reference [irssi](https://irssi.org/New-users/).

## Use of AI
AI has been mainly used to automate the Makefile testing rules, as well as writing documentation such [architecture](/docs/architecture.md), or [testing](/docs/testing.md).

Also AI has been used to explain us how to use our IRC client reference, which is `irssi`:

### After executing `irssi` in the terminal:

#### How to connect to the server
```
/connect 127.0.0.1 6667 secret
```

#### How to join channels
```
/join #general
```

> Once inside a channel you can send messges without following any specific format

#### Invite users
```
/invite marvin #general
```

#### Kick users
```
/kick marvin reason
```

#### See the `irssi` guide linked in the [resources](#resources) section

# **Requirements**

- The server must be capable of handling multiple clients simultaneously without hanging.
- Forking is prohibited. All I/O operations must be **non-blocking**.
- Only 1 *poll()* (or equivalent) can be used for handling all these operations (read, write, but also listen, and so forth).

>

- Several IRC clients exist. You have to choose one of them as **reference**. Your reference client will be used during the evaluation process.
- Your reference client must be able to connect to your server without encountering any error.
- Using your reference client with your server must be similar to using it with any official IRC server. However, you only have to implement the following features:
    - You must be able to authenticate, set a nickname, a username, join a channel, send and receive private messages using your reference client.
    - All the messages sent from one client to a channel have to be forwarded to every other client that joined the channel.
    - You must have **operators** and regular users.
    - Then, you have to implement the commands that are specific to **channel operators**:
        - KICK - Eject a client from the channel.
        - INVITE - Invite a client to a channel.
        - TOPIC - Change or view the channel topic.
        - MODE - Change the channel's mode:
            - i: Set/remove Invite-only channel.
            - t: Set/remove the restrictions of the TOPIC command to channel operators.
            - k: Set/remove the channel key (password).
            - o: Give/take channel operator privilege.
            - l: Set/remove the user limit to channel.
- Of course, you are expected to write a clean code.

# **Test example**
Verify every possible error and issue, such as receiving partial data, low bandwidth, etc.

<br>

To ensure that your server correctly processes all data sent to it, the following simple test using **nc** can be performed:

```
\$> nc -C 127.0.0.1 6667
com^Dman^Dd
\$>
```

<br>

Use **ctrl+D** to send the command in several parts: '**com**', then '**man**', then '**d\n**'.

<br>

In order to process a command, you have to first aggregate the received packets in order to rebuild it.

# **Bonus part**

- Handle file transfer
- A bot.
