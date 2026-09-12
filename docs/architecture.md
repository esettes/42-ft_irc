### Server
 - listening socket
 - poll descriptors
 - clients
 - channels
 - command dispatcher
 - main event loop

### Client
 - connection state
 - registration state
 - identity
 - input buffer
 - output buffer
 - joined channels

### Channel
 - members
 - operators
 - invited clients
 - topic
 - key
 - limit
 - modes

### Message
 - command
 - parameters

## General flow

```text
socket → Client → MessageParser → CommandDispatcher → CommandHandler
                                        ↓
                                    MessageRouter
                                ↙        ↓        ↘
                            Client    Channel      Bot
```

Responsibilities of each part:

- `Client`: connection, buffers, and user state.
- `MessageParser`: converts IRC text into an IrcMessage.
- `CommandDispatcher`: selects the handler for `NICK`, `JOIN`, `PRIVMSG`, and so on.
- `MessageRouter`: decides who receives a message.
- `Bot`: processes only the messages directed at it.
- `Channel`: manages members, operators, and invitations.

### Bot

The bot bonus is implemented as a virtual `Client` (`fd < 0`) with no entry in `poll()`. `Bot` owns that identity, reserves the nick `marvin`, and joins `#bot` at startup. Real users have a socket and buffers; the bot shares the channel model, `NAMES`, and `INVITE` without an extra descriptor.

`PRIVMSG` directed at the nick or at a channel the bot is a member of is processed in `Bot`; `NOTICE` does not generate an automatic reply. An `INVITE` causes the bot to `JOIN`.

### File transfer

In IRC the file does not normally pass through the IRC server. DCC is used:

- The sender opens a TCP socket for the file.
- It sends the receiver a CTCP message via PRIVMSG.
- The IRC server forwards that message unchanged.
- The receiver connects directly to the sender’s socket.
- The file bytes travel directly between the two clients.

A message looks like:

`PRIVMSG Roxana :\x01DCC SEND example.txt 2130706433 5000 1200\x01`

The `\x01` characters delimit a CTCP message. For the server it remains a normal `PRIVMSG`.

Therefore, to support DCC you must ensure that:

- The parser keeps the full trailing parameter after `:`.
- The contents of `PRIVMSG` are not split on spaces.
- The `\x01` character is not removed.
- The `DCC SEND` message is not reinterpreted.
- The content is forwarded exactly to the recipient.
- IRC sockets are non-blocking and support partial sends through the output buffer.
- Only characters that are truly invalid for an IRC line are rejected, such as internal CR/LF or NUL.

```text
command: "PRIVMSG"

parameters[0]:
"Roxana"

parameters[1]:
"\x01DCC SEND example.txt 2130706433 5000 1200\x01"
```
