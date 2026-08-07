# To-do list

## Project foundation
- [ ] Review the IRC protocol and choose a reference client for test cases
- [ ] Define the supported IRC commands and the internal message format
- [ ] Define the numeric replies that will be used for registration and errors
- [ ] Implement the server entry point with argument validation and password handling

## Server skeleton
- [ ] Create the main server object and initialize its core components
- [ ] Handle shutdown cleanly through signal management
- [ ] Ensure all file descriptors are closed correctly on exit

## TCP networking
- [ ] Create the listening socket with IPv4/TCP settings
- [ ] Enable socket options such as SO_REUSEADDR
- [ ] Make the socket non-blocking and bind it to the chosen port
- [ ] Put the socket into listening mode and verify TCP connections work
- [ ] Build the main event loop around a single poll() call
- [ ] Accept new clients and manage multiple connections concurrently

## Client model and state
- [ ] Create a Client abstraction with socket, buffers, identity, and registration state
- [ ] Track password acceptance, nickname, username, and registration status
- [ ] Handle client disconnects and remove stale descriptors safely

## IRC message handling
- [ ] Implement a persistent input buffer per client
- [ ] Reconstruct complete IRC lines from fragmented TCP data
- [ ] Parse IRC commands into a structured representation with parameters and trailing text
- [ ] Preserve trailing parameters exactly, including spaces and CTCP payloads

## Command execution
- [ ] Implement command dispatching for PASS, NICK, USER, PING, PONG, QUIT, and CAP
- [ ] Handle registration flow so PASS, NICK, and USER complete the connection properly
- [ ] Send the welcome message and appropriate numeric replies once registration succeeds
- [ ] Return clear error replies for invalid or missing parameters

## Output management
- [ ] Implement an output buffer for each client
- [ ] Write responses asynchronously without blocking the server
- [ ] Avoid busy polling by disabling POLLOUT when the output buffer is empty

## Channel support
- [ ] Create a Channel model with members, operators, topic, key, and modes
- [ ] Implement JOIN, PART, TOPIC, MODE, INVITE, and KICK behavior
- [ ] Handle channel creation, membership updates, and operator assignment
- [ ] Notify channel members when users join, leave, or are kicked

## Cleanup and robustness
- [ ] Remove disconnected clients from channels and global indexes
- [ ] Delete empty channels when appropriate
- [ ] Ensure no stale pointers remain after client removal
- [ ] Test basic IRC flows with a real client such as HexChat, Irssi, or WeeChat
