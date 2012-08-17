#include "RSP.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace gdb {

RSP::Buffer::Buffer(Socket* s, size_t size)
{
	this->s    = s;
	this->len  = 0;
	this->size = size;
	this->buf  = new char[size];
	this->cur  = NULL;
	this->ch   = -1;
}

RSP::Buffer::~Buffer()
{
	if(buf) {
		delete[] buf;
		buf = NULL;
	}
}

void
RSP::Buffer::dump(const char* name, const char* ptr, size_t n)
{
	FILE*       fp    = stderr;
	const char* p     = ptr;
	const char* limit = ptr + n;

	fprintf(fp, "%s> ", name);
	for(; p < limit; p++) {
		if((unsigned char) *p < 0x20 || (unsigned char) *p >= 0x7f) {
			fprintf(fp, "\\x%02x", *p & 0xff);
		} else {
			fprintf(fp, "%c", *p);
		}
	}
	fprintf(fp, "\n");
	fflush(fp);
}

int
RSP::Buffer::peek()
{
	if(this->ch >= 0) {
		return this->ch;
	}
	if(cur == NULL || cur >= buf + len) {
		if(!s->isReadable()) {
			return -1;
		}
		int n = s->read(buf, size);

		if(n <= 0) {
			return -1;
		}
		len = n;
		cur = buf;
		dump("in", buf, len);
	}
	return *cur & 0xff;
}

#undef getc

int
RSP::Buffer::getc()
{
	if(this->ch >= 0) {
		int ch = this->ch;
		this->ch = -1;
		return ch;
	}
	if(cur == NULL || cur >= buf + len) {
		int n = s->read(buf, size);

		if(n <= 0) {
			len = 0;
			cur = NULL;
			return -1;
		}
		len = n;
		cur = buf;
		dump("in", buf, len);
	}
	return *cur++ & 0xff;
}

#undef ungetc

void
RSP::Buffer::ungetc(int ch)
{
	this->ch = ch;
}

#undef putc

int
RSP::Buffer::putc(int ch)
{
	if(len >= size) {
		if(flush() < 0) {
			return -1;
		}
	}
	if(cur == NULL) {
		cur = buf;
	}
	*cur++ = ch, len++;
	return ch;
}

int
RSP::Buffer::read(void* buffer, size_t length)
{
	char* out   = (char*) buffer;
	char* limit = (char*) buffer + length;

	while(out < limit) {
		int ch = getc();

		if(ch < 0) {
			break;
		}
		*out++ = ch;
	}
	return out - (char*) buffer;
}

int
RSP::Buffer::write(const void* buffer, size_t length)
{
	const char* in    = (const char*) buffer;
	const char* limit = (const char*) buffer + length;

	for(; in < limit; in++) {
		int ret = putc(*in);

		if(ret < 0) {
			break;
		}
	}
	return in - (const char*) buffer;
}

int
RSP::Buffer::flush()
{
	char*  ptr       = buf;
	size_t remaining = len;

	while(remaining > 0) {
		int n = s->write(ptr, remaining);

		if(n < 0) {
			return -1;
		}
		dump("out", ptr, n);

		ptr += n, remaining -= n;
	}
	cur = NULL;
	len = 0;
	return ptr - buf;
}

RSP::RSP(Socket* s, size_t size):
	recv_buffer(s, size), send_buffer(s, size)
{
	noAckMode = false;
}

RSP::~RSP()
{
}

bool
RSP::isInterrupted()
{
	int ch = recv_buffer.peek();

	if(ch == 0x03) {
		recv_buffer.getc();
		return true;
	}
	return false;
}

void
RSP::setNoAckMode(bool noAckMode)
{
	this->noAckMode = noAckMode;
}

bool
RSP::isNoAckMode() const
{
	return noAckMode;
}

int
RSP::receivePacket(char* packet, size_t packet_size)
{
	int   state     = STATE_INIT;
	int   checksum  = 0;
	int   checkcode = 0;
	char* out       = packet;
	char* limit     = packet + packet_size - 1;

	while(out < limit && state != STATE_VERIFIED) {
		int ch = recv_buffer.getc();

		if(ch < 0) {
			return DISCONNECTED;
		}

		if(ch == '$') {
			state     = STATE_CMD;
			checksum  = 0;
			checkcode = 0;
			out       = packet;
			continue;
		}
		if(ch == '#') {
			if(state == STATE_CMD) {
				state = STATE_CHECKSUM1;
				continue;
			}
			goto reset;
		}

		switch(state) {
			case STATE_INIT:
			{
				if(ch == 0x03) {
					//interrupted by Ctrl-C
					return INTERRUPTED;
				}
				continue;
			}
			case STATE_CMD:
			{
				checksum += ch;

				switch(ch) {
					case '*': //run-length encoding
					{
						if(out > packet) {
							//require preceding char to repeat
							goto reset;
						}

						if((ch = recv_buffer.getc()) < 0) {
							return DISCONNECTED;
						}
						if(ch == '#' || ch == '$' || ch == '+' || ch == '-') {
							//illegal chars as the encoding
							goto reset;
						}
						checksum += ch;

						int repeat = ch - 29;	//available number to repeat: 4, 5, 8 to 97.

						for(ch = out[-1]; repeat > 0 && out < limit; repeat--) {
							*out++ = ch;
						}
						continue;
					}
					case '}': //escape '#', '$', and '}' by XOR-ing with 0x20 
					{
						if((ch = recv_buffer.getc()) < 0) {
							return DISCONNECTED;
						}
						checksum += ch;
						*out++ = ch ^ 0x20;
						continue;
					}
				}
				*out++ = ch;
				continue;
			}
			case STATE_CHECKSUM1:
			case STATE_CHECKSUM2:
			{
				if((ch = HEXVAL(ch)) < 0) {
					//invalid hex char
					goto reset;
				}
				checkcode = (checkcode << 4) + ch;

				switch(state) {
					case STATE_CHECKSUM1:
					{	state = STATE_CHECKSUM2;
						continue;
					}
					case STATE_CHECKSUM2:
					{	if((checksum & 0xff) == checkcode) {
							state = STATE_VERIFIED;
							continue;
						}
						//pass thru
					}
				}
				//pass thru
			}
			default:
			reset:
			{
				state     = STATE_INIT;
				checksum  = 0;
				checkcode = 0;
				out       = packet;

				//respond with NAK
				send_buffer.putc('-');
				send_buffer.flush();
				continue;
			}
		}
	}

	if(state == STATE_VERIFIED) {
		*out = '\0';

		if(!noAckMode) {
			send_buffer.putc('+');
		}
		send_buffer.flush();
		return out - packet;
	}
	//buffer was too small
	return OVERFLOWED;
}

int
RSP::sendPacket(const char* buffer, size_t len)
{
	int         checksum = 0;
	const char* in       = buffer;

	if(len == 0) {
		for(; *in; in++, len++) {
			checksum += *in & 0xff;
		}
	} else {
		const char* limit = buffer + len;

		for(; in < limit; in++) {
			checksum += *in & 0xff;
		}
	}

	while(true) {
		if(send_buffer.putc('$') < 0) {
			return -1;
		}
		if(send_buffer.write(buffer, len) != len) {
			return -1;
		}
		if(send_buffer.putc('#') < 0) {
			return -1;
		}
		if(send_buffer.putc(HEXCHAR(checksum >> 4)) < 0) {
			return -1;
		}
		if(send_buffer.putc(HEXCHAR(checksum)) < 0) {
			return -1;
		}
		if(send_buffer.flush() < 0) {
			return -1;
		}
		if(noAckMode) {
			break;
		}

		int ch = recv_buffer.getc();

		if(ch < 0) {
			return -1;
		}
		if(ch == '$') {
			recv_buffer.ungetc(ch);
			break;
		}
		if(ch == '+') {
			break;
		}
	}
	return len;
}

int
RSP::sendPacketFormatV(const char* fmt, va_list args)
{
	char*  buf = NULL;
	size_t len = vasprintf(&buf, fmt, args);

	if(buf == NULL) {
		return -1;
	}
	return sendPacket(buf, len);
}

int
RSP::sendPacketFormat(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int ret = sendPacketFormatV(fmt, args);
	va_end(args);
	return ret;
}

string
RSP::unhexify(const string& str)
{
	const char* p      = str.c_str();
	const char* limit  = str.c_str() + str.length();
	string      result = "";

	for(; p + 1 < limit; p += 2) {
		result += char((HEXVAL(p[0]) << 4) + HEXVAL(p[1]));
	}
	return result;
}

string
RSP::hexify(const string& str)
{
	const char* p      = str.c_str();
	const char* limit  = str.c_str() + str.length();
	string      result = "";

	for(; p < limit; p++) {
		result += char(HEXCHAR(*p >> 4));
		result += char(HEXCHAR(*p));
	}
	return result;
}

bool
RSP::getNextParamStr(const char* &ptr, string& param)
{
	if(ptr == NULL) {
		return false;
	}

	if(*ptr == '\0') {
		ptr = NULL;
		return true;
	}

	static const char delimiters[] = ":;,";

	for(; *ptr && strchr(delimiters, *ptr) == NULL; ptr++) {
		param += *ptr;
	}
	if(*ptr != '\0') {
		ptr++;
	}
	return true;
}

bool
RSP::getNextParamInt(const char* &ptr, int &param, int base)
{
	string str = "";

	if(getNextParamStr(ptr, str) && str.length() > 0) {
		char* last = NULL;

		param = strtol(str.c_str(), &last, base);

		if(*last == '\0') {
			return true;
		}
	}
	return false;
}

}; // endof namespace gdb
