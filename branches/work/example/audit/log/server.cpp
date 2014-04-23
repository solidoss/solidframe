#include <iostream>
#include "audit/log/logmanager.hpp"
#include "audit/log/logconnectors.hpp"
#include "audit/log.hpp"
#include "utility/iostream.hpp"
#include "system/thread.hpp"
#include "system/directory.hpp"
#include "system/debug.hpp"

#ifdef ON_WINDOWS
#include <Windows.h>
#else
#include <unistd.h>
#endif

using namespace std;
using namespace solid;

#ifdef ON_WINDOWS
typedef HANDLE DescriptorT;
static const HANDLE invalid_descriptor = INVALID_HANDLE_VALUE;;
void close_descriptor(HANDLE _d){
	CloseHandle(_d);
}
int doread(DescriptorT _d, char *_pb, uint _sz){
	DWORD cnt;
	BOOL rv = ReadFile(_d, _pb, _sz, &cnt, NULL);
	if(rv){
		return cnt;
	}else{
		return -1;
	}
}
int dowrite(DescriptorT _d, const char *_pb, uint _sz){
	DWORD cnt;
	BOOL rv = WriteFile(_d, _pb, _sz, &cnt, NULL);
	if(rv){
		return cnt;
	}else{
		return -1;
	}
}
void create_pipe(HANDLE *_ph){
	CreatePipe(_ph, _ph + 1, NULL, 2048);
}
#else
typedef int DescriptorT;
static const int invalid_descriptor = -1;
void close_descriptor(int _d){
	::close(_d);
}
int doread(DescriptorT _d, char *_pb, solid::uint _sz){
	return ::read(_d, _pb, _sz);
}
int dowrite(DescriptorT _d, const char *_pb, solid::uint _sz){
	return ::write(_d, _pb, _sz);
}
void create_pipe(DescriptorT *_ph){
	pipe(_ph);
}
#endif


struct DeviceInputOutputStream: InputOutputStream{
	DeviceInputOutputStream(DescriptorT _d, DescriptorT _pd):d(_d), pd(_pd){}
	void close(){
		DescriptorT tmp = d;
		d = invalid_descriptor;
		if(pd != invalid_descriptor){
			close_descriptor(pd);
			close_descriptor(tmp);
		}
	}
	/*virtual*/ int read(char *_pb, uint32 _bl, uint32 _flags = 0){
		int rv = doread(d, _pb, _bl);
		return rv;
	}
	/*virtual*/ int write(const char *_pb, uint32 _bl, uint32 _flags = 0){
		return dowrite(d, _pb, _bl);
	}
	int64 seek(int64, SeekRef){
		return -1;
	}
	DescriptorT d;
	DescriptorT pd;
};

DescriptorT pairfd[2];

int main(int _argc, char *argv[]){
	Thread::init();
#ifdef ON_WINDOWS
	WSADATA	wsaData;
    int		err;
	WORD	wVersionRequested;
/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
    wVersionRequested = MAKEWORD(2, 2);

    err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
        /* Tell the user that we could not find a usable */
        /* Winsock DLL.                                  */
        printf("WSAStartup failed with error: %d\n", err);
        return 1;
    }
#endif
	create_pipe(pairfd);
#ifdef UDEBUG
	Debug::the().levelMask();
	Debug::the().moduleMask();
	Debug::the().initStdErr();
#endif
	audit::LogManager lm;
	lm.start();
	lm.insertChannel(new DeviceInputOutputStream(pairfd[0], pairfd[1]));
	lm.insertListener("localhost", "8888");
	Directory::create("log");
	lm.insertConnector(new audit::LogBasicConnector("log"));
	Log::the().reinit(argv[0], new DeviceInputOutputStream(pairfd[1],invalid_descriptor), "ALL");
	
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
