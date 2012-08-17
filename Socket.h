#ifndef __Socket__h__
#define __Socket__h__

#include <stdarg.h>
#include <string>

using namespace std;

namespace gdb 
{
	#undef getc
	#undef putc

	class Socket
	{
	protected:
		Socket();

	public:
		static Socket*
		createInstance(const string& name, int sd);

		virtual
		~Socket();

		int
		getc();

		int
		putc(int ch);

		int
		printf(const string& fmt, ...);

		int
		vprintf(const string& fmt, va_list args);

		virtual int
		read(void* buf, size_t maxlen) = 0;

		virtual int
		write(const void* buf, size_t len) = 0;

		virtual int
		flush() = 0;

		virtual bool
		isReadable() = 0;
	};
};

#endif/*__Socket__h__*/
