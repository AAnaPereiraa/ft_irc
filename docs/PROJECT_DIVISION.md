# Project Division

## Ana (`ana-pdos`)

- Argument validation
- Server socket setup
- Non-blocking sockets
- `poll()` event loop
- Accepting and disconnecting clients
- Basic client storage
- Input and output buffering

## Sami (`samalves`)

- IRC command parser
- `PASS`, `NICK`, and `USER`
- Client registration state
- `PRIVMSG`
- Channel storage
- `JOIN` and `PART`
- `KICK`
- `INVITE`
- `TOPIC`
- `MODE`

## Shared Integration Work

- Agree on class interfaces before implementation.
- Integrate the server and command-handling components.
- Test fragmented messages and multiple clients.
- Review each other's pull requests.
- Investigate memory leaks, crashes, and protocol errors.

## GitHub Issues

### Ana's Issues

- [ ] **Validate command-line port and password arguments**
  - Validate that the program receives exactly `<port> <password>`.
  - Reject an empty password, non-digit ports, overflow, and ports outside `1–65535`.
  - Store the validated values and report clear errors without crashing.

- [ ] **Create and configure the listening socket**
  - Create an IPv4 or IPv6 TCP socket.
  - Configure address reuse with `setsockopt()`.
  - Bind the socket to the requested port and call `listen()`.
  - Report system-call failures and close resources during shutdown.

- [ ] **Make file descriptors non-blocking**
  - Configure the listening socket and every accepted client socket as non-blocking.
  - Use only the subject-approved `fcntl()` usage on macOS.
  - Ensure no blocking read, write, accept, or connection operation can stall the server.

- [ ] **Implement the single `poll()` event loop**
  - Maintain one poll set containing the listening socket and all clients.
  - Handle readable, writable, hang-up, and error events.
  - Call `recv()` and `send()` only after the corresponding readiness event.
  - Avoid using `errno` after socket I/O to drive another read or write attempt.

- [ ] **Accept new clients and manage client connections**
  - Accept every pending connection reported by the listening socket.
  - Create a `Client` object and add its descriptor to the poll set.
  - Track each client by descriptor and remove it safely when necessary.

- [ ] **Handle client disconnections and cleanup**
  - Detect `recv()` returning zero and poll error/hang-up events.
  - Remove the client from all channels and notify affected users.
  - Close the descriptor and release all associated memory without leaks.

- [ ] **Implement client input buffering**
  - Preserve data between poll iterations because TCP can split messages.
  - Append received bytes to a per-client buffer.
  - Extract complete IRC lines ending in `\r\n`, process multiple commands, and retain incomplete data.

- [ ] **Implement client output buffering**
  - Queue responses instead of assuming one `send()` transmits everything.
  - Enable `POLLOUT` only while a client has pending output.
  - Remove only successfully sent bytes and retain the remainder for later events.

### Sami's Issues

- [ ] **Define and implement IRC command parsing**
  - Parse complete IRC lines into an optional prefix, command, parameters, and trailing parameter.
  - Support commands with spaces in the trailing parameter.
  - Normalize command names and return protocol errors for malformed or unknown commands.

- [ ] **Implement `PASS`, `NICK`, and `USER`**
  - Validate the password against the server password.
  - Validate nickname syntax and reject nicknames already in use.
  - Store username and real name from `USER`.
  - Return the required IRC error numerics for missing or invalid parameters.

- [ ] **Implement client registration state**
  - Track whether each client has passed authentication and supplied nickname and user information.
  - Send welcome numerics only after all required registration steps succeed.
  - Reject channel and messaging commands from unregistered clients.

- [ ] **Implement private messages with `PRIVMSG`**
  - Send a message to a registered nickname.
  - Validate targets and non-empty trailing text.
  - Return errors for missing or unknown targets.
  - Preserve the sender prefix in forwarded messages.

- [ ] **Define channel and membership storage**
  - Store channels, members, operators, topics, invitations, and channel modes.
  - Provide safe lookup and membership operations.
  - Define ownership and cleanup rules for empty channels.

- [ ] **Implement `JOIN` and `PART`**
  - Create channels when appropriate and add or remove members.
  - Enforce invite-only mode, channel keys, and user limits.
  - Notify channel members and send the current topic after a successful join.

- [ ] **Implement `KICK`**
  - Require the sender to be a channel operator.
  - Validate the channel and target membership.
  - Remove the target and broadcast the kick message with an optional reason.

- [ ] **Implement `INVITE`**
  - Require operator privileges where appropriate.
  - Validate the target nickname and channel.
  - Store an invitation that allows the target to join an invite-only channel.

- [ ] **Implement `TOPIC`**
  - Return the current topic when no new topic is supplied.
  - Allow topic changes according to the channel's `t` mode.
  - Broadcast successful topic changes to channel members.

- [ ] **Implement channel `MODE` options: `i`, `t`, `k`, `o`, and `l`**
  - Restrict mode changes to channel operators.
  - Support adding and removing invite-only, topic restriction, key, operator, and user-limit modes.
  - Validate required mode parameters and broadcast accepted changes.

### Shared Issues

- [ ] **Agree on interfaces between `Server`, `Client`, `Channel`, and command handling**
  - Define constructors, ownership rules, lookup methods, and message-delivery interfaces before parallel implementation.
  - Document which component owns sockets, clients, channels, and command execution.
  - Keep the interfaces compatible with C++98.

- [ ] **Integrate command handling with the server event loop**
  - Connect parsed commands to registered clients, channels, and server state.
  - Ensure responses are queued through the output-buffer mechanism.
  - Resolve integration conflicts through small reviewed pull requests.

- [ ] **Test fragmented TCP messages and multiple commands in one packet**
  - Verify commands split across multiple `recv()` calls are reconstructed.
  - Verify multiple `\r\n`-terminated commands in one packet are processed in order.
  - Test incomplete, malformed, and unusually long input safely.

- [ ] **Test multiple clients, slow clients, and unexpected disconnections**
  - Connect several clients simultaneously and exchange channel and private messages.
  - Verify that one slow client does not block other clients.
  - Disconnect clients during registration, messaging, and channel membership.

- [ ] **Check memory leaks and crash resistance**
  - Run invalid input, repeated connections, and abrupt disconnect tests.
  - Use a leak detector such as Valgrind where available.
  - Confirm that the server remains running and cleans up all resources.

- [ ] **Verify behavior with a reference IRC client**
  - Connect using the selected reference client.
  - Test authentication, registration, channels, messages, and operator commands.
  - Record any compatibility issues and resolve them before final submission.
