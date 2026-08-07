# To-do list

Status legend: [x] implemented, [~] partially implemented, [ ] pending

## Project foundation
- [x] Review the IRC protocol and choose a reference client for test cases (the docs describe the expected flow, but no real-client test run is recorded)
- [x] Define the supported IRC commands and the internal message format (basic command structures and dispatcher entries exist)
- [x] Define the numeric replies that will be used for registration and errors (documented in the project notes, but not fully implemented in code)
- [x] Implement the server entry point with argument validation and password handling

## Server skeleton
- [x] Create the main server object and initialize its core components
- [x] Handle shutdown cleanly through signal management
- [x] Ensure file descriptors are closed correctly on exit

## TCP networking
- [x] Create the listening socket with IPv4/TCP settings
- [x] Enable socket options such as SO_REUSEADDR
- [x] Make the socket non-blocking and bind it to the chosen port
- [x] Put the socket into listening mode and verify TCP connections work
- [x] Build the main event loop around a single poll() call
- [x] Accept new clients and manage multiple connections concurrently

## Client model and state
- [x] Create a Client abstraction with socket, buffers, identity, and registration state
- [x] Track password acceptance, nickname, username, and registration status
- [x] Handle client disconnects and remove stale descriptors safely

## IRC message handling
- [x] Implement a persistent input buffer per client
- [x] Reconstruct complete IRC lines from fragmented TCP data
- [ ] Implement a real IRC message parser and integrate it with command dispatching
- [ ] Preserve trailing parameters exactly, including spaces and CTCP payloads

## Command execution
- [~] Implement command dispatching for PASS, NICK, USER, PING, PONG, QUIT, and CAP
  - PASS, NICK, and USER handlers exist, but they are still basic stubs
  - PING, PONG, QUIT, and CAP are not implemented yet
- [~] Handle registration flow so PASS, NICK, and USER complete the connection properly
  - the server can queue a welcome message when registration is ready, but validation remains minimal
- [ ] Send the welcome message and appropriate numeric replies once registration succeeds
- [ ] Return clear error replies for invalid or missing parameters

## Output management
- [x] Implement an output buffer for each client
- [x] Write responses asynchronously without blocking the server
- [x] Avoid busy polling by disabling POLLOUT when the output buffer is empty

## Channel support
- [~] Create a Channel model with members, operators, topic, key, and modes
  - the header exists, but the channel logic is not fully implemented or wired into the server
- [ ] Implement JOIN, PART, TOPIC, MODE, INVITE, and KICK behavior
- [ ] Handle channel creation, membership updates, and operator assignment
- [ ] Notify channel members when users join, leave, or are kicked

## Cleanup and robustness
- [~] Remove disconnected clients from channels and global indexes
  - client removal is implemented, but channel/member cleanup is not yet complete
- [ ] Delete empty channels when appropriate
- [ ] Ensure no stale pointers remain after client removal
- [ ] Test basic IRC flows with a real client such as HexChat, Irssi, or WeeChat
