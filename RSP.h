//RSP: Remote Serial Protocol

#ifndef __RSP__h__
#define __RSP__h__

#include "Port.h"
#include "Socket.h"

#define RSP_DEFAULT_BUFFER_SIZE		(4096)

#define HEXVAL(ch) \
	((ch) >= '0' && (ch) <= '9'? ch - '0': \
	 (ch) >= 'a' && (ch) <= 'f'? ch - 'a' + 10: \
	 (ch) >= 'A' && (ch) <= 'F'? ch - 'A' + 10: -1)

#define HEXCHAR(val) \
	(((val & 0xf) <= 9)? (val & 0xf) + '0': (val & 0xf) - 10 + 'a')

namespace gdb {

	enum {
		STATE_INIT,
		STATE_CMD,
		STATE_CHECKSUM1,
		STATE_CHECKSUM2,
		STATE_VERIFIED
	};

	class RSP
	{
		class Buffer
		{
			Socket* s;
			size_t  len;
			size_t  size;
			char*   buf;
			char*   cur;
			int     ch;

			void
			dump(const char* name, const char* ptr, size_t n);

		public:
			Buffer(Socket* s, size_t size);

			~Buffer();

			int
			peek();

			int
			getc();

			void
			ungetc(int ch);

			int
			putc(int ch);

			int
			read(void* buf, size_t len);

			int
			write(const void* buf, size_t len);

			int
			flush();
		};

		bool   noAckMode;
		Buffer recv_buffer;
		Buffer send_buffer;

	public:
		RSP(Socket* s, size_t size = RSP_DEFAULT_BUFFER_SIZE);

		~RSP();

		enum {
			DISCONNECTED = -1,
			INTERRUPTED  = -2,
			OVERFLOWED   = -3
		};

		bool
		isInterrupted();

		void
		setNoAckMode(bool noAckMode);

		bool
		isNoAckMode() const;

		int
		receivePacket(char* packet, size_t packet_size);

		int
		sendPacket(const char* buffer, size_t len = 0);

		int
		sendPacketFormat(const char* fmt, ...);

		int
		sendPacketFormatV(const char* fmt, va_list args);

		static string
		unhexify(const string& str);

		static string
		hexify(const string& str);

		static bool
		getNextParamStr(const char* &ptr, string& param);

		static bool
		getNextParamInt(const char* &ptr, int &param, int base = 10);
	};

}; // endof namespace gdb

#endif/*__RSP__h__*/
