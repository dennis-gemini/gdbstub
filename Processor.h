#ifndef __Processor__h__
#define __Processor__h__

#include <string>
#include <map>
#include "RSP.h"

using namespace std;

namespace gdb {

	class Handler
	{
	public:
		Handler();

		virtual
		~Handler();

		virtual bool
		onHandle(RSP* rsp, const string& cmd, const string& param) = 0;
	};

	class Processor
	{
		struct Response
		{
			Handler* handler;
			string   message;

			Response();
			~Response();
		};

		typedef map<string, Response> ResponseMap;

		ResponseMap responseMap;

		bool
		getNextToken(const char* buf, const char* &sep, string& cmd, string& param) const;

	public:
		Processor();

		~Processor();

		void
		defineResponseV(const string& cmd, const string& message, va_list args);

		void
		defineResponse(const string& cmd, const string& message, ...);

		void
		defineResponse(const string& cmd, Handler* handler);

		void
		serve(const string& name, const string& params = "");
	};

}; //namespace gdb

#endif/*__Processor__h__*/
