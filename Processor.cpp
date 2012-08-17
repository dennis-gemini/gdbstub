#include "Processor.h"
#include <stdio.h>
#include <string.h>

namespace gdb {

//////////////////////////////////////////////////////
Handler::Handler()
{
}

Handler::~Handler()
{
}

//////////////////////////////////////////////////////
Processor::Response::Response()
{
	handler = NULL;
}

Processor::Response::~Response()
{
}

//////////////////////////////////////////////////////
Processor::Processor()
{
}

Processor::~Processor()
{
}

void
Processor::defineResponseV(const string& cmd, const string& message, va_list args)
{
	char* buf = NULL;

	vasprintf(&buf, message.c_str(), args);

	if(buf) {
		responseMap[cmd].message = buf;
		responseMap[cmd].handler = NULL;
	}
}

void
Processor::defineResponse(const string& cmd, const string& message, ...)
{
	va_list args;
	va_start(args, message);
	defineResponseV(cmd, message, args);
	va_end(args);
}

void
Processor::defineResponse(const string& cmd, Handler* handler)
{
	responseMap[cmd].handler = handler;
	responseMap[cmd].message = "";
}

bool
Processor::getNextToken(const char* buf, const char* &sep, string& cmd, string& param) const
{
	static const char delimiters[] = ":;,";

	if(sep == NULL) {
		return false;
	}
	if(sep <= buf) {
		sep = NULL;
		return false;
	}

	int digits = 0;

	for(--sep; sep >= buf; sep--) {
		if(strchr(delimiters, *sep) != NULL) {
			cmd.assign(buf, sep - buf);
			param.assign(sep + 1);
			return true;
		}
		if(*sep == '-' || (*sep >= '0' && *sep <= '9')) {
			digits++;
			continue;
		}
		if(digits > 0) {
			sep++;
			cmd.assign(buf, sep - buf);
			param.assign(sep);
			return true;
		}
	}
	cmd.assign(buf);
	param = "";
	return true;
}

void
Processor::serve(const string& name, const string& params)
{
	Port*   port = NULL;
	Socket* s    = NULL;
	RSP*    rsp  = NULL;

	if((port = Port::createInstance(name, params)) == NULL) {
		goto leave;
	}
	if((s = port->accept()) == NULL) {
		goto leave;
	}
	rsp = new RSP(s);

	if(rsp == NULL) {
		goto leave;
	}

nextPacket:
	{
		char        buf[RSP_DEFAULT_BUFFER_SIZE];
		int         n     = rsp->receivePacket(buf, sizeof(buf));
		const char* sep   = buf + n;
		string      cmd   = "";
		string      param = "";

		if(n < 0) {
			if(n == RSP::INTERRUPTED) {
				goto nextPacket;
			}
			goto leave;
		}

		if(n == 0) {
			//empty command
			goto nextPacket;
		}


		while(sep) {
			if(!getNextToken(buf, sep, cmd, param)) {
				cmd.assign(buf, 1);
				param.assign(buf + 1);
			}

			ResponseMap::const_iterator response = responseMap.find(cmd);

			if(response != responseMap.end()) {
				if(response->second.handler == NULL) {
					if(rsp->sendPacket(response->second.message.c_str(), response->second.message.length()) < 0) {
						goto leave;
					}
					goto nextPacket;
				}
				if(response->second.handler->onHandle(rsp, cmd, param)) {
					goto nextPacket;
				}
			}
		}

		//reply with empty packet
		if(rsp->sendPacket("") < 0) {
			goto leave;
		}
		goto nextPacket;
	}

leave:
	if(rsp) {
		delete rsp;
	}
	if(s) {
		delete s;
	}
	if(port) {
		delete port;
	}
}

}; //namespace gdb
