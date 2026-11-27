#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <string>

// One connected client. The Server owns every Client: it creates one per
// accepted connection and deletes it (and closes the fd) on disconnect.
class Client
{
	private:
		int			_fd;

		// Bytes received but not yet processed. TCP is a byte stream, so one
		// recv() may hold half a message, several messages, or both. Data
		// stays here across poll() iterations until a full line has arrived.
		std::string	_inBuffer;

		// Bytes queued for this client but not yet accepted by send().
		// send() may take only part of what we give it (the kernel's socket
		// buffer is full), so the rest waits here for the next POLLOUT.
		std::string	_outBuffer;

		std::string	_nickname;
		std::string	_username;
		std::string	_realName;
		bool		_passwordAccepted;
		bool		_hasUserInfo;
		bool		_registered;

		// Not copyable: a copy would share the same socket fd.
		Client(const Client &other);
		Client &operator=(const Client &other);

	public:
		explicit Client(int fd);
		~Client();

		int		getFd() const;

		// Appends raw bytes from recv() to the input buffer.
		void	appendToBuffer(const char *data, size_t len);

		// If the buffer holds a complete line (ending in "\n" or "\r\n"),
		// moves it into `line` without its terminator, removes it from the
		// buffer and returns true. Otherwise leaves everything untouched
		// and returns false.
		bool	extractLine(std::string &line);

		// Bytes still waiting for their line terminator.
		size_t	bufferSize() const;

		// Queues one IRC message. `msg` is given WITHOUT "\r\n": it is
		// appended here so no reply can ever forget its terminator.
		void				queueMessage(const std::string &msg);

		bool				hasPendingOutput() const;
		const std::string	&getOutBuffer() const;

		// Removes the first `count` bytes, i.e. the ones send() accepted.
		void				consumeOutput(size_t count);

		bool				hasAcceptedPassword() const;
		void				markPasswordAccepted();
		void				clearPasswordAccepted();
		bool				hasNickname() const;
		const std::string	&getNickname() const;
		void				setNickname(const std::string &nickname);
		bool				hasUserInfo() const;
		const std::string	&getUsername() const;
		const std::string	&getRealName() const;
		void				setUserInfo(const std::string &username,
								const std::string &realName);
		bool				isRegistered() const;
		bool				tryCompleteRegistration();
};

#endif
