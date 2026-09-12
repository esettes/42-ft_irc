# Phase 15 — Implementing `INVITE`

## Goal

Implement the `INVITE` command, which lets you invite a user to a channel.

The invitation is especially important for channels that have mode `+i` enabled, since only invited users will be able to enter them.

---

## Command syntax

```irc
INVITE <nickname> <channel>
```

Example:

```irc
INVITE roxana #private
```

In this example, the user who runs the command invites `roxana` to channel `#private`.

---

## Required state in `Channel`

Each channel must keep a collection of invited users.

A possible representation would be:

```cpp
std::set<Client *> invitedClients;
```

A stable client identifier, such as its file descriptor, can also be used:

```cpp
std::set<int> invitedClientDescriptors;
```

Complete copies of the clients should not be stored.

The `Channel` class should provide methods similar to the following:

```cpp
void inviteClient(Client &client);
bool isClientInvited(const Client &client) const;
void removeInvitation(const Client &client);
```

Using a `std::set` avoids storing the same invitation several times.

---

## Initial checks

Before processing the invitation, the server must check:

1. That the sending client is registered.
2. That the nickname and the channel have been received.
3. That the channel exists.
4. That the target user exists.
5. That the sender belongs to the channel.
6. That the target user does not already belong to the channel.
7. That the sender has permission to invite.
8. That the target user is not already invited, if this case is to be controlled explicitly.

---

## Recommended validation order

The command flow can follow this order:

```text
Is the client registered?
    ↓
Were nickname and channel received?
    ↓
Does the channel exist?
    ↓
Does the target user exist?
    ↓
Does the sender belong to the channel?
    ↓
Does the target already belong to the channel?
    ↓
Does the sender have permissions?
    ↓
Add the target to the invited collection
    ↓
Confirm the invitation to the sender
    ↓
Notify the invitation to the target user
```

Always keeping the same validation order makes errors predictable and makes tests easier.

---

## Permissions to invite

At a minimum, when the channel has mode `+i` enabled, only an operator should be able to invite users.

For `ft_irc`, the safest option is to treat `INVITE` as an operator action and require the sender to be a channel operator.

```cpp
if (!channel.isOperator(client))
{
    // Send ERR_CHANOPRIVSNEEDED.
    return;
}
```

The chosen rule must be applied consistently throughout the server.

---

## Correct invitation

When every check is valid, the server must:

1. Add the target user to the channel’s invited collection.
2. Send `RPL_INVITING` to the user who ran the command.
3. Send an `INVITE` message to the invited user.

### Confirmation to the sender

The usual numeric reply is:

```irc
:irc.local 341 operator roxana #private
```

Code `341` corresponds to:

```text
RPL_INVITING
```

### Notification to the invited user

The target user must receive a message with the sender’s full prefix:

```irc
:operator!username@localhost INVITE roxana :#private
```

The invitation should not be notified to every channel member. Only these need to receive it:

- The user who sends the invitation, through `341 RPL_INVITING`.
- The invited user, through the `INVITE` message.

---

## Integration with `JOIN`

The `JOIN` implementation must check whether the user is invited when the channel has mode `+i` enabled.

A simplified check would be:

```cpp
if (channel.isInviteOnly()
    && !channel.isClientInvited(client))
{
    // Send ERR_INVITEONLYCHAN.
    return;
}
```

If the user appears in the invited collection, they can bypass the `+i` restriction.

The invitation must only avoid the error:

```text
473 ERR_INVITEONLYCHAN
```

It should not allow skipping other restrictions, unless that has been decided expressly.

For example, the invited user must still satisfy:

- The channel password set through `+k`.
- The user limit set through `+l`.

---

## Consuming the invitation

The invitation must be removed after the user enters the channel correctly:

```cpp
channel.addMember(client);
channel.removeInvitation(client);
```

It is important to remove it after completing `JOIN` correctly.

It must not be consumed if entry fails because of:

- An incorrect password.
- The user limit.
- Another validation error.

This way the user can correct the problem and try to enter again without needing a new invitation.

---

## Cleaning up invitations

References to an invited user must also be removed when:

- The user disconnects through `QUIT`.
- Their connection is closed unexpectedly.
- The channel is deleted.
- The user enters correctly and consumes the invitation.

This avoids invalid references to clients that no longer exist.

If invitations are stored by nickname, they will also need to be updated when the user changes nickname. For this reason, using a stable identifier is preferable.

---

## Relevant errors

### Unregistered client

```text
451 ERR_NOTREGISTERED
```

Used if the client tries to run `INVITE` before completing registration.

Example:

```irc
:irc.local 451 * :You have not registered
```

### Missing parameters

```text
461 ERR_NEEDMOREPARAMS
```

Used when the nickname or the channel is missing.

Incorrect examples:

```irc
INVITE
INVITE roxana
```

Reply:

```irc
:irc.local 461 operator INVITE :Not enough parameters
```

### Nonexistent user

```text
401 ERR_NOSUCHNICK
```

Used when the target nickname does not exist.

```irc
:irc.local 401 operator unknown :No such nick
```

### Nonexistent channel

```text
403 ERR_NOSUCHCHANNEL
```

Used when the specified channel does not exist.

```irc
:irc.local 403 operator #unknown :No such channel
```

### The sender does not belong to the channel

```text
442 ERR_NOTONCHANNEL
```

```irc
:irc.local 442 operator #private :You're not on that channel
```

### The target already belongs to the channel

```text
443 ERR_USERONCHANNEL
```

```irc
:irc.local 443 operator roxana #private :is already on channel
```

### The sender is not an operator

```text
482 ERR_CHANOPRIVSNEEDED
```

```irc
:irc.local 482 user #private :You're not channel operator
```

---

## Recommended functions

The logic can be split into small, clearly distinct functions:

```cpp
void Server::handleInvite(Client &client, const Command &command);
Client *Server::findClientByNickname(const std::string &nickname);
Channel *Server::findChannel(const std::string &channelName);
```

The `Channel` class can take care of managing invitation state:

```cpp
void Channel::inviteClient(Client &client);
bool Channel::isClientInvited(const Client &client) const;
void Channel::removeInvitation(const Client &client);
```

`Server` must validate the command and send the replies, while `Channel` must manage the channel’s internal state.

---

## Minimum test cases

### Valid invitation

1. Create a channel.
2. Make the sender an operator.
3. Invite another user.
4. Check that the sender receives `341 RPL_INVITING`.
5. Check that the target user receives the `INVITE` message.

### Entering a `+i` channel

1. Enable mode `+i`.
2. Try to enter without an invitation.
3. Check that `473 ERR_INVITEONLYCHAN` is received.
4. Invite the user.
5. Repeat the `JOIN`.
6. Check that they can now enter.

### Consuming the invitation

1. Invite a user.
2. Have them enter correctly.
3. Check that the invitation has been removed.
4. Have them leave the channel.
5. Try to enter again without a new invitation.
6. Check that they receive `473 ERR_INVITEONLYCHAN`.

### Invitation without permissions

1. Enter the channel with a user who is not an operator.
2. Try to invite another user.
3. Check that `482 ERR_CHANOPRIVSNEEDED` is received.
4. Check that the invitation is not stored.

### User already present

1. Invite a user who already belongs to the channel.
2. Check that `443 ERR_USERONCHANNEL` is received.
3. Check that no invitation is added.

### Additional restrictions

1. Enable `+i` and `+k`.
2. Invite a user.
3. Try to enter with an incorrect password.
4. Check that the `JOIN` fails.
5. Check that the invitation is not consumed.
6. Enter with the correct password.
7. Check that the invitation is removed after the `JOIN`.

---

## Expected result of the phase

At the end of this phase, the server must be able to:

- Process `INVITE <nickname> <channel>` correctly.
- Validate that the user and the channel exist.
- Check that the sender belongs to the channel.
- Check the sender’s permissions.
- Avoid inviting users who already belong to the channel.
- Store invitations without duplicates.
- Send `341 RPL_INVITING` to the sender.
- Notify the invitation to the target user.
- Let an invited user bypass mode `+i`.
- Consume the invitation only after a successful `JOIN`.
- Clean up invitations when a client disconnects.
- Reply with consistent numeric codes for each error.
