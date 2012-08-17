#include "Debug.h"
#include "Socket.h"

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>

//////////////////////////////////////////////////////////////////

namespace gdb {

class TcpSocket: public Socket
{
	int sd;

	void
	close();

public:
	TcpSocket(int sd);

	virtual
	~TcpSocket();

	virtual int
	read(void* buf, size_t maxlen);

	virtual int
	write(const void* buf, size_t len);

	virtual int
	flush();

	virtual bool
	isReadable();
};

class StdioSocket: public Socket
{
public:
	StdioSocket();

	virtual
	~StdioSocket();

	virtual int
	read(void* buf, size_t maxlen);

	virtual int
	write(const void* buf, size_t len);

	virtual int
	flush();

	virtual bool
	isReadable();
};

//////////////////////////////////////////////////////////////////
//
//	class Socket
//

Socket::Socket()
{
}

Socket::~Socket()
{
}

#undef getc

int
Socket::getc()
{
	unsigned char c   = 0;
	int           ret = read(&c, 1);

	if(ret > 0) {
		return c & 0xff;
	}
	return -1;
}

#undef putc

int
Socket::putc(int ch)
{
	unsigned char c = ch & 0xff;
	return write(&c, 1);
}

int
Socket::printf(const string& fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int ret = vprintf(fmt, args);
	va_end(args);
	return ret;
}

int
Socket::vprintf(const string& fmt, va_list args)
{
	int    ret = -1;
	char*  buf = NULL;
	size_t len = ::vasprintf(&buf, fmt.c_str(), args);

	if(buf) {
		ret = write(buf, len);
		free(buf);
	}
	return ret;
}

Socket*
Socket::createInstance(const string& name, int sd)
{
	if(name == "tcp") {
		return new TcpSocket(sd);
	}
	if(name == "stdio") {
		return new StdioSocket();
	}
	return NULL;
}
//////////////////////////////////////////////////////////////////
//
//	class TcpSocket
//

TcpSocket::TcpSocket(int sd): sd(sd)
{
}

TcpSocket::~TcpSocket()
{
	close();
}

void
TcpSocket::close()
{
	if(sd >= 0) {
		::close(sd);
		sd = -1;
	}
}

int
TcpSocket::read(void* buf, size_t maxlen)
{
	if(sd < 0) {
		return -1;
	}
	return recv(sd, buf, maxlen, MSG_NOSIGNAL);
}

int
TcpSocket::write(const void* buf, size_t len)
{
	if(sd < 0) {
		return -1;
	}
	return send(sd, buf, len, MSG_NOSIGNAL);
}

int
TcpSocket::flush()
{
	if(sd < 0) {
		return -1;
	}
	return 0;
}

bool
TcpSocket::isReadable()
{
	if(sd < 0) {
		return false;
	}

	struct timeval tv = {0, 0};
	fd_set         readfds;

	FD_ZERO(&readfds);
	FD_SET(sd, &readfds);

	if(select(sd + 1, &readfds, NULL, NULL, &tv) > 0) {
		if(FD_ISSET(sd, &readfds)) {
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////
//
//	class StdioSocket
//

StdioSocket::StdioSocket()
{
}

StdioSocket::~StdioSocket()
{
}

int
StdioSocket::read(void* buf, size_t maxlen)
{
	return ::read(0, buf, maxlen);
}

int
StdioSocket::write(const void* buf, size_t len)
{
	return ::write(1, buf, len);
}

int
StdioSocket::flush()
{
	return 0;
}

bool
StdioSocket::isReadable()
{
	int            fd = 0;	//stdin
	struct timeval tv = {0, 0};
	fd_set         readfds;

	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);

	if(select(fd + 1, &readfds, NULL, NULL, &tv) > 0) {
		if(FD_ISSET(fd, &readfds)) {
			return true;
		}
	}
	return false;
}

}; //end of namespace gdb

