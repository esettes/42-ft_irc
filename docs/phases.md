```text
socket + poll
    ↓
byte reception
    ↓
per-client buffer
    ↓
extraction of complete lines
    ↓
IRC parser
    ↓
command validation and execution
    ↓
reply generation
    ↓
output buffer
    ↓
send when poll reports POLLOUT
```

## Phase 0 — Study the protocol and choose a reference client

- Choose a real client for tests: HexChat, irssi, WeeChat, etc.
- Observe which commands it sends as soon as it connects.
- Define exactly which commands will be supported.
- Define the internal message format.
- Define the numeric codes that will be needed.

The client will probably send something similar to:

```text
CAP LS 302
PASS password
NICK roxana
USER roxana 0 * :Roxana
```
Even if `CAP` is not a main feature of the subject, real clients commonly send it. The server should recognize it or ignore it correctly, not disconnect.

## Phase 1 — Project skeleton
- Check that there are exactly two arguments.
- Validate the port.
- Store the password.
- Build the Server object.
- Handle termination through signals.
- Close every file descriptor correctly.

Execution:

```bash
./ircserv port password
```

## Phase 2 — Listening socket

- `socket()`.
- `setsockopt()` with `SO_REUSEADDR`.
- `fcntl()` to make it non-blocking.
- `bind()`.
- `listen()`.

```bash
nc 127.0.0.1 6667
``` 

A TCP connection must be able to be established.

## Phase 3 — Main loop with a single poll()

The server should keep a collection similar to:

```cpp
std::vector<pollfd> pollDescriptors;
```

The first descriptor will normally be the listening socket:

```text
pollDescriptors[0] → listening socket
pollDescriptors[1] → client A
pollDescriptors[2] → client B
pollDescriptors[3] → client C
```

The main loop will conceptually be:

```text
poll()
 ├── listener has POLLIN → accept()
 ├── client has POLLIN → recv()
 ├── client has POLLOUT → send()
 └── error/disconnection → remove client
 ```

 There must not be one poll() for accepting, another for reading and another for writing. There must be a single central point that manages every descriptor. The subject forbids calling recv() or send() without first checking availability through poll() or an equivalent.

 Goal:

- Accept several clients.
- Do not block.
- Detect disconnections.
- Remove their descriptors correctly.

## Phase 4 — Basic client model

When a connection is accepted, a ``Client`` object is created.
```text
Client
 ├── socket file descriptor
 ├── input buffer
 ├── output buffer
 ├── nickname
 ├── username
 ├── real name
 ├── password accepted
 ├── registered
 └── channels joined
```

Recommended states:
```cpp
bool passwordAccepted;
bool nicknameReceived;
bool usernameReceived;
bool registered;
```

A client will be registered when this is satisfied:

```text
correct PASS
    +
valid and available NICK
    +
USER received
```

## Phase 5 — Reconstructing the TCP stream

Each client needs a persistent buffer: ``std::string inputBuffer;``

When recv() returns data:
1. received data
2. they are appended to inputBuffer
3. line terminators are searched
4. only complete commands are extracted
5. leftovers remain in inputBuffer

Example of fragmented reception:
```text
First recv:    "PRIV"
Second recv:   "MSG #general :Hello"
Third recv:    "\r\n"
```
The parser must not receive those three parts. It must finally receive:

```text
PRIVMSG #general :Hello
```
The opposite can also happen:
```text
PASS secret\r\nNICK roxana\r\nUSER roxana 0 * :Roxana\r\n
```
All of that may arrive in a single `recv()`, so three commands must be extracted.

- recv() does not call the parser directly with what it just received.
- recv() appends bytes to the buffer.
- The framing system extracts lines.
- The parser receives complete lines.

## Phase 6 — IRC parser

The parser should transform:

`PRIVMSG #general :Hello everyone`

into something like:

```text
Command
 ├── name: "PRIVMSG"
 ├── parameters:
 │    └── "#general"
 └── trailing: "Hello everyone"
 ```

Simple structure:
```cpp
class Command
{
private:
    std::string commandName;
    std::vector<std::string> parameters;
};
```

#### Fundamental parser rules

The line:
`COMMAND param1 param2 :text with spaces`

contains:
- A command name.
- Parameters separated by spaces.
- An optional final parameter that starts with `:`.
- The final parameter may contain spaces.

Example:

`USER roxana 0 * :Roxana Example`

Must produce:

```text
command = USER
parameters[0] = roxana
parameters[1] = 0
parameters[2] = *
parameters[3] = Roxana Example
```

## Phase 7 — Output buffer and non-blocking writes

Do not assume that a call to send() will send the whole message.

Each client must have:

```cpp
std::string outputBuffer;
```

When you want to reply:

```text
IRC reply
    ↓
it is appended to outputBuffer
    ↓
POLLOUT is enabled for that client
    ↓
poll reports that writing is possible
    ↓
send tries to send
    ↓
only the bytes actually sent are removed
```

Example:

```text
outputBuffer has 200 bytes
send returns 80
120 bytes remain pending
```

The 200 bytes must not be deleted.

When the buffer becomes empty, stop requesting POLLOUT, otherwise poll() may wake up constantly and consume CPU.

## Phase 8 — IRC reply system

Centralize message construction.

Usual format:

`:server.name 001 roxana :Welcome to the IRC Network`

Message emitted by a user:

`:roxana!username@hostname PRIVMSG #general :Hello`

It is useful to create separate functions:

```text
buildNumericReply()
buildClientPrefix()
sendReply()
queueMessage()
```

Implement basic error replies:

```text
431 ERR_NONICKNAMEGIVEN
432 ERR_ERRONEUSNICKNAME
433 ERR_NICKNAMEINUSE
451 ERR_NOTREGISTERED
461 ERR_NEEDMOREPARAMS
462 ERR_ALREADYREGISTERED
464 ERR_PASSWDMISMATCH
421 ERR_UNKNOWNCOMMAND
```

## Phase 9 — Client registration

`PASS`

```text
PASS secret
```

Must check:

- That it has a parameter.
- That the user is not registered yet.
- That the password matches.

`NICK`

```text
NICK roxana
```

Must check:

- That the parameter exists.
- That the nickname has a valid format.
- That it is not already in use.
- That the change is propagated if the client was already registered.

An efficient global lookup is needed:

`nickname -> Client`

`USER`

```text
USER roxana 0 * :Roxana
```

Must store:

- Username.
- Real name.
- Other fields you decide to keep.

After `PASS`, `NICK` or `USER`, call a function similar to:

`void Server::tryRegisterClient(Client &client);`

When the requirements are complete, the welcome message is sent exactly once.

## Phase 10 — Auxiliary connection commands

A real client may send:

`PING :token`

The server must reply:

`PONG :token`

---

`QUIT`

Must:

- Notify the leave to the affected clients.
- Remove the user from all of their channels.
- Remove them from operator and invite lists.
- Close their descriptor.
- Remove their nickname from the global indexes.

`CAP`

A minimal reply can be implemented, or the negotiation can be finished correctly. At a minimum, it must not break registration.

## Phase 11 — Channel model

Create the `Channel` class.

```text
Channel
 ├── name
 ├── topic
 ├── members
 ├── operators
 ├── invited clients
 ├── invite-only mode
 ├── topic-restricted mode
 ├── key
 └── user limit
 ```

States corresponding to the mandatory modes:

```cpp
bool inviteOnly;
bool topicRestricted;
bool keyEnabled;
bool limitEnabled;
std::string channelKey;
std::size_t userLimit;
```

The first user who creates or enters an empty channel should become an operator.

The server should own the channels:

`channel name → Channel`

It is not a good idea for each client to own independent copies of the same channel.

## Phase 12 — `JOIN`

`JOIN #general`

The flow should be:


```text
Is the client registered?
    ↓
Is the channel name valid?
    ↓
Does it exist?
 ├── no → create it and make the first user an operator
 └── yes → check restrictions
              ├── mode +i
              ├── key +k
              └── limit +l
    ↓
add the client
    ↓
notify JOIN
    ↓
send topic
    ↓
send member list
```

Usual replies:

```text
331 RPL_NOTOPIC
332 RPL_TOPIC
353 RPL_NAMREPLY
366 RPL_ENDOFNAMES
```

Relevant errors:

```text
403 ERR_NOSUCHCHANNEL
471 ERR_CHANNELISFULL
473 ERR_INVITEONLYCHAN
475 ERR_BADCHANNELKEY
```

Even though `PART` does not appear as one of the subject’s central features, implementing it at this point simplifies tests and gives more natural behaviour with real clients.

## Phase 13 — `PRIVMSG`

First implement private messages between users:

`PRIVMSG roxana :Hello`

Then messages to channels:

`PRIVMSG #general :Hello everyone`

For a channel:

- The sender must belong to the channel.
- The message is forwarded to every other member.
- It is normally not forwarded back to the sender.
- The sender’s prefix must be preserved.

Example sent to the receivers:

`:roxana!roxana@localhost PRIVMSG #general :Hello everyone`

The subject requires private messages and that messages directed at a channel are distributed to the other members.

Relevant errors:

```text
401 ERR_NOSUCHNICK
403 ERR_NOSUCHCHANNEL
404 ERR_CANNOTSENDTOCHAN
411 ERR_NORECIPIENT
412 ERR_NOTEXTTOSEND
```

## Phase 14 — `TOPIC`

First implement the query:

`TOPIC #general`

And then the change:

`TOPIC #general :New topic`

Rules:

- The channel must exist.
- The user must belong to the channel.
- If mode +t is active, only an operator can change it.
- Querying it should not require being an operator.
- The change must be notified to the channel.

## Phase 15 — `INVITE`

Format:

`INVITE roxana #private`

Checks:

- The channel exists.
- The target user exists.
- The sender belongs to the channel.
- The target does not already belong to the channel.
- When required, the sender must be an operator.
- The invited user is added to the invited collection.

Afterwards, an invited user can bypass the `+i` restriction when they `JOIN`.

Once the invitation has been consumed, it should be removed.

## Phase 16 — `KICK`

Format:

`KICK #general roxana :Reason`

Must check:

- Existing channel.
- Existing target user.
- Sender belonging to the channel.
- Sender is an operator.
- Target belonging to the channel.

Then:

- Notify the KICK to every member.
- Remove the target from the channel.
- Remove their operator privileges if they had them.
- Delete the channel if it becomes empty.

## Phase 17 — `MODE`

The subject requires:

```text
+i / -i    invite only
+t / -t    topic restricted
+k / -k    channel key
+o / -o    channel operator
+l / -l    user limit
```

The subject expressly requires `KICK`, `INVITE`, `TOPIC` and these five channel modes.

##### Recommended order

First:

`MODE #channel`

To query the current modes.

Then:

```text
MODE #channel +i
MODE #channel -i
MODE #channel +t
MODE #channel -t
```

Then modes with arguments:

```text
MODE #channel +k secret
MODE #channel -k
MODE #channel +l 10
MODE #channel -l
MODE #channel +o roxana
MODE #channel -o roxana
```

Finally, combinations:

```text
MODE #channel +it
MODE #channel +kl secret 10
MODE #channel -it
MODE #channel +o-l roxana
```

The mode string must be walked and parameters consumed only when the mode requires it.

Example:

`MODE #general +kol password roxana 10`

Each letter has a different meaning and associated parameter. It is useful to implement a dedicated mode parser, separate from the general IRC message parser.

## Phase 18 — State cleanup and disconnections

When a client disconnects you must:

```text
remove it from poll
remove it from the client map
remove its nickname
remove it from every channel
remove it from operators
remove it from invited
notify QUIT
close the fd
delete empty channels
```

You must avoid:

- Invalidated iterators.
- Dangling pointers.
- Clients present in a channel after being destroyed.
- Operators who are no longer members.
- Invitations to nonexistent clients.
- Empty channels that remain stored.

It is recommended that every disconnection go through a single function:

```cpp
void Server::disconnectClient(int clientFileDescriptor, const std::string &reason);
```

Never split this logic partially across `recv()`, `QUIT`, `KICK` and the destructor.

## Phase 19 — Robustness and adversarial tests

#### TCP framing

```text
"PRIV"
"MSG #general :he"
"llo\r"
"\n"
```

#### Several commands together

`"NICK one\r\nUSER one 0 * :One\r\nJOIN #a\r\n"`

#### Terminators

```text
\r\n
\n
```

Internally you can be tolerant with `\n`, but when sending IRC replies you must use `\r\n`.

#### Partial writes

Simulate a large output buffer and verify that bytes are not lost when `send()` returns fewer bytes than requested.

#### Protocol errors

Try:

```text
NICK
NICK existingNick
JOIN
JOIN invalid
PRIVMSG
PRIVMSG nobody :hello
MODE #channel +k
KICK #channel nobody
```

#### Disconnections

- Disconnect a user who is in several channels.
- Disconnect the only operator.
- Disconnect the last member.
- Close the client while it has pending messages.
- Receive `POLLHUP`, `POLLERR` or `recv() == 0`.

#### Memory

```bash
valgrind --leak-check=full --track-fds=yes ./ircserv 6667 secret
```

## Clients

Irssi as the main client.

It is lightweight, works in a terminal and forces you to understand the real IRC commands, without a graphical interface hiding server bugs. It is also very convenient for opening several connections and testing channels, operators, kicks, invites, and so on.

```bash
sudo apt install irssi
irssi
```

Then start a listener:

```bash
nc -lv 127.0.0.1 6667
```

Recommendations:

- Irssi: main client for developing and evaluating IRC behaviour.
- netcat (`nc`): low-level tests of the parser, incomplete commands, CRLF, errors and edge cases.
- HexChat: optional, at the end, to check that it also works with a real graphical client.
