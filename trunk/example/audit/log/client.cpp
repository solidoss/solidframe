#include "audit/log.hpp"
#include "system/socketdevice.hpp"
#include "system/socketaddress.hpp"
#include "system/debug.hpp"
#include "utility/ostream.hpp"

struct SocketOutputStream: OutputStream{
	/*virtual*/ void close(){
		sd.close();
	}
	/*virtual*/ int write(const char *_pb, uint32 _bl, uint32 _flags = 0){
		return sd.write(_pb, _bl);
	}
	int64 seek(int64, SeekRef){
		return -1;
	}
	SocketDevice	sd;
};

int main(int argc, char *argv[]){
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
	Log::instance().reinit(argv[0], Log::AllLevels, "any");
	{
		SocketOutputStream	*pos(new SocketOutputStream);
		ResolveData			rd = synchronous_resolve("localhost", "8888", 0, SocketInfo::Inet4, SocketInfo::Stream);
		if(!rd.empty()){
			pos->sd.create(rd.begin());
			pos->sd.connect(rd.begin());
			Log::instance().reinit(pos);
			idbg("Logging");
		}else{
			delete pos;
			edbg("Sorry no logging");
		}
	}
	ilog(Log::any, 0, "some message");
	ilog(Log::any, 1, "some message 1");
	ilog(Log::any, 2, "some message 2");
	ilog(Log::any, 3, "some message 3");
	return 0;
}

