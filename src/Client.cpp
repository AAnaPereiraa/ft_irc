#include "Client.hpp"

Client::Client(int fd) : _fd(fd), _inBuffer(), _outBuffer()
{}

// The Server closes the fd, so the Client does not.
Client::~Client()
{}

int Client::getFd() const
{
	return _fd;
}

void Client::appendToBuffer(const char *data, size_t len)
{
	// (data, len) and not just data: recv() does not null-terminate, and
	// the bytes may contain '\0' -- the parser rejects those later.
	_inBuffer.append(data, len);
}

bool Client::extractLine(std::string &line)
{
	// Search for '\n' rather than "\r\n": the RFC requires "\r\n", but nc
	// and some clients send a bare "\n". Accepting both covers both cases.
	std::string::size_type newline = _inBuffer.find('\n');
	if (newline == std::string::npos)
		return false;	// incomplete: keep waiting for more bytes

	std::string::size_type end = newline;
	if (end > 0 && _inBuffer[end - 1] == '\r')
		--end;			// drop the '\r' of "\r\n"

	line = _inBuffer.substr(0, end);
	_inBuffer.erase(0, newline + 1);	// remove the line AND its '\n'
	return true;
}

size_t Client::bufferSize() const
{
	return _inBuffer.size();
}

void Client::queueMessage(const std::string &msg)
{
	_outBuffer += msg;
	_outBuffer += "\r\n";
}

bool Client::hasPendingOutput() const
{
	return !_outBuffer.empty();
}

const std::string &Client::getOutBuffer() const
{
	return _outBuffer;
}

void Client::consumeOutput(size_t count)
{
	// Only what was really sent is removed; the remainder stays queued
	// and goes out on a later POLLOUT, in the same order.
	_outBuffer.erase(0, count);
}
