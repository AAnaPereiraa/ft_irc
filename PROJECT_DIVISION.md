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

- [ ] Validate command-line port and password arguments.
- [ ] Create and configure the listening socket.
- [ ] Make file descriptors non-blocking.
- [ ] Implement the single `poll()` event loop.
- [ ] Accept new clients and manage client connections.
- [ ] Handle client disconnections and cleanup.
- [ ] Implement client input buffering.
- [ ] Implement client output buffering.

### Sami's Issues

- [ ] Define and implement IRC command parsing.
- [ ] Implement `PASS`, `NICK`, and `USER`.
- [ ] Implement client registration state.
- [ ] Implement private messages with `PRIVMSG`.
- [ ] Define channel and membership storage.
- [ ] Implement `JOIN` and `PART`.
- [ ] Implement `KICK`.
- [ ] Implement `INVITE`.
- [ ] Implement `TOPIC`.
- [ ] Implement channel `MODE` options: `i`, `t`, `k`, `o`, and `l`.

### Shared Issues

- [ ] Agree on interfaces between `Server`, `Client`, `Channel`, and command handling.
- [ ] Integrate command handling with the server event loop.
- [ ] Test fragmented TCP messages and multiple commands in one packet.
- [ ] Test multiple clients, slow clients, and unexpected disconnections.
- [ ] Check memory leaks and crash resistance.
- [ ] Verify behavior with a reference IRC client.
