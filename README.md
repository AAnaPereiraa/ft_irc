*This project has been created as part of the 42 curriculum by AAnaPereiraa.*

# ft_irc

Internet Relay Chat or IRC is a text-based communication protocol on the Internet.
It offers real-time messaging that can be either public or private. Users can exchange
direct messages and join group channels.
IRC clients connect to IRC servers in order to join channels. IRC servers are connected
together to form a network.

## Description

This project implements an IRC server in C++98. The server accepts TCP connections,
authenticates clients with a password, and will support IRC users, channels, messages,
and operator commands required by the 42 `ft_irc` subject.

## Instructions

Build the project with:

```bash
make
```

Run the server with:

```bash
./ircserv <port> <password>
```

Clean generated build files with:

```bash
make clean
make fclean
```

## Collaboration workflow

The `main` branch is protected. Work on a feature branch, push it to GitHub, and open
a pull request. The other contributor should review and approve the pull request before
it is merged. Do not push directly to `main`.

```bash
git switch -c feature/short-description
git push -u origin feature/short-description
```

Pull requests must build successfully with `make re` and must not include generated
files such as `ircserv`, object files, or dependency files.

## Resources

- [RFC 2812: Internet Relay Chat: Client Protocol](https://www.rfc-editor.org/rfc/rfc2812)
- [Linux `poll` documentation](https://man7.org/linux/man-pages/man2/poll.2.html)
- [42 `ft_irc` subject](./en.subject.pdf)

AI was used to clarify the subject requirements, discuss implementation choices, and
review build configuration. All generated suggestions are reviewed, tested, and
understood by the project contributors.