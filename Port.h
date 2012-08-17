#ifndef __Port__h__
#define __Port__h__

#include "Socket.h"

#include <string>

using namespace std;

namespace gdb 
{
	class Port
	{
	protected:
		Port();

	public:
		static Port*
		createInstance(const string& name, const string& params);

		virtual
		~Port();

		virtual Socket*
		accept() = 0;
	};
};

#endif/*__Port__h__*/
