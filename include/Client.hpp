#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <string>

/*
 * This class currently stores IRC registration state only.
 * Coordinate with Ana before adding file descriptors or I/O buffers here.
 *
 * Integration dependencies:
 * - The input layer must provide one complete IRC line, with its line
 *   terminator removed, before parsing and dispatching it with this Client.
 * - Registration handlers must queue replies through the transport layer's
 *   per-client output mechanism; they must not call send() directly.
 * - Nickname uniqueness requires lookup across connected clients, and
 *   disconnect cleanup must release the client's nickname.
 */
class Client
{
    private:
        std::string _nickname;
        std::string _username;
        std::string _realName;
        bool        _passwordAccepted;
        bool        _hasUserInfo;
        bool        _registered;

        public:
            Client();
            bool                hasAcceptedPassword() const;
// Call only after the server handler verifies PASS against the server password.
            void                markPasswordAccepted();

            bool                hasNickname() const;
            const std::string   &getNickname() const;
// The caller must validate nickname syntax and uniqueness before updating state.
// A registered nickname change also requires lookup updates and notification.
            void                setNickname(const std::string &nickname);

            bool                hasUserInfo() const;
            const std::string   &getUsername() const;
            const std::string   &getRealName() const;
// Call only after validating the initial USER command and its required parameters.
            void                setUserInfo(const std::string &username, const std::string &realName);

            bool                isRegistered() const;
// Call after each successful registration command. Returns true only once,
// when this project’s password, nickname, and USER requirements are all met.
// The caller should then queue the welcome numeric exactly once.
            bool                tryCompleteRegistration();
};

#endif
