#include <iostream>
#include "audit/log/logmanager.hpp"
#include "audit/log.hpp"
#include "utility/iostream.hpp"

using namespace std;

struct DeviceIOStream: IOStream{
	DeviceIOStream(int _d):d(_d){}
	void close(){
		int tmp = d;
		d = -1;
		close(tmp);
	}
	virtual int read(char *_pb, uint32 _bl, uint32 _flags = 0){
		return ::read(d, _pb, _bl);
	}
	virtual int write(const char *_pb, uint32 _bl, uint32 _flags = 0){
		return ::write(d, _pb, _bl);
	}
};

int pairfd[2];

int main(int _argc, char *argv[]){
	pipe(pairfd);
	audit::LogManager lm;
	lm.addChannel(new DeviceIOStream(pairfd[0]));
	lm.addListener("localhost", "3333");
	Log::instance().reinit(argv[0], Log::AllLevels, "ALL", new DeviceIOStream(pairfd[1]));
	
	string s;
	while(true){
		cin>>s;
		if(s.size() == 1 && tolower(s[0]) == 'q') break;
		ilog(Log::any, s.size(), "new text : "<<s);
	}
	lm.stop(true);
	Thread::waitAll();
}
