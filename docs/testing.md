# Verification guide

This document explains how to verify that `ircserv` meets the subject specifications (summarized in `README.md`): several simultaneous clients, non-blocking I/O, a single `poll()`, IRC registration, channels, private messages and operator commands.

Verification combines **automatic tests** (`make test`) and **manual tests** with netcat and the reference client (**irssi**).

## 1. Compile and start the server

From the repository root:

```bash
make
./ircserv 6667 secret
```
Usage:

```text
./ircserv <port> <password>
```

- The port must be an integer between `1` and `65535`.
- The password cannot be empty.
- With a number of arguments other than 2, the program prints `Usage` and exits.

To see the Makefile rules:

```bash
make help
```

The binary is compiled with AddressSanitizer (`-fsanitize=address`). If the server aborts during tests, review the sanitizer report first.

Stop the server: `Ctrl+C` (SIGINT) or `SIGTERM`. It must close the descriptors and terminate cleanly.

---

## 2. Automatic tests

The protocol tests start `./ircserv` on a free port, with password `secret`, and speak IRC over TCP. They must be run **from the repository root**, where the binary is.

```bash
make test
```

That rule runs, in this order:

| Target | What it checks |
|---|---|
| `make test-parser` | IRC message parser (uppercase command, trailing with spaces, empty `:`). |
| `make test-message` | `IrcMessage` serialization and RFC 1459 casemap. |
| `make test-protocol` | Protocol checklist (registration, PING, CAP, PRIVMSG, JOIN/PART) plus the independent suites. |
| `make test-channel` | `Channel` model: members, operators, invitations, modes, topic and key. |

If everything is fine, each suite prints a success message and the process exits with code 0. A failure prints `expected` / `actual` and exits with a non-zero code.

### Protocol suites (mapping to the subject)

`make test-protocol` builds `ircserv` if needed and launches `ProtocolTests.cpp` plus every independent `src/tests/Protocol*Tests.cpp`.

| Suite | Subject requirement |
|---|---|
| `ProtocolTests.cpp` | Authentication (`PASS`/`NICK`/`USER`), welcome, `JOIN`, channel and private `PRIVMSG`, `PING`/`PONG`, `CAP`, `QUIT`. |
| `ProtocolChannelHistoryTests.cpp` | Channel `PRIVMSG` history forwarded to whoever `JOIN`s later. |
| `ProtocolFramingTests.cpp` | Reassembly of split commands (the subject’s `nc` + `Ctrl+D` test). |
| `ProtocolMultiClientTests.cpp` | Several clients at once: registration, channel, private messages, operators. |
| `ProtocolPartialWriteTests.cpp` | Partial TCP writes (slow client); the server does not block. |
| `ProtocolKickCommandTests.cpp` / `ProtocolKickErrorTests.cpp` | `KICK`. |
| `ProtocolInviteCommandTests.cpp` / `ProtocolInviteErrorTests.cpp` / `ProtocolInviteJoinTests.cpp` | `INVITE` and `+i` channel. |
| `ProtocolTopicQueryTests.cpp` / `ProtocolTopicSetTests.cpp` / `ProtocolTopicErrorTests.cpp` | `TOPIC` and mode `+t`. |
| `ProtocolModeFlagTests.cpp` | Modes `i` and `t`. |
| `ProtocolModeKeyLimitTests.cpp` | Modes `k` and `l`. |
| `ProtocolModeOperatorTests.cpp` | Mode `o` (grant/remove operator). |
| `ProtocolModeQueryTests.cpp` / `ProtocolModeCombinationTests.cpp` / `ProtocolModeErrorTests.cpp` | Query, combinations and `MODE` errors. |
| `ProtocolDisconnectTests.cpp` / `ProtocolChannelEdgeTests.cpp` / `ProtocolOversizedInputTests.cpp` / `ProtocolErrorRobustnessTests.cpp` | Disconnections, last member, lines > 512 bytes, error numerics. |
| `ProtocolDccTests.cpp` | File-transfer bonus: CTCP `DCC SEND`/`CHAT` forwarded as-is, `NOTICE` `DCC REJECT`, and file bytes over TCP between clients. |

Independent suites are discovered automatically: a new `src/tests/Protocol*Tests.cpp` file (except `ProtocolTests.cpp`) is included in `make test-protocol` without touching the Makefile.

---

## 3. Subject framing test (netcat)

The subject asks you to check that the server **aggregates TCP fragments** before parsing a command. With the server running:

```bash
nc -C 127.0.0.1 6667
```

Type `com`, press `Ctrl+D`, type `man`, press `Ctrl+D`, type `d` and press `Enter`. The server must reconstruct `command` (plus the newline) and not treat each fragment as a different command.

Equivalent, more explicit check:


```bash
# Three fragments: "NICK", " ro", "xy\n"
printf 'NICK' >&0
# Ctrl+D in the nc session, or:
python3 - <<'PY'
import socket, time
s = socket.create_connection(("127.0.0.1", 6667))
s.send(b"PASS secret\r\n")
time.sleep(0.05)
s.send(b"NICK")
time.sleep(0.05)
s.send(b" ro")
time.sleep(0.05)
s.send(b"xy\r\nUSER roxy 0 * :Roxy\r\n")
print(s.recv(4096).decode("utf-8", "replace"))
s.close()
PY
```

The client must not register until the complete line arrives. `ProtocolFramingTests.cpp` covers this case automatically.

---

## 4. Reference client: irssi

The subject requires a reference client that connects **without error** and behaves similarly to a real IRC server. In this project the reference client is **irssi**.

```bash
sudo apt install irssi
./ircserv 6667 secret
```

In another terminal:

```bash
irssi
```

Inside irssi:

```text
/connect 127.0.0.1 6667 secret
```

If the default nick is taken:


```text
/nick roxy
```

Irssi sends `CAP LS` on connect. The server must reply (`CAP * LS`) and **not disconnect**. Then irssi sends `PASS`, `NICK` and `USER`. The welcome must appear (numeric `001` and following).

### Second session (messages and channel)

```bash
irssi
```

```text
/set nick dani
/connect 127.0.0.1 6667 secret
```

Check, as the subject asks:

| Action | irssi command | Expected result |
|---|---|---|
| Join a channel | `/join #general` | The first user is an operator (`@`). The others see the `JOIN`. |
| Channel message | type in `#general` | Every member receives the text. The sender should not see an abnormal duplicate. |
| Private message | `/msg dani hello` | Only `dani` receives it. |
| View/change topic | `/topic Hello world` | Members see the new topic. |
| Channel modes | `/mode #general +i` | `JOIN` without an invitation fails. |
| Invite | `/invite dani #general` | `dani` receives `INVITE` and can enter with `+i`. |
| Kick | `/kick dani reason` | `dani` leaves the channel; the others see `KICK`. |
| Operator | `/mode #general +o dani` | `dani` becomes an operator. `/mode #general -o dani` removes it. |
| Key | `/mode #general +k key` | `JOIN` requires the key. |
| Limit | `/mode #general +l 1` | A third user cannot enter. |

Leave:

```text
/quit
```

The remaining channel members must see `QUIT`. The server keeps accepting connections.

HexChat or another graphical client is optional: it is useful to confirm interoperability, but evaluation is focused on irssi.

---

## 5. Manual tests with netcat (IRC commands)

Useful for seeing exact numerics without the irssi layer. Terminal 1: the server. Terminals 2 and 3: clients.

Client A:

```text
PASS secret
NICK roxy
USER roxy 0 * :Roxy
JOIN #general
PRIVMSG #general :hello channel
PRIVMSG dani :hello private
```

- Must receive `roxy`’s `JOIN` (if `roxy` enters later) or see `roxy` in `NAMES`.
- Must receive `PRIVMSG #general :hello channel`.
- Must receive the private `hello private`.
- `roxy` must not receive their own private message directed at `dani`.

Client B (after registering as `dani` and doing `JOIN #general`):

```text
PASS secret
NICK dani
USER dani 0 * :Dani
JOIN #general
```

Incomplete registration or bad password:

```text
PASS wrong
NICK roxy
USER roxy 0 * :Roxy
```

`001` must not be sent. Incorrect password → `464`. Missing parameters → `461`. Unknown command → `421`.

Channel operators (client A is `@` because they created `#general`):

```text
MODE #general +i
INVITE dani #general
TOPIC #general :topic
KICK #general dani :out
MODE #general +o dani
MODE #general +t
MODE #general +k secretkey
MODE #general +l 5
MODE #general -i-t-k-l-o dani
```

---

## 6. Subject checklist

Tick each point during evaluation or before submitting.

- [ ] The program is called `ircserv` and is started with `port` and `password`.
- [ ] `make` / `make all` / `clean` / `fclean` / `re` work.
- [ ] Several clients are served at the same time without the server hanging.
- [ ] `fork` is not used for clients. All I/O is non-blocking (`fcntl`).
- [ ] A single `poll()` (or equivalent) covers listen, read and write.

### Reference client

- [ ] irssi connects to `ircserv` without error.
- [ ] The flow looks like a usual IRC server (welcome, channels, messages).

### Mandatory functionality

- [ ] Authentication with the server password (`PASS`).
- [ ] Nickname (`NICK`) and username (`USER`).
- [ ] `JOIN` a channel.
- [ ] Private messages (`PRIVMSG` to a nick).
- [ ] A `PRIVMSG` to a channel reaches **every** other member.
- [ ] There are channel operators and regular users.
- [ ] `KICK` removes a client from the channel.
- [ ] `INVITE` invites a client to a channel.
- [ ] `TOPIC` queries or changes the topic.
- [ ] `MODE +i` / `-i`: invite-only channel.
- [ ] `MODE +t` / `-t`: only operators can change the topic.
- [ ] `MODE +k` / `-k`: channel key.
- [ ] `MODE +o` / `-o`: grant or remove operator privilege.
- [ ] `MODE +l` / `-l`: user limit.

### Robustness (subject example)

- [ ] A command split across several `recv`s is reassembled (`nc` + `Ctrl+D` or `make test-protocol`).
- [ ] After a problematic client (abrupt disconnect, huge line, invalid command), the server stays alive.

---

## 7. Several clients and limits

The subject requires not hanging with several clients. Quick checks:

```bash
# Server with few descriptors (the process must not abort uncleanly)
bash -c 'ulimit -n 16; exec ./ircserv 6667 secret'
```

In another terminal, many connections:

```bash
python3 -c 'import socket,time; c=[socket.create_connection(("127.0.0.1",6667)) for _ in range(30)]; print(len(c),"clients"); time.sleep(10)'
```

If the fd limit is exhausted, the server must keep the `poll` loop with the clients it already had; it must not block.

Helper scripts in `docs/utils/`:

| Script | Use |
|---|---|
| `docs/utils/force_close_tcp.py` | TCP close with `SO_LINGER` (RST). The server must clean up the client. |
| `docs/utils/search_errors.py` | Opens and closes 1000 connections in a row. |
| `docs/utils/monitoring.sh` | Statistics of the `ircserv` process (CPU, memory, fds). |
| `docs/utils/commands.md` | How to locate `IP:PORT` with `ss`. |

See also `docs/slow_client.md` (output buffer and slow client) and `docs/phases/phase_19.md` (framing, disconnections, oversized lines).

---

## 8. Memory and descriptors

The default build includes AddressSanitizer. `make test` already detects many invalid accesses.

For leaks and file descriptors, **recompile without sanitizer** (ASan and Valgrind do not combine well):

```bash
make fclean
# Temporarily compile without -fsanitize=address, or use an equivalent debug build
valgrind --leak-check=full --track-fds=yes ./ircserv 6667 secret
```

Connect and disconnect several clients (irssi or netcat), join channels and leave. When stopping the server, Valgrind must not report client leaks or unclosed sockets (except standard descriptors).

---

## 9. Bonus part (if implemented)

The subject marks as bonus:

- File transfer (DCC over CTCP `PRIVMSG`; the server forwards the message).
- A bot.

They are not part of the mandatory checklist. If they exist, try them with irssi (`/dcc send`, message to the bot, `/list`) in addition to `make test`.

### File transfer (DCC)

The file **does not pass through the IRC server**. The sender opens a TCP socket, announces `DCC SEND` in a CTCP `PRIVMSG`, the server forwards that text unchanged (including the `\x01`) and the receiver connects to the sender.

Automatic check: `make test-protocol` includes `ProtocolDccTests.cpp`.

Check with irssi (two clients, server `./ircserv 6667 secret`):

```text
# Client A
/connect 127.0.0.1 6667 secret
/nick alice
/dcc send bob /tmp/hello.txt

# Client B
/connect 127.0.0.1 6667 secret
/nick bob
/dcc get alice
```

- B must receive the DCC offer (CTCP `DCC SEND` message).
- After accepting, the file must arrive at B.
- The server must not rewrite the payload or truncate the file name.

With netcat, the handshake is a normal `PRIVMSG`:

```text
PRIVMSG bob :^ADCC SEND hello.txt 2130706433 5000 5^A
```

`^A` is the SOH character (`\x01`). The server must deliver it to `bob` with the same bytes.

### Bot

The bot is a virtual IRC user **without a socket**: it does not enter `poll()`, reserves the nick `marvin` and stays in `#bot`. It replies to private queries and to `!command` or `marvin: command` in the channels it is a member of. An `INVITE` makes it join the channel; `NOTICE` does not generate an automatic reply.

Automatic check: `make test-protocol` includes `ProtocolBotTests.cpp`.

Check with irssi (`./ircserv 6667 secret`):

```text
/connect 127.0.0.1 6667 secret
/nick alice
/msg marvin help
/join #bot
# in #bot:
!ping
!time
marvin: dice
```

To take it to another channel:

```text
/join #lounge
/invite marvin #lounge
!help
```

Commands: `help`, `ping`, `time`, `date`, `info`, `uptime`, `version`, `users`, `whoami`, `echo <text>`, `dice`.

With netcat, after `PASS` / `NICK` / `USER`:

```text
PRIVMSG marvin :help
PRIVMSG marvin :ping
JOIN #bot
PRIVMSG #bot :!ping
```

The nick `marvin` cannot be registered (433). The bot does not use an extra `poll()` or a child process.

---

## 10. Recommended work order

1. `make test` — if it fails, the protocol or framing is not ready.
2. Start `./ircserv 6667 secret` and irssi: connect, `JOIN`, channel and private messages.
3. Second irssi session: operators (`KICK`, `INVITE`, `TOPIC`, `MODE i/t/k/o/l`).
4. Subject `nc` + `Ctrl+D` test.
5. Abrupt disconnections and several clients (`docs/utils/`).
6. Walk through the list in section 6.

---

## 11. More tests

Server running (`./ircserv 6667 secret`) and netcat clients as in section 5. In each case, register the clients first (`PASS` / `NICK` / `USER`). The first user who enters an empty channel is an operator (`@`).

### 1. Messages reach clients already in the channel

Client A:

```text
PASS secret
NICK roxy
USER roxy 0 * :Roxy
JOIN #general
PRIVMSG #general :hello channel
```

Client B:

```text
PASS secret
NICK dani
USER dani 0 * :Dani
JOIN #general
```

- B must receive `:roxy!… PRIVMSG #general :hello channel`.
- A must not receive their own channel message.
- A registered client C, without `JOIN #general`, must not receive that `PRIVMSG`.

### 2. A regular user cannot use operator commands

Client A creates the channel and restricts the topic. Client B enters as a normal member. Client C registers (`charlie`) and does not enter the channel.

Client A:

```text
JOIN #ops
MODE #ops +t
```

Client B:

```text
JOIN #ops
KICK #ops roxy :out
INVITE charlie #ops
TOPIC #ops :should not
MODE #ops +i
```

- Each of B’s commands must reply `482` (`ERR_CHANOPRIVSNEEDED`).
- A must not see `KICK`, `INVITE`, `TOPIC` or `MODE`.
- C must not receive `INVITE`.
- `MODE #ops` (query, no flags) can be used by B: numeric `324`.

### 3. An operator can use operator commands in each channel they created

Client A creates two channels. Client B enters both. Client C registers (`charlie`) and does not enter.

Client A:

```text
JOIN #alpha
JOIN #bravo
MODE #alpha +t
MODE #bravo +t
TOPIC #alpha :alpha topic
TOPIC #bravo :bravo topic
INVITE charlie #alpha
INVITE charlie #bravo
MODE #alpha +o dani
MODE #bravo +o dani
KICK #alpha dani :out
KICK #bravo dani :out
```

- In `#alpha` and `#bravo`, B must see `MODE`, `TOPIC` and `KICK`.
- C must receive both `INVITE`s.
- A must receive `341` (`RPL_INVITING`) for each invitation.
- Privilege is per channel: if B creates `#charlie` and A enters later, A is a regular member. `MODE #charlie +i` from A must reply `482`.

### 4. Client A sends a message; B receives it when joining

Client A:

```text
JOIN #general
PRIVMSG #general :before b joins
```

- B, still outside the channel, must not receive that `PRIVMSG` in real time.

Client B:

```text
JOIN #general
```

- After `JOIN`, `331`/`332`, `353` and `366`, B must receive `:roxy!… PRIVMSG #general :before b joins`.

Client A:

```text
PRIVMSG #general :after b joins
```

- B must receive `:roxy!… PRIVMSG #general :after b joins`.

### 5. Invite-only channel (`+i`)

Client A:

```text
JOIN #invite
MODE #invite +i
```

Client B:

```text
JOIN #invite
```

- B must receive `473` (`ERR_INVITEONLYCHAN`) and not enter.
- A must not see a `JOIN` from B.

Optional, to confirm that the invitation unlocks entry:

```text
INVITE dani #invite
```

Client B:

```text
JOIN #invite
```

- B enters. A and B see B’s `JOIN`.

### 6. Channel with a key (`+k`)

Client A:

```text
JOIN #keyed
MODE #keyed +k secretkey
```

Client B:

```text
JOIN #keyed
JOIN #keyed wrong
```

- Both `JOIN`s must reply `475` (`ERR_BADCHANNELKEY`). B does not enter.

Client B, with the correct key:

```text
JOIN #keyed secretkey
```

- B enters. A and B see B’s `JOIN`.

### 7. User limit (`+l`)

Client A (only member; the limit must be `1` so B does not fit):

```text
JOIN #limited
MODE #limited +l 1
```

Client B:

```text
JOIN #limited
```

- B must receive `471` (`ERR_CHANNELISFULL`) and not enter.
- A must not see a `JOIN` from B.

Optional: `MODE #limited -l` or `MODE #limited +l 2` and repeat B’s `JOIN`; now they must enter.

---

## 12. More tests (irssi)

The same cases as section 11, with the reference client. Server running (`./ircserv 6667 secret`) and one irssi session per client, as in section 4. Error numerics (`482`, `473`, `475`, `471`, `324`, `341`) appear in the status window.

Client A:

```text
/set nick roxy
/connect 127.0.0.1 6667 secret
```

Client B:

```text
/set nick dani
/connect 127.0.0.1 6667 secret
```

When a third client is needed (cases 2 and 3):

```text
/set nick charlie
/connect 127.0.0.1 6667 secret
```

The first user who enters an empty channel is an operator (`@`).

### 1. Messages reach clients already in the channel

Client A:

```text
/join #general
```

Client B:

```text
/join #general
```

Client A, in the `#general` window:

```text
hello channel
```

- B must see `hello channel` in `#general`.
- A must not see their own message duplicated abnormally.
- A connected client C, without `/join #general`, must not receive that text.

### 2. A regular user cannot use operator commands

Client A creates the channel and restricts the topic. Client B enters as a normal member. Client C (`charlie`) does not enter the channel.

Client A:

```text
/join #ops
/mode #ops +t
```

Client B:

```text
/join #ops
/kick roxy out
/invite charlie #ops
/topic should not
/mode #ops +i
```

- Each of B’s commands must show `482` (`ERR_CHANOPRIVSNEEDED`) in the status window.
- A must not see `KICK`, `INVITE`, `TOPIC` or a mode change.
- C must not receive an invitation.
- `/mode #ops` (query, no flags) can be used by B: numeric `324`.

### 3. An operator can use operator commands in each channel they created

Client A creates two channels. Client B enters both. Client C (`charlie`) does not enter.

Client A:

```text
/join #alpha
/join #bravo
/mode #alpha +t
/mode #bravo +t
/topic #alpha alpha topic
/topic #bravo bravo topic
/invite charlie #alpha
/invite charlie #bravo
/mode #alpha +o dani
/mode #bravo +o dani
/kick #alpha dani out
/kick #bravo dani out
```

- In `#alpha` and `#bravo`, B must see the mode change, the new topic and the `KICK`.
- C must receive both invitations (`INVITE` notice in the status window).
- A must receive `341` (`RPL_INVITING`) for each invitation.
- Privilege is per channel: if B creates `#charlie` (`/join #charlie`) and A enters later, A is a regular member. `/mode #charlie +i` from A must show `482`.

### 4. Client A sends a message; B receives it when joining

Client A:

```text
/join #general
```

In the `#general` window:

```text
before b joins
```

- B, still outside the channel, must not see that text in real time.

Client B:

```text
/join #general
```

- B must see `before b joins` in `#general` when they enter.

Client A, again in `#general`:

```text
after b joins
```

- B must see `after b joins` in `#general`.

### 5. Invite-only channel (`+i`)

Client A:

```text
/join #invite
/mode #invite +i
```

Client B:

```text
/join #invite
```

- B must see `473` (`ERR_INVITEONLYCHAN`) and not enter.
- A must not see a `JOIN` from B.

Optional, to confirm that the invitation unlocks entry.

Client A:

```text
/invite dani #invite
```

Client B:

```text
/join #invite
```

- B enters. A and B see B’s `JOIN`.

### 6. Channel with a key (`+k`)

Client A:

```text
/join #keyed
/mode #keyed +k secretkey
```

Client B:

```text
/join #keyed
/join #keyed wrong
```

- Both `/join`s must show `475` (`ERR_BADCHANNELKEY`). B does not enter.

Client B, with the correct key:

```text
/join #keyed secretkey
```

- B enters. A and B see B’s `JOIN`.

### 7. User limit (`+l`)

Client A (only member; the limit must be `1` so B does not fit):

```text
/join #limited
/mode #limited +l 1
```

Client B:

```text
/join #limited
```

- B must see `471` (`ERR_CHANNELISFULL`) and not enter.
- A must not see a `JOIN` from B.

Optional: `/mode #limited -l` or `/mode #limited +l 2` and repeat B’s `/join #limited`; now they must enter.
