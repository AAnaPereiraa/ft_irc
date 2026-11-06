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
