# 42 ft_irc project evaluation

> [!info] Notes and questions to explain the project during evaluation. The
> reference client is **irssi**. Run `./ircserv <port> <password>` (example:
> `./ircserv 6667 secret`). Tests: `make test`.

## How the pieces fit together

```text
TCP socket  →  Client (inputBuffer)  →  MessageParser  →  CommandDispatcher
                                                                    ↓
                                                              Server / Channel
                                                                    ↓
                                                         Client (outputBuffer)
                                                                    ↓
                                                              poll() + send()
```

The server is a single process. There is **no fork**, **no extra threads**, and
**one `poll()`** that watches the listening socket and every client socket.
All I/O is **non-blocking**. IRC commands are reconstructed from TCP fragments
in `Client::inputBuffer` before they are parsed.

Registration requires `PASS` (correct password) + `NICK` + `USER`. After that
the client can `JOIN` channels, send `PRIVMSG`/`NOTICE`, and channel operators
can use `KICK`, `INVITE`, `TOPIC` and `MODE` (`i`, `t`, `k`, `o`, `l`).

Bonus: a virtual bot (`marvin` on `#bot`) and DCC file transfer by forwarding
CTCP `PRIVMSG` unchanged so clients open a direct TCP connection.

---

## Data structures and key functionalities

### Server.hpp

Owns the whole process: TCP listen socket, the event loop, clients, channels,
command dispatch and the bot.

**Data structures**

| Member | Type | Role |
|---|---|---|
| `pollFds` | `std::vector<struct pollfd>` | Unique `poll()` set. Index `0` is the listening socket; the rest are clients. |
| `clients` | `std::map<int, Client *>` | Socket fd → `Client *`. Fast lookup when `poll()` reports activity. Server owns the pointer. |
| `clientsByNickname` | `std::map<std::string, Client *>` | **RFC 1459-normalized** nick → `Client *`. Duplicate nick detection and `PRIVMSG`/`INVITE`/`KICK` lookup. |
| `channels` | `std::map<std::string, Channel>` | Normalized channel name → `Channel` **by value**. Display spelling is stored on the object. |
| `dispatcher` | `CommandDispatcher` | Routes parsed commands to handlers. |
| `bot` | `Bot *` | Built-in helper; created after the listen socket is ready. |

Two client maps are needed because `poll()` identifies connections by **fd**,
while IRC identifies users by **nickname**. Channel keys are normalized so
`#General` and `#general` are the same channel.

**Key functionalities**

- `createListeningSocket()`: `socket()` → `fcntl(O_NONBLOCK)` →
  `setsockopt(SO_REUSEADDR)` → `bind()` → `listen()`.
- `run()`: loop `poll()` until `SignalHandler` requests shutdown. On `POLLIN`
  of fd `0`, `acceptClient()`. On client `POLLIN`, `recv` into the input
  buffer. On `POLLOUT`, `flushClientOutput()`. Disconnects are processed in a
  second pass so iterators stay valid.
- `acceptClient()`: non-blocking `accept()`. If the process is out of fds,
  `pauseAcceptingConnections()` (clears `POLLIN` on the listen socket) so
  `poll()` keeps serving existing clients. `disconnectClient()` calls
  `resumeAcceptingConnections()`.
- `processClientBuffer()`: extract complete lines, `MessageParser::parse()`,
  `dispatchCommand()`, then `tryRegisterClient()`.
- `queueMessage()`: append to `outputBuffer`, enable `POLLOUT`. Never calls
  `send()` from a command handler. Virtual clients (the bot) are skipped.
- `flushClientOutput()`: `send()` at most `MAX_SEND_SIZE` (16384) bytes,
  then `removeSentOutput()`. `EAGAIN` keeps the remaining bytes.
- `assignNickname()`: the only place that updates `Client::nickname`,
  `nicknameReceived` and `clientsByNickname` together. If the new nick is
  taken, the old nick is left unchanged.
- `addClientToChannel()`: first member of an empty channel becomes operator.
- `validateChannelJoinAccess()`: checks `+l`, then `+i`, then `+k`.
- `disconnectClient()`: broadcasts one `QUIT` to related clients, detaches
  from channels, frees the nick, removes the poll entry, closes the fd,
  deletes the `Client`.
- Limits: input buffer 8192 bytes, output buffer 65536 bytes, IRC line 512
  bytes including `CR-LF`. Overflow requests a deferred disconnect.

### Client.hpp

One object per connection (or virtual identity). Owns buffers and registration
state. Does **not** own the socket lifetime: `Server` closes the fd.

**Data structures**

| Member | Type | Role |
|---|---|---|
| `socketFd` | `int` | Real client fd, or `< 0` for the bot (`isVirtual()`). |
| `inputBuffer` | `std::string` | Aggregates TCP fragments until a full IRC line exists. |
| `outputBuffer` | `std::string` | Pending bytes that `send()` has not fully written. |
| `nickname` / `username` / `realname` / `host` | `std::string` | Identity. Original nick casing is kept for display. |
| `passwordAccepted` / `nicknameReceived` / `usernameReceived` / `registered` | `bool` | Registration machine. Ready when the first three are true. |
| `joinedChannels` | `std::set<std::string>` | Normalized channel names this client is in. |
| `disconnectRequested` / `disconnectReason` | `bool` / `std::string` | Deferred close. First reason wins. |

`LineReadStatus`: `LINE_INCOMPLETE`, `LINE_COMPLETE`, `LINE_TOO_LONG`.

**Key functionalities**

- `extractNextLine()`: a line ends with `\n`, optional `\r` before it. If the
  pending fragment already exceeds 511 bytes without a newline, the buffer is
  cleared (`LINE_TOO_LONG`) so the same junk cannot loop forever. This is the
  subject’s “`com` + `Ctrl+D` + `man` + `Ctrl+D` + `d`” test: three TCP writes
  become one command only when `\n` arrives.
- `isReadyToRegister()`: `PASS` + `NICK` + `USER` completed, in any order.
- `requestDisconnect()`: does **not** close the socket. The event loop calls
  `disconnectClient()` later so a handler never destroys the object it is
  running on.
- Copy constructor / assignment are private on the public API side of most
  types; `Client` still implements them because it is a concrete object, but
  `Server` stores pointers so identity is stable in `Channel` sets.

### Channel.hpp

Channel state, membership and operator-specific modes.

**Data structures**

| Member | Type | Role |
|---|---|---|
| `name` | `std::string` | Display name (`#General`), not the map key. |
| `topic` | `std::string` | Empty means no topic (`RPL_NOTOPIC`). |
| `members` | `std::set<Client *>` | Who is on the channel. Pointers, not copies. |
| `operators` | `std::set<Client *>` | Per-channel ops (`MODE +o`). Subset of members. |
| `invitedClients` | `std::set<Client *>` | Pending invitations for `+i`. Consumed on `JOIN`. |
| `inviteOnly` / `topicRestricted` / `keyEnabled` / `limitEnabled` | `bool` | Modes `i`, `t`, `k`, `l`. |
| `channelKey` | `std::string` | Password when `+k`. |
| `userLimit` | `std::size_t` | Max members when `+l`. `0` means unset. |
| `messageHistory` | `std::vector<std::string>` | Serialized channel `PRIVMSG` replayed to later `JOIN`. |

Valid names: start with `#`, length 2–50, no space/control/`,`/`:`.

**Key functionalities**

- `std::set` gives uniqueness and O(log n) membership tests. `removeMember()`
  also drops operator status and any leftover invitation.
- `inviteClient()` stores a **pending** invite, not a permanent privilege. It
  is removed on successful join, on leave, or when the client disconnects
  (`detachClientFromChannels`).
- Default modes: all flags **off**. Anyone in the channel can change the topic
  until `MODE +t`. The first joiner is made operator by `Server`, not by
  `Channel` itself.
- Empty channels are erased from `Server::channels` so they do not leak.

### CommandDispatcher.hpp

Command router. One `std::map` from uppercase command name to a member-function
pointer, minimum parameter count, and whether the client must already be
registered.

**Data structures**

| Member / type | Role |
|---|---|
| `CommandHandler` | `void (CommandDispatcher::*)(Client &, const IrcMessage &)` |
| `CommandDefinition` | `{ handler, minParams, requiresRegistration }` |
| `cmmds` | `std::map<std::string, CommandDefinition>` |
| `ModeOperation` | Parsed `MODE` step: add/remove, flag letter, optional argument |

Commands that do **not** require registration: `PASS`, `NICK`, `USER`, `PING`,
`PONG`, `QUIT`, `CAP`. The rest (`JOIN`, `PART`, `PRIVMSG`, `NOTICE`, `TOPIC`,
`INVITE`, `KICK`, `MODE`) need a completed welcome.

**Key functionalities**

- `execute()`: unknown command → `421`; not registered → `451`; too few params
  → `461`; then `(this->*handler)(client, message)`.
- `PASS` / `USER` refuse a second registration (`462`). Wrong password → `464`.
- `NICK`: validate, then `assignNickname()`. After registration, a real nick
  change is broadcast with the **previous** prefix
  (`oldnick!user@host NICK :newnick`).
- `CAP`: irssi sends `CAP LS` on connect. The server answers with an empty
  list and `NAK`s `CAP REQ`, and must **not** disconnect.
- `JOIN` / `PART`: comma-separated lists; keys are paired by index.
- `PRIVMSG`: if target starts with `#`, send to channel members except the
  sender and store history; otherwise send to one nick. Trailing text is not
  split, so CTCP/`DCC SEND` survives. `PRIVMSG` to `marvin` then calls the bot.
- `NOTICE`: same routing, **no** error numerics (RFC 2812).
- Operator commands:
  - `KICK`: op only; target stays connected; default reason is the op nick.
  - `INVITE`: op only; `RPL_INVITING` to the op, `INVITE` to the target.
  - `TOPIC`: query always allowed for members; set requires op when `+t`.
  - `MODE`: query (`MODE #chan`) needs no op (`324`). Changes need op.
    `+o`/`-o` always take a nick; `+k` and `+l` take an argument; `-k`/`-l`
    do not. Unknown letters → `472`. Validation of the **whole** list happens
    before any flag is applied.

### IrcMessage.hpp

Parsed or outbound IRC line.

**Data structures**

| Field | Type | Role |
|---|---|---|
| `prefix` | `std::string` | Origin, without the leading `:`. Example: `nick!user@host`. |
| `cmd` | `std::string` | Command or numeric, stored uppercase on parse. |
| `params` | `std::vector<std::string>` | Middle params, then optional trailing. |
| `hasTrailingParameter` | `bool` | Last param was introduced by `:`, even if empty. |

Constants: max message **512** bytes including `CR-LF`; max input buffer
**8192**; max output buffer **65536**.

**Key functionalities**

- `serialize()`: `:prefix CMD param1 param2 :trailing\r\n`. The last parameter
  is written with `:` when it is empty, contains a space, starts with `:`, or
  `hasTrailingParameter` is set. Rejects embedded `NUL`/`CR`/`LF`. Throws if
  the result exceeds 512 bytes.

### MessageParser.hpp

Stateless helper: `static IrcMessage parse(const std::string &line)`.

**Key functionalities**

- Rejects `NUL`, `CR` and `LF` inside the line (terminators were already
  stripped by `extractNextLine()`).
- Optional `:prefix`, then command (forced to ASCII uppercase), then space-
  separated params. A param starting with `:` consumes the **rest of the
  line**, including spaces and `\x01` (needed for CTCP/DCC).
- Empty lines are not parsed (`processClientBuffer` skips them).

### IrcCasemap.hpp

RFC 1459 casemapping. Display keeps original case; maps and equality use the
normalized form.

`[]\` and `{}|` are case-equivalent; `~` maps to `^`. So nicks `Nick[A]` and
`nick{a}` collide. Advertised to clients as `CASEMAPPING=rfc1459` in
`RPL_ISUPPORT` (`005`).

Functions: `normalize()`, `equal()`.

### NumericReplies.hpp

IRC numeric codes as `int` (welcome `001`–`005`, channel `324`/`331`/`332`/
`341`/`353`/`366`, errors `401`–`482`), plus `formatCode()` which prints them
as three digits (`1` → `"001"`). Helper strings match the RFC wording
(`Welcome to the IRC Network …`, `You're not channel operator`, …).

`Server::buildNumericReply()` / `queueNumericReply()` turn these into:

```text
:irc.42.local 001 nick :Welcome to the IRC Network nick!user@host
```

### Irc.hpp

Protocol and runtime constants used across the server: port range `1`–`65535`,
command names, mode letters `itkol`, `POLL_TIMEOUT_MS = 1000`, receive chunk
`4096`, `MAX_SEND_SIZE = 16384`, channel name max `50`, default server name
`irc.42.local`, bot help strings. `BotConstants` lists bot commands (`help`,
`ping`, `time`, `dice`, …).

### Bot.hpp

Bonus helper implemented as a **virtual** `Client` (`fd = -1`). It is **not**
in `pollFds`, so it does not consume a descriptor.

**Data structures**

| Member | Type | Role |
|---|---|---|
| `server` | `Server &` | Uses the same channel/nick maps as real users. |
| `user` | `Client *` | Virtual identity; nick reserved as `marvin` at startup. |
| `startedAt` | `std::time_t` | Uptime / `srand` seed. |

Identity: nick `marvin`, user `bot`, host `bot.local`, home channel `#bot`
(bot is operator there).

**Key functionalities**

- `start()` runs before any TCP accept, so nobody can steal `marvin`.
- Direct `PRIVMSG marvin …` or, in a channel the bot is in, `!cmd` or
  `marvin: cmd`. `NOTICE` does not trigger a reply.
- `INVITE` makes the bot `JOIN`. `KICK` just removes it; it stays in `#bot`.
- Commands: `help`, `ping`, `time`, `date`, `info`, `uptime`, `version`,
  `users`, `whoami`, `echo`, `dice`/`roll`. CTCP `VERSION`/`PING`/`TIME`
  answered with `NOTICE`.
- Replies are queued to **real** clients’ output buffers; the bot has none.

### Console.hpp

ANSI colours for server logs: `RESET`, `SERVER` (cyan), `CLIENT` (green),
`WARNING` (yellow), `ERROR` (red). Defined as `const char[]` in the `.cpp`.

Why `const char[]` instead of `std::string`: no heap, no static constructor
order issues, and they are C string literals meant to be streamed as-is.

### SignalHandler.hpp

Process-wide shutdown flag.

**Data structures**

| Member | Type | Role |
|---|---|---|
| `shutdownRequested` | `volatile sig_atomic_t` | Set from the signal handler only. |

**Key functionalities**

- `runSignalHandler()`: `sigaction` for `SIGINT` and `SIGTERM` → set the flag.
  `SIGPIPE` is ignored so a write to a closed socket does not kill the process
  (`EPIPE` is handled in `flushClientOutput()`).
- `run()` checks the flag each loop iteration, then `Server::~Server()` closes
  every fd through `closeAllFds()`.

---

## Subject checklist (what evaluators usually test)

| Requirement | How this project does it |
|---|---|
| Multiple clients, no hang | Single `poll()` + non-blocking `recv`/`send`/`accept` |
| No fork; all I/O non-blocking | `fcntl(O_NONBLOCK)` on listen and client sockets |
| Only one `poll()` (or equivalent) | `Server::pollFds` is the only poll set |
| Authenticate, nick, user | `PASS` + `NICK` + `USER` → `001`–`005` |
| Join a channel, private messages | `JOIN`, `PRIVMSG #chan`, `PRIVMSG nick` |
| Channel messages forwarded | `sendMessageToChannel` queues to every member except the sender |
| Operators and regular users | First joiner is op; `MODE +o` / `-o` |
| `KICK` / `INVITE` / `TOPIC` / `MODE i,t,k,o,l` | `CommandDispatcher` handlers |
| Partial TCP commands | `inputBuffer` + `extractNextLine()` |
| Reference client | **irssi** (`/connect 127.0.0.1 6667 secret`) |
| File transfer (bonus) | CTCP `DCC SEND`/`CHAT` forwarded as-is; file bytes never enter `ircserv` |
| Bot (bonus) | Virtual user `marvin` |

---

## Questions

Group these by topic. Prefer explaining *why* the structure exists, not only
*what* the function is named.

### Network and event loop

1. Why is `fork()` forbidden, and why would one thread per client also fail
   the subject?
2. Why must every socket be non-blocking? What happens if `recv()` or
   `send()` blocks on one client?
3. Why is there a single `poll()` instead of one per client? What does index
   `0` in `pollFds` represent?
4. When is `POLLOUT` enabled, and why is it not always set?
5. How do you rebuild a command that arrives in several TCP packets? Walk
   through `com` / `Ctrl+D` / `man` / `Ctrl+D` / `d` + Enter.
6. What is a slow client? What limits `outputBuffer`, and what happens when
   the limit is hit?
7. Why ignore `SIGPIPE`? What would happen without that?
8. Why is disconnection *requested* instead of closing the socket inside
   `handleQuit()`?
9. If `accept()` fails because there are no file descriptors left, why disable
   `POLLIN` on the listening socket instead of exiting?
10. Why does `run()` use a timeout of 1000 ms instead of `-1`?

### Data structures

11. Why two maps for clients (`fd → Client *` and `nick → Client *`)?
12. Why store `Channel` **by value** in `channels` but `Client` **by pointer**
    in `clients` and in `std::set<Client *>`?
13. Why are channel and nickname map keys normalized with RFC 1459 instead of
    ASCII `tolower`? Give an example of two nicks that must collide.
14. Why does `Client` keep `joinedChannels` *and* `Channel` keep `members`?
    What must stay in sync on `JOIN` / `PART` / `QUIT`?
15. Why is an invitation a pending entry in a `std::set`, not a permanent
    flag on the `Client`?
16. Why does the first member of a new channel become operator? What happens
    when the last member leaves?
17. Why member-function pointers in `CommandDispatcher` instead of a long
    `if/else` on the command string?
18. Why is `hasTrailingParameter` a separate bool, not just “last param
    contains a space”?
19. Why `const char[]` for console colours instead of `std::string`?

### Protocol and commands

20. In which order can a client send `PASS`, `NICK` and `USER`? When is `001`
    sent?
21. Why implement `CAP` if the subject does not list it? What does irssi send
    on connect?
22. How do you tell a channel target from a nick in `PRIVMSG`?
23. Why must `PRIVMSG` trailing text not be split on spaces? How does that
    relate to DCC?
24. Why does `NOTICE` not send `401` / `404` when the target is missing?
25. Walk through `MODE #chan +k secret +l 5 -i`. Which flags take arguments?
    Why is the whole list validated before any change is applied?
26. Who can change the topic when `+t` is off? When it is on?
27. Does `INVITE` bypass `+k` and `+l`, or only `+i`?
28. Does `KICK` disconnect the target from the server?
29. What numeric is `001` vs `421` vs `451` vs `464` vs `482`?
30. How is a nick change announced after registration, and why is the
    **old** prefix used?

### Bonus

31. Why is the bot a virtual client with `fd < 0` instead of a second process
    or a real TCP connection to itself?
32. How do you talk to the bot in a channel vs in private?
33. In DCC, does the file pass through `ircserv`? Describe the three steps
    (`PRIVMSG` CTCP, receiver connects to sender, bytes on a separate TCP
    socket).
34. Why must the parser keep `\x01` and the rest of the trailing parameter
    intact?

### Practical / irssi

35. How do you start the server and connect with irssi?
36. How do you show that two clients in `#general` see each other’s messages,
    and that a third client joining later still sees history?
37. How do you demonstrate `MODE +i`, `INVITE`, then `JOIN`?
38. What automatic tests exist (`make test`) and which suite covers framing /
    slow clients / DCC / the bot?

### Design trade-offs (good if the evaluator goes deeper)

39. `Client::isOperator` exists but channel ops live on `Channel`. What is
    actually used?
40. The comment on nicknames mentions a 9-character RFC limit. Is that
    enforced in `isValidNickname()`?
41. Why replay channel `PRIVMSG` history on `JOIN` if classic IRC does not?
    (Local choice so late joiners see context.)
42. Why can `Irc.hpp` and `NumericReplies.hpp` both hold similar trailing
    strings? Which path does `queueNumericReply()` actually use?
