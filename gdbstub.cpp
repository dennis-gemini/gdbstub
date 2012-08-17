#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "Processor.h"

//http://www.cims.nyu.edu/cgi-systems/info2html?(gdb)Packets

using namespace gdb;

class QueryHandler: public Handler
{
public:
	QueryHandler()
	{
	}

	virtual
	~QueryHandler()
	{
	}

	virtual bool
	onHandle(RSP* rsp, const string& cmd, const string& param)
	{
		const char* p      = param.c_str();
		string      subcmd = "";

		if(!RSP::getNextParamStr(p, subcmd)) {
			return false;
		}
		if(cmd == "Q") {
			if(subcmd == "StartNoAckMode") {
				rsp->setNoAckMode(true);
				rsp->sendPacket("OK");
				return true;
			}
			if(subcmd == "PassSignals") {
				rsp->sendPacket("OK");
				return true;
			}
			return false;
		}

		if(subcmd == "Xfer") {
			//$qXfer:features:read:target.xml:0,ffa#78, l = last, m = more
			rsp->sendPacket("l<target version=\"1.0\"><architecture>i386</architecture></target>");
			return true;
		}
		if(subcmd == "Supported") {
			//$qSupported:xmlRegisters=i386;qRelocInsn+#25
			rsp->sendPacketFormat("PacketSize=%x"
				";qXfer:libraries:read+"
				";qXfer:features:read+"		//for registers
			//	";qXfer:auxv:read+"
				";QStartNoAckMode+"
				";QPassSignals+"
				, RSP_DEFAULT_BUFFER_SIZE - 1);
			return true;
		}
		if(subcmd == "C") {
			//current thread: hex encoded 16it process id
			//$qC#b4
			rsp->sendPacket("QC1234");
			return true;
		}
		if(subcmd == "Attached") {
			//$qAttached#8f
			rsp->sendPacket("1");
			return true;
		}
		if(subcmd == "TStatus") {
			//$qTStatus#49
			return false;
		}
		///////////////////////////////////////////////////////////////////////////////
		//info threads
		if(subcmd == "fThreadInfo") {
			//all thread ids: first
			//$qfThreadInfo#bb
			rsp->sendPacket("m1234,1000,2000,3000");
			return true;
		}
		if(subcmd == "sThreadInfo") {
			//all thread ids: subsequent
			//$qsThreadInfo#bb
			rsp->sendPacket("l");
			return true;
		}
		if(subcmd == "ThreadExtraInfo") {
			//extra thread info
			rsp->sendPacket(RSP::hexify("thread info la la la").c_str()); //$qThreadExtraInfo,1234#4f
			return true;
		}
		if(subcmd == "L") {	//replaced by qThreadExtraInfo
			//query LIST or thread LIST (deprecated)
			//$qL1200000000000000000#50
			return false;
		}
		///////////////////////////////////////////////////////////////////////////////
		if(subcmd == "CRC") {
			//query CRC of memory block
			//qCRC:addr,len
			//Ccrc32
			return false;
		}
		if(subcmd == "Offsets") {
			//get section offsets
			rsp->sendPacket("Text=0;Data=1;Bss=2");
			return true;
		}
		if(subcmd == "P") {
			//$qP0000001f0000000000001234#82
			return false;
		}
		if(subcmd == "Symbol") {
			string sym_name;
			string sym_value;

			if(!RSP::getNextParamStr(p, sym_name)) {
				return false;
			}
			if(!RSP::getNextParamStr(p, sym_value)) {
				return false;
			}

			if(sym_name == "" && sym_value == "") {
				rsp->sendPacketFormat("qSymbol:%s", RSP::hexify("main").c_str());
			} else {
				rsp->sendPacket("OK");
			}
		}
		return true;
	}
};

class MemoryHandler: public Handler
{
public:
	MemoryHandler()
	{
	}

	virtual
	~MemoryHandler()
	{
	}

	virtual bool
	onHandle(RSP* rsp, const string& cmd, const string& param)
	{
		if(cmd == "m") {
			const char* p    = param.c_str();
			int         addr = 0; 
			int         len  = 0; 

			if(RSP::getNextParamInt(p, addr, 16) && RSP::getNextParamInt(p, len, 16)) {
				string result = "";

				for(; len > 0; len--) {
					result += "00";
				}
				rsp->sendPacket(result.c_str());
			} else {
				rsp->sendPacket("E00");
			}
		} else if(cmd == "M") {
			rsp->sendPacket("OK");
		}
		return true;
	}
};

class ContinueHandler: public Handler
{
public:
	ContinueHandler()
	{
	}

	virtual
	~ContinueHandler()
	{
	}

	virtual bool
	onHandle(RSP* rsp, const string& cmd, const string& param)
	{
		if(cmd == "C") {
			fprintf(stderr, "send signal %s\n", param.c_str());
			rsp->sendPacketFormat("S%s", param.c_str());
			return true;
		}

		if(cmd == "c") {
			fprintf(stderr, "resume at %s...\n", param == ""? "current": param.c_str());
		}

		while(true) {
			fprintf(stderr, "continue...\n");

			if(rsp->isInterrupted()) {
				rsp->sendPacketFormat("S%02x", SIGINT);
				break;
			}
			sleep(1);
		}
		return true;
	}
};

class StepHandler: public Handler
{
public:
	StepHandler()
	{
	}

	virtual
	~StepHandler()
	{
	}

	virtual bool
	onHandle(RSP* rsp, const string& cmd, const string& param)
	{
		if(cmd == "S") {
			fprintf(stderr, "send signal %s\n", param.c_str());
			rsp->sendPacketFormat("S%s", param.c_str());
			return true;
		}

		if(cmd == "s") {
			fprintf(stderr, "stepping at %s...\n", param == ""? "current": param.c_str());
		}

		rsp->sendPacketFormat("T%02x05:01020304;04:02030405;08:03040506;thread:1234;", SIGTRAP);
		//rsp->sendPacketFormat("T%02x", SIGTRAP);
		//rsp->sendPacket("OK");
		return true;
	}
};

class RemoteCommandHandler: public Handler
{

public:
	RemoteCommandHandler()
	{
	}

	virtual
	~RemoteCommandHandler()
	{
	}

	virtual bool
	onHandle(RSP* rsp, const string& cmd, const string& param)
	{
		fprintf(stderr, "remote command: %s ==> %s\n", param.c_str(), RSP::unhexify(param).c_str());
		rsp->sendPacketFormat("O%s", RSP::hexify("how are you?\n").c_str());
		rsp->sendPacket("OK");
		return true;
	}
};

int
main(int argc, char** argv)
{
	const char* name   = "tcp";
	const char* params = "1234";

	if(argc > 1) {
		argc--, argv++;

		for(; argc > 0; argv++, argc--) {
			if(strncasecmp(*argv, "--tcp", 5) == 0) {
				name = "tcp";

				if(argc > 1) {
					params = *++argv, argc--;
					continue;
				}
				params = "1234";
				continue;
			}
			if(strncasecmp(*argv, "--stdio", 7) == 0) {
				name   = "stdio";
				params = "";
				continue;
			}
		}
	}

	Processor* processor = new Processor();

	///////////////////////////////////////////////////////////////////////////////////////////////////////
	//a                      -- reserved
	//Aarglen,argnum,arg,... -- set program arguments (reserved)
	//b                      -- set baud (deprecated)
	//Baddr,mode             -- set breakpoint (deprecated); mode = {'S': set, 'C': clear}, replaced by 'Z'/'z'
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	//caddr                  -- continue; if addr is omitted, resume at current addr.
	ContinueHandler continue_handler;
	processor->defineResponse("c", &continue_handler); //$c#63
	//Csig;addr              -- continue with signal in hex; if ';addr' is omitted, resume the same addr.
	processor->defineResponse("C", &continue_handler); //$C01#a4
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	//d                      -- toggle debug flag (deprecated)
	//D                      -- detach
	processor->defineResponse("D", "OK");	//$D#44 (no reply)
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	//e                      -- reserved
	//E                      -- reserved
	//f                      -- reserved
	//F                      -- reserved
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	//g                      -- read registers: REGISTER_RAW_SIZE and REGISTER_NAME
	processor->defineResponse("g", "xxxxxxxx0000000100000002");	//$g#67
	//GXX...                 -- write registers
	processor->defineResponse("G", "OK");				//$G#67
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	//h                      -- reserved
	//Hct                    -- set thread for subsequent op (m, M, g, G...), c for step continue, t = -1: for all threads
	//Hgt                    -- set thread for subsequent op (m, M, g, G...), g for other operations, t = -1: for all threads
	processor->defineResponse("Hc", "OK"); //$Hc-1#09 $Hc0#db
	processor->defineResponse("Hg", "OK"); //$Hg0#df
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	//iaddr,nnn              -- cycle step (draft), step the remote target by a signel clock cycle. If ,nnn is present, cycle step nnn cycles. If addr is present, cycle step starting at that addr.
	//I                      -- signal then cycle step (reserved)
	//j                      -- reserved
	//J                      -- reserved
	//k                      -- kill request ???
	//K                      -- reserved
	//l                      -- reserved
	//L                      -- reserved
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	//maddr,len              -- read memory (addr, len)
	MemoryHandler memory_handler;
	processor->defineResponse("m", &memory_handler); //$m0,1#fa $m0,8#01 $m0,7#00
	//Maddr,len:XX...        -- write memory
	processor->defineResponse("M", &memory_handler);
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	//n                      -- reserved
	//N                      -- reserved
	//o                      -- reserved
	//O                      -- reserved
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	//pn...                  -- read reg (reserved): hex encoded value in target byte order
	//Pn...=r                -- write reg n with value r containing two hex digits in target byte order
	processor->defineResponse("p", "12345678"); //$p8#a8
	processor->defineResponse("P", "OK"); //$P8#a8
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	//qquery                 -- general query
	QueryHandler query_handler;
	processor->defineResponse("q", &query_handler); //$q...#xx
	processor->defineResponse("Q", &query_handler); //$Q...#xx
	//$qRcmd,xxxx....................xx#cc
	RemoteCommandHandler remote_command_handler;
	processor->defineResponse("qRcmd", &remote_command_handler);
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	//r                      -- reset the entire system (deprecated)
	//RXX                    -- remote restart. (extended mode); no reply
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	//saddr                  -- step
	StepHandler step_handler;
	processor->defineResponse("s", &step_handler);
	//Ssig;addr              -- step with signal
	//taddr:PP,MM            -- search
	//TXX                    -- thread alive
	processor->defineResponse("T", "OK");	//OK or Enn $T1234#1e
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	//u                      -- reserved
	//U                      -- reserved
	//v                      -- reserved
	//$vCont?#49
	//V                      -- reserved
	//w                      -- reserved
	//W                      -- reserved
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	//x                      -- reserved
	//Xaddr,len:XX...        -- write mem with (XX) binary data (addr,len:XX), (escaped)
	processor->defineResponse("X" , "OK");
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	//y                      -- reserved
	//Y                      -- reserved
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	processor->defineResponse("z" , "OK");
	processor->defineResponse("Z" , "OK");
	//zt,addr,len            -- remove break or watchpoint (draft)
	//Zt,addr,len            -- insert break or watchpoint (draft): 0: sw breakpoint, 1: hw breakpoint, 2: write watchpoint, 3: read watchpoint, 4: acess watchpoint
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	//?                      -- current signal
	processor->defineResponse("?" , "S%02x", SIGTRAP); //$?#3f
	///////////////////////////////////////////////////////////////////////////////////////////////////////

	processor->serve(name, params);
	return 0;
}

