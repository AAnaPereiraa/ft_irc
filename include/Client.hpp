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
};

#endif
