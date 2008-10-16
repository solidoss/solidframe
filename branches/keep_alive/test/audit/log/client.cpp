#include "audit/log.hpp"
#include "system/socketdevice.hpp"
#include "system/debug.hpp"
#include "utility/ostream.hpp"

struct SocketOStream: OStream{
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
	Log::instance().reinit(argv[0], Log::AllLevels, "any");
	{
		SocketOStream	*pos(new SocketOStream);
		AddrInfo ai("localhost", 3333, 0, AddrInfo::Inet4, AddrInfo::Stream);
		if(!ai.empty()){
			pos->sd.create(ai.begin());
			pos->sd.connect(ai.begin());
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

