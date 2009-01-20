#include <iostream>
#include "audit/log/logmanager.hpp"
#include "audit/log/logconnectors.hpp"
#include "audit/log.hpp"
#include "utility/iostream.hpp"
#include "system/thread.hpp"
#include "system/directory.hpp"
#include "system/debug.hpp"

using namespace std;

struct DeviceIOStream: IOStream{
	DeviceIOStream(int _d, int _pd):d(_d), pd(_pd){}
	void close(){
		int tmp = d;
		d = -1;
		if(pd > 0){
			::close(tmp);
			::close(pd);
		}
	}
	/*virtual*/ int read(char *_pb, uint32 _bl, uint32 _flags = 0){
		int rv = ::read(d, _pb, _bl);
		return rv;
	}
	/*virtual*/ int write(const char *_pb, uint32 _bl, uint32 _flags = 0){
		return ::write(d, _pb, _bl);
	}
	int64 seek(int64, SeekRef){
		return -1;
	}
	int d;
	int pd;
};

int pairfd[2];

int main(int _argc, char *argv[]){
	pipe(pairfd);
	Dbg::instance().init(NULL, Dbg::AllLevels, "any");
	audit::LogManager lm;
	lm.start();
	lm.insertChannel(new DeviceIOStream(pairfd[0], pairfd[1]));
	lm.insertListener("localhost", "3333");
	Directory::create("log");
	lm.insertConnector(new audit::LogBasicConnector("log"));
	Log::instance().reinit(argv[0], Log::AllLevels, "ALL", new DeviceIOStream(pairfd[1],-1));
	
	string s;
	while(true){
		cin>>s;
		if(s.size() == 1 && tolower(s[0]) == 'q') break;
// 		if(Log::instance().isSet(Log::any, Log::Info)){
// 			Log::instance().record(Log::Info, Log::any, 1, __FILE__, __FUNCTION__, __LINE__)<<"new text ";Log::instance().done();
// 		}
		ilog(Log::any, s.size(), "new text : "<<s);
	}
	lm.stop(true);
	Thread::waitAll();
}
