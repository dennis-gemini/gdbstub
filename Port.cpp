#include "Debug.h"
#include "Port.h"

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>

#define GDB_DEFAULT_TCP_PORT	(1234)

namespace gdb {

class TcpPort: public Port
{
	int sd;

	void
	close();

public:
	TcpPort(const string& params);

	virtual
	~TcpPort();

	virtual Socket*
	accept();
};

class StdioPort: public Port
{
public:
	StdioPort();

	virtual
	~StdioPort();

	virtual Socket*
	accept();
};

//////////////////////////////////////////////////////////////////
//
//	class Port
//

Port*
Port::createInstance(const string& name, const string& params)
{
	if(name == "tcp") {
		return new TcpPort(params);
	}
	if(name == "stdio") {
		return new StdioPort();
	}
	return NULL;
}

Port::Port()
{
}

Port::~Port()
{
}

//////////////////////////////////////////////////////////////////
//
//	class TcpPort
//

TcpPort::TcpPort(const string& params)
{
	int                port = ::atoi(params.c_str());
	int                n    = 1;
	struct sockaddr_in addr;

	if(port == 0) {
		port = GDB_DEFAULT_TCP_PORT;
	}

	sd = ::socket(AF_INET, SOCK_STREAM, 0);

	if(sd < 0) {
		LOG("socket error: %m");
		goto failure;
	}
	::fcntl(sd, F_SETFD, FD_CLOEXEC);
	::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n));

	::memset(&addr, 0, sizeof(addr));
	addr.sin_family      = AF_INET;
	addr.sin_port        = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if(::bind(sd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
		LOG("bind error: %m");
		goto failure;
	}
	if(::listen(sd, 1) < 0) {
		LOG("listen error: %m\n");
		goto failure;
	}
	return;

failure:
	close();
}

TcpPort::~TcpPort()
{
	close();
}

void
TcpPort::close()
{
	if(sd >= 0) {
		::close(sd);
		sd = -1;
	}
}

Socket*
TcpPort::accept()
{
	struct sockaddr_in addr;
	socklen_t          n      = sizeof(addr);
	int                client = ::accept(sd, (struct sockaddr*) &addr, (socklen_t*) &n);

	if(client < 0) {
		LOG("accept error: %m");
		return NULL;
	}
	::fcntl(n, F_SETFD, FD_CLOEXEC);

	return Socket::createInstance("tcp", client);
}

//////////////////////////////////////////////////////////////////
//
//	class StdioPort
//

StdioPort::StdioPort()
{
}

StdioPort::~StdioPort()
{
}

Socket*
StdioPort::accept()
{
	return Socket::createInstance("stdio", -1);
}

}; //end of namespace gdb

