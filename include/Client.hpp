#ifndef CLIENT_HPP
# define CLIENT_HPP

// One connected client. The Server owns every Client: it creates one per
// accepted connection and deletes it (and closes the fd) on disconnect.
class Client
{
	private:
		int	_fd;

		// Not copyable: a copy would share the same socket fd.
		Client(const Client &other);
		Client &operator=(const Client &other);

	public:
		explicit Client(int fd);
		~Client();

		int	getFd() const;
};

#endif
