# ft_irc implementation to-do

Current status: the mandatory part is incomplete. CLI, non-blocking socket,
`poll()`, basic multi-client handling, TCP framing, parser, output buffer and
partial sends already exist. The following work remains.

## 1. Protocol and replies

- [x] Complete the single serialization of `IrcMessage`: prefix, command, parameters, trailing and `\r\n`.
- [x] Reject internal CR, LF and NUL in generated messages.
- [x] Define a stable server name and store each client’s hostname or IP.
- [x] Centralize the `:server` and `:nick!user@host` prefixes.
- [x] Centralize numeric replies, their three-digit format and the `*` target when the nick is missing.
- [x] Make every handler queue replies and enable `POLLOUT`; never write directly or modify the buffer without updating `poll()`.
- [x] Return errors from the dispatcher instead of ignoring unknown commands, missing parameters or unregistered clients.
- [x] Implement the documented numerics: `001–005`, `324`, `331`, `332`, `341`, `353`, `366`, `401`, `403`, `404`, `409`, `411`, `412`, `421`, `431`, `432`, `433`, `441`, `442`, `443`, `451`, `461`, `462`, `464`, `471`, `472`, `473`, `475` and `482`.
- [x] Limit every IRC line to 512 bytes, including `\r\n`.
- [x] Limit the input and output buffers; disconnect abusive or too-slow clients cleanly.
- [x] Implement IRC casemapping for nicknames and channels: keep the original name, but look it up through a normalized key.

References: [phase 0](phases/phase_0.md) and [phase 8](phases/phase_8.md).

## 2. Registration

- [ ] Implement `PASS`: check the real password and handle `461`, `462` and `464`.
- [ ] Guarantee that an incorrect password never sets `passwordAccepted`.
- [ ] Implement `NICK`: validate the format, prevent duplicates and reply with `431`, `432` or `433`.
- [ ] Create a normalized global index `nickname -> Client`.
- [ ] Allow nickname changes after registration; update the index and notify related users once.
- [ ] Release the nickname when the client disconnects.
- [ ] Implement `USER`: require the necessary parameters, store username and realname and reject repeats with `462`.
- [ ] Complete registration only with `PASS + NICK + USER`; keep the operation idempotent.
- [ ] Send the welcome exactly once. Minimum: `001`; full documented contract: `001–005`.
- [ ] Before registration, allow only `CAP`, `PASS`, `NICK`, `USER`, `PING`, `PONG` and `QUIT`.
- [ ] Reply `451` to messaging or channel commands executed before registration.

Reference: [phase 9](phases/phase_9.md).

## 3. Connecting with a real client

- [ ] Implement `PING`: reply `PONG` with the exact token, also before registration; reply `409` if it is missing.
- [ ] Accept `PONG` without producing `421`.
- [ ] Implement `CAP LS` with an empty capability reply.
- [ ] Implement `CAP LIST`.
- [ ] Implement `CAP REQ` with a `NAK` reply.
- [ ] Implement `CAP END` without blocking registration.
- [ ] Implement `QUIT`: keep the reason, notify related users once and disconnect.
- [ ] Verify a complete connection with Irssi, the client chosen in the documentation.

Reference: [phase 10](phases/phase_10.md).

## 4. Channel model

- [ ] Implement the constructor and the full `Channel` API.
- [ ] Manage name, topic, members, operators and invited clients.
- [ ] Implement the state of modes `+i`, `+t`, `+k` and `+l`.
- [ ] Keep operators per channel; the global `Client::isOperator` state does not represent the IRC model correctly.
- [ ] Guarantee that every operator is a member, that there are no duplicate members and that invitations point to valid clients.
- [ ] Keep key and limit in a consistent state when their modes are disabled.
- [ ] Create, look up and destroy a single instance of each channel from `Server`.
- [ ] Keep `Client::joinedChannels` synchronized with the real members of each channel.
- [ ] Remove empty channels.

Reference: [phase 11](phases/phase_11.md).

## 5. Channel and messaging commands

- [ ] Implement `JOIN`: validate the channel, create it, add the member and make the first user an operator.
- [ ] Apply modes `+i`, `+k` and `+l` on `JOIN`; consume the invitation only after a successful join.
- [ ] Broadcast `JOIN` and send `331` or `332`, followed by `353` and `366`.
- [ ] Prevent duplicate entries in a channel.
- [ ] Implement `PART`: validate membership, broadcast the leave, clean up state and delete the empty channel.
- [ ] Implement `NAMES`: return the members and mark operators with `@`.
- [ ] Implement `PRIVMSG` to a nickname: locate the target and deliver the actual message.
- [ ] Implement `PRIVMSG` to a channel: require membership and forward to every member except the sender.
- [ ] Keep the trailing exactly, including CTCP/DCC messages.
- [ ] Handle errors `401`, `403`, `404`, `411` and `412`.
- [ ] Implement `NOTICE` with the same routing as `PRIVMSG`, but without automatic error replies.
- [ ] Remove the existing dummy reply `Message sent to...`.

References: [phase 12](phases/phase_12.md) and [phase 13](phases/phase_13.md).

## 6. Operator commands

- [ ] Implement `TOPIC`: query, set and clear the topic; apply `+t` and broadcast changes.
- [ ] Implement `INVITE`: check user, channel, membership and privileges; store the invitation; send `341` and notify the invitee.
- [ ] Consume an invitation only after a successful `JOIN` and clear it if the client disconnects.
- [ ] Implement `KICK`: validate operator, target and membership; notify before removing the user.
- [ ] Keep the kicked user’s connection open, remove their membership and privileges and delete the channel if it becomes empty.
- [ ] Implement the `MODE #channel` query through `324`.
- [ ] Implement `+i/-i`, `+t/-t`, `+k/-k`, `+l/-l` and `+o/-o`.
- [ ] Validate that the user limit is positive and does not overflow.
- [ ] Process combinations and sign changes, such as `+it`, `+kl` and `+o-l`.
- [ ] Consume the parameters required by each mode correctly.
- [ ] Validate the whole command before changing state to avoid partial updates.
- [ ] Broadcast mode changes to channel members.
- [ ] Integrate the modes with `JOIN`, `TOPIC`, `INVITE` and `KICK`.

References: [phase 14](phases/phase_14.md), [phase 15](phases/phase_15.md), [phase 16](phases/phase_16.md) and [phase 17](phases/phase_17.md).

## 7. Disconnection and robustness

- [ ] Unify every disconnection in an idempotent function.
- [ ] Use it for `QUIT`, `recv() == 0`, fatal errors, `POLLHUP`, `POLLERR`, `POLLNVAL` and write errors.
- [ ] Collect recipients before destroying the client and avoid duplicate `QUIT` messages.
- [ ] Remove the client from `poll`, the main map, the nickname index, channels, operators and invitations.
- [ ] Close each descriptor once and never use a client after deleting it.
- [ ] Avoid invalidated iterators when deleting clients or channels.
- [ ] Recover from individual client errors without stopping the whole server.
- [ ] Handle descriptor exhaustion and `accept()` failures without corrupting state.
- [ ] Avoid logging complete lines that include `PASS`.
- [ ] Complete or remove incomplete declarations such as `stop()`, `getPort()` and the lookup and prefix helpers.

Reference: [phase 18](phases/phase_18.md).

## 8. Pending tests

- [ ] Expand the parser tests: currently there is only one happy-path case.
- [ ] Test fragmented commands, grouped commands, empty trailing, spaces, prefixes and invalid input.
- [ ] Test the 512-byte limit and buffers that never receive a terminator.
- [ ] Test partial writes, slow clients and disconnections with pending output.
- [ ] Test every numeric and incomplete command.
- [ ] Test several clients, unique nicknames, channels, messages, topics, invitations, kicks and modes.
- [ ] Test every disconnection and cleanup path.
- [ ] Run Irssi and save a complete transcript of the connection and commands.
- [ ] Run Valgrind with descriptor tracking.
- [ ] Run ASan and UBSan.
- [ ] Confirm there are no leaks, double closes, dangling references or empty channels.

Reference: [phase 19](phases/phase_19.md).

## 9. Delivery and documentation

- [ ] Create `PROTOCOL.md` with supported commands, registration, format, numerics, channels, modes and an Irssi transcript.
- [ ] Add the real run command to the README.
- [ ] Add an explicit description of AI use to the README, required by the subject.
- [ ] Add classic technical references and fix README typos.
- [ ] Remove `-fsanitize=address` from the final build and keep it in a debug target.
- [ ] Verify a clean build, `clean`, `fclean`, `re`, no unnecessary relink and final C++98 compatibility.

References: [subject](en.subject.pdf), [README](../README.md) and [Makefile](../Makefile).

## 10. Bonus

Only after completing and verifying the entire mandatory part.

- [x] Implement DCC transfer: forward CTCP exactly through `PRIVMSG`; the file bytes travel between clients.
- [x] Implement a built-in bot as an IRC user without a socket or through a separate abstraction.
- [x] Add specific DCC and bot tests.

Reference: [architecture](architecture.md).
