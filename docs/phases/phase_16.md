# Phase 16 — KICK

## Goal

Implement the `KICK` command, which lets an operator remove a user from a channel.

## Command format

```text
KICK #general roxana :Reason for the kick
```

The command receives:

1. The channel name.
2. The target user’s nickname.
3. An optional reason.

If no reason is provided, the operator’s nickname can be used as the default reason.

## Execution flow

```text
KICK #general roxana :Reason
        ↓
Are the channel and the target user present?
        ↓
Does the channel exist?
        ↓
Does the target user exist?
        ↓
Does the sender belong to the channel?
        ↓
Is the sender a channel operator?
        ↓
Does the target belong to the channel?
        ↓
Notify the KICK to every member
        ↓
Remove the target from the channel
        ↓
Remove their operator privileges
        ↓
Has the channel become empty?
    ├── yes → delete the channel
    └── no → keep the channel
```

## Required checks

The server must check:

1. That the mandatory parameters have been received.
2. That the channel exists.
3. That the target user exists.
4. That the user who runs `KICK` belongs to the channel.
5. That the sender is a channel operator.
6. That the target user belongs to the channel.

The checks must be performed before modifying the channel state.

## KICK notification

If every check is correct, the server must build a message with the operator’s full prefix:

```text
:operator!username@hostname KICK #general roxana :Reason
```

This message must be sent to every current channel member, including the kicked user.

The notification must be performed before removing the target to guarantee that they also receive the message.

## Updating the channel

After sending the notification:

1. Remove the target user from the member collection.
2. Remove them from the operator collection, if they had that privilege.
3. Remove any other user information associated with the channel.
4. Check whether the channel has become empty.
5. Delete the channel from the server if it no longer has members.

Kicking a user from a channel must not close their connection to the server or remove them from other channels.

## Relevant errors

```text
401 ERR_NOSUCHNICK
```

The target user does not exist on the server.

```text
403 ERR_NOSUCHCHANNEL
```

The indicated channel does not exist.

```text
441 ERR_USERNOTINCHANNEL
```

The target user does not belong to the channel.

```text
442 ERR_NOTONCHANNEL
```

The sender does not belong to the channel.

```text
461 ERR_NEEDMOREPARAMS
```

The channel or the target user’s nickname is missing.

```text
482 ERR_CHANOPRIVSNEEDED
```

The sender belongs to the channel, but is not an operator.

## Complete example

Received command:

```text
KICK #general roxana :Inappropriate behaviour
```

Message sent to the channel members:

```text
:admin!admin@localhost KICK #general roxana :Inappropriate behaviour
```

After sending the message:

```text
Members before:     admin, roxana, user2
Operators before:   admin, roxana

Members after:      admin, user2
Operators after:    admin
```

## Expected result

At the end of this phase, the server must be able to:

- Interpret the `KICK` command correctly.
- Validate the permissions of the user requesting the kick.
- Check that the channel and the target user are valid.
- Notify the kick to every current member.
- Remove the user from the channel’s members and operators.
- Keep the kicked user’s connection open.
- Delete the channel when it becomes completely empty.
- Reply with consistent numeric codes when the command cannot be executed.
