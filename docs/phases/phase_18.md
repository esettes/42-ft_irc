# Phase 18 — State cleanup and disconnections

## Goal

Implement a safe and complete removal of disconnected clients.

This phase is critical because a client may be referenced from several server structures:

- List used by `poll()`.
- Map of connected clients.
- Global nickname index.
- Channels they belong to.
- Operator lists.
- Invite lists.
- Input and output buffers.

An incomplete disconnection can leave invalid references, cause inconsistent behaviour or produce memory errors.

---

## Central disconnection function

Every disconnection must go through a single function:

```cpp
void Server::disconnectClient(
    int clientFileDescriptor,
    const std::string &reason
);
```

This function must be responsible for completely removing the client from the server.

It must be used when:

- The client sends `QUIT`.
- `recv()` returns `0`.
- `recv()` returns an unrecoverable error.
- A disconnection is detected through `poll()`.
- The server needs to close the connection explicitly.

This logic must not be duplicated in different handlers.

---

## Recommended flow

```text
Locate the client
        ↓
Build the QUIT message
        ↓
Obtain the clients that must receive it
        ↓
Remove the client from every channel
        ↓
Remove them from operators and invitees
        ↓
Delete empty channels
        ↓
Remove their nickname from the global index
        ↓
Remove them from poll()
        ↓
Close their descriptor
        ↓
Remove them from the client map
```

---

## 1. Locate the client

Before starting, it must be checked that the descriptor corresponds to an existing client.

```cpp
std::map<int, Client>::iterator clientIterator;

clientIterator = clients.find(clientFileDescriptor);
if (clientIterator == clients.end())
{
    return;
}
```

The function must tolerate an attempt to disconnect a descriptor that has already been removed, avoiding double closes and invalid accesses.

---

## 2. Build the `QUIT` message

If the client was registered, the message that the other users will receive must be built:

```text
:nickname!username@hostname QUIT :Connection closed
```

If the disconnection comes from the command:

```text
QUIT :Leaving
```

The reason provided by the user must be kept:

```text
:nickname!username@hostname QUIT :Leaving
```

The message must be built before destroying the `Client` object, because afterwards its nickname, username and hostname will no longer be available.

---

## 3. Determine who must receive the `QUIT`

The `QUIT` message must be sent to the users who share at least one channel with the disconnected client.

The same user may share several channels with them, but must receive the message only once.

To avoid duplicates, it is useful to collect the recipients in a `std::set`:

```cpp
std::set<int> recipientFileDescriptors;
```

For each channel the client belongs to:

1. Traverse its members.
2. Exclude the client who is being disconnected.
3. Add each member’s descriptor to the set.

Then the `QUIT` message is sent to every collected recipient.

---

## 4. Remove the client from every channel

The client must disappear from every internal collection of each channel:

- Members.
- Operators.
- Invitees.

After the removal these conditions must always be met:

- No operator can exist if they are not a channel member.
- No invitation must point to a nonexistent client.
- No channel must keep references to the disconnected client.

It can be useful to implement a helper function:

```cpp
void Channel::removeClient(int clientFileDescriptor);
```

This function can take care of removing the client from:

- `members`.
- `operators`.
- `invitedClients`.

---

## 5. Delete empty channels

After removing the client, each channel must be checked:

```cpp
if (channel.isEmpty())
{
    // Remove the channel from the server.
}
```

Channels that are left without members must be deleted from the server’s global map.

Elements of a container must not be erased while it is being traversed incorrectly, because that can invalidate the iterator.

A safe way is to save the next iterator before erasing:

```cpp
std::map<std::string, Channel>::iterator channelIterator;
std::map<std::string, Channel>::iterator nextChannelIterator;

channelIterator = channels.begin();
while (channelIterator != channels.end())
{
    nextChannelIterator = channelIterator;
    ++nextChannelIterator;

    channelIterator->second.removeClient(clientFileDescriptor);

    if (channelIterator->second.isEmpty())
    {
        channels.erase(channelIterator);
    }

    channelIterator = nextChannelIterator;
}
```

Another option is to first collect the names of the empty channels and delete them afterwards.

---

## 6. Remove the nickname

If there is a global index such as:

```cpp
std::map<std::string, int> nicknameIndex;
```

The entry corresponding to the client’s nickname must be removed:

```cpp
nicknameIndex.erase(client.getNickname());
```

The removal must be performed before destroying the `Client` object.

If the client had not sent `NICK` yet, there will be no entry to remove.

---

## 7. Remove the descriptor from `poll()`

The descriptor must be removed from the container of `pollfd` structures.

If a `std::vector<pollfd>` is used, the structure whose `fd` field matches the client’s descriptor must be found.

```cpp
for (std::vector<pollfd>::iterator iterator = pollFileDescriptors.begin();
     iterator != pollFileDescriptors.end();
     ++iterator)
{
    if (iterator->fd == clientFileDescriptor)
    {
        pollFileDescriptors.erase(iterator);
        break;
    }
}
```

After removing it, the server must not process events associated with that descriptor again during the same loop iteration.

---

## 8. Close the descriptor

Once it has been removed from the server structures, the socket must be closed:

```cpp
::close(clientFileDescriptor);
```

The descriptor must be closed only once.

After calling `close()`, it must not be used again to:

- Read data.
- Send messages.
- Look up the client.
- Modify `poll()` events.
- Access buffers.

---

## 9. Remove the client from the map

Finally, the client can be removed from the main container:

```cpp
clients.erase(clientFileDescriptor);
```

This operation must be performed after using all of the object’s necessary information, such as:

- Nickname.
- Username.
- Hostname.
- Channels.
- IRC prefix.

After `erase()`, any reference, pointer or iterator to the client is no longer valid.

---

## Handling `recv()`

When `recv()` returns `0`, it means the client has closed the connection:

```cpp
ssize_t receivedBytes;

receivedBytes = recv(
    clientFileDescriptor,
    buffer,
    sizeof(buffer),
    0
);

if (receivedBytes == 0)
{
    disconnectClient(
        clientFileDescriptor,
        "Connection closed"
    );
    return;
}
```

If `recv()` returns `-1`, `errno` must be checked.

Temporary errors must not disconnect the client:

- `EAGAIN`.
- `EWOULDBLOCK`.
- `EINTR`.

Other errors can be treated as a disconnection:

```cpp
if (receivedBytes == -1)
{
    if (errno == EAGAIN || errno == EWOULDBLOCK)
    {
        return;
    }

    if (errno == EINTR)
    {
        return;
    }

    disconnectClient(
        clientFileDescriptor,
        "Connection error"
    );
    return;
}
```

---

## Handling `poll()` events

Events that can indicate a closed or invalid connection include:

- `POLLHUP`.
- `POLLERR`.
- `POLLNVAL`.

Example:

```cpp
if (pollFileDescriptor.revents & (POLLHUP | POLLERR | POLLNVAL))
{
    disconnectClient(
        pollFileDescriptor.fd,
        "Connection closed"
    );
    continue;
}
```

The same client must not continue to be processed after calling `disconnectClient()`.

---

## Integration with `QUIT`

The `QUIT` handler must not manually implement the entire cleanup.

Its responsibility must be limited to:

1. Obtaining the optional reason.
2. Calling `disconnectClient()`.
3. Immediately finishing processing of the client.

```cpp
void Server::handleQuit(
    Client &client,
    const IrcMessage &message
)
{
    std::string reason = "Client Quit";

    if (!message.parameters.empty())
    {
        reason = message.parameters[0];
    }

    disconnectClient(
        client.getFileDescriptor(),
        reason
    );
}
```

After calling `disconnectClient()`, the handler must not access `client` again, because the reference may have been invalidated.

---

## Difference between `QUIT` and `KICK`

`QUIT` completely disconnects the client from the server.

`KICK` only removes a user from a channel:

```text
KICK #general roxana :Reason
```

Therefore `KICK` must not call `disconnectClient()`.

It can reuse a helper function for channel removal:

```cpp
void Server::removeClientFromChannel(
    Client &client,
    Channel &channel
);
```

This function must:

- Remove the client from the channel members.
- Remove their operator privileges on that channel.
- Remove them from the invite list if appropriate.
- Delete the channel if it becomes empty.

The client must stay connected to the server and can keep using other channels.

---

## Avoid invalidated iterators

Disconnection can modify containers that are being traversed, especially:

- The `pollfd` vector.
- The client map.
- The channel map.
- The member collections.
- The operator collections.
- The invite collections.

An iterator must never be incremented or used after its element has been removed.

When erasing during a traversal, a safe strategy must be used:

- Save the next iterator before erasing.
- Use the iterator returned by `erase()` if the available standard allows it.
- First store the elements that must be removed and erase them afterwards.

In C++98, saving the next iterator before calling `erase()` is usually the simplest and most portable option.

---

## Avoid dangling pointers and references

If channels store pointers or references to `Client` objects, they must be removed before deleting the client from the main map.

Mandatory order:

```text
Remove references from channels
        ↓
Delete the Client object
```

This must never be done:

```text
Delete the Client object
        ↓
Try to remove it from the channels
```

The second sequence would require accessing an object that no longer exists.

A more robust alternative is for channels to store stable identifiers, such as descriptors, instead of direct pointers.

---

## Disconnections during the event loop

If the server traverses the `pollfd` vector by indexes and removes an element, later positions shift.

For example:

```text
Before:  [server][client A][client B][client C]
Erase:                   [client B]
After:   [server][client A][client C]
```

If the index is incremented immediately, `client C` might not be processed.

Possible solutions are:

- Do not increment the index when the current element is removed.
- Traverse the vector in reverse order.
- Mark the clients that must be disconnected and remove them afterwards.
- Have `disconnectClient()` indicate whether the vector has changed.

---

## Invariants that must be kept

After each disconnection these rules must be met:

- Every descriptor present in `poll()` corresponds to a valid connection.
- Every connected client appears only once in the client map.
- Every registered nickname belongs to an existing client.
- Every channel member belongs to a connected client.
- Every operator is also a channel member.
- Every invitation belongs to a connected client.
- No empty channel remains stored.
- No descriptor is closed more than once.
- No recipient receives the same `QUIT` message twice.

---

## Recommended test cases

### Normal disconnection

```text
QUIT :Leaving
```

Check that:

- The other users receive `QUIT`.
- The socket is closed.
- The client disappears from `poll()`.
- The nickname becomes available.
- The client disappears from all of their channels.

### Unexpected close

Close `netcat` or the IRC client without sending `QUIT`.

Check that:

- `recv()` returns `0`.
- The same cleanup is executed.
- The other members receive the notification.
- No references to the client remain.

### User present in several channels

Add a user to several channels and disconnect them.

Check that:

- They disappear from every channel.
- They disappear from every operator list.
- Users who shared several channels receive a single `QUIT`.

### Last member of a channel

Disconnect the only member.

Check that:

- The channel becomes empty.
- The channel is deleted from the server.

### Disconnected operator

Disconnect an operator.

Check that:

- They are removed from `operators`.
- They are not left registered as an operator after leaving the channel.
- The channel keeps working if it still has members.

### Invited user

Invite a user and disconnect them before they `JOIN`.

Check that:

- They are removed from `invitedClients`.
- No invalid reference remains.

### Double disconnection

Try to disconnect the same descriptor twice.

Check that:

- A double `close()` does not occur.
- A nonexistent client is not accessed.
- The server keeps working.

### Several clients disconnected during the same `poll()`

Close several connections almost simultaneously.

Check that:

- No event is skipped because of the vector shift.
- Every client is removed correctly.
- Indexes or iterators are not invalidated.

---

## Expected result

At the end of this phase, any kind of disconnection must leave the server in a completely consistent state:

```text
Client disconnected
        ↓
No descriptor in poll()
No entry in clients
No reserved nickname
No channel membership
No operator privileges
No pending invitations
No empty channels
No dangling references
```

The main rule of this phase is:

> Every complete disconnection must be executed through `Server::disconnectClient()`, and no function must continue using the client after calling it.
