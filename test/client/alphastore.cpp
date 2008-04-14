#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <deque>

#include "system/filedevice.hpp"
#include "system/debug.hpp"
#include "system/cassert.hpp"
#include "system/socketaddress.hpp"
#include "system/thread.hpp"
//#include "common/utils.h"
#include "utility/istream.hpp"
#include "writer.hpp"

using namespace std;

class FileStream: public IStream{
public:
	FileStream(FileDevice &_rfd):fd(_rfd){}
	int read(char *_pb, uint32 _sz, uint32 _flags = 0){
		return fd.read(_pb, _sz);
	}
	int64 seek(int64, SeekRef){
	}
private:
	FileDevice &fd;
};

int main(int argc, char *argv[]){
	if(argc != 5){
		cout<<"Usage: alphastore alpha_addr alpha_port local_path path"<<endl;
		return 0;
	}
	signal(SIGPIPE, SIG_IGN);
	Thread::init();
#ifdef UDEBUG
	{
	string s = "dbg/";
	s+= argv[0]+2;
	//initDebug(s.c_str());
	}
#endif
	AddrInfo    ai(argv[1], argv[2]);
	if(ai.empty()){
		idbg("No such address");
		return 0;
	}
	AddrInfoIterator it(ai.begin());
	int sd = socket(it.family(), it.type(), it.protocol());
	if(sd < 0){
		idbg("error creating socket");
		//cout<<"failed socket "<<pos<<endl;
		return 0;
	}
	//cout<<"before connect "<<pos<<endl;
	if(connect(sd, it.addr(), it.size())){
		idbg("failed connect");
		//cout<<"failed connect "<<pos<<endl;
		return 0;
	}
	FileDevice fd;
	fd.open(argv[3], FileDevice::RO);
	FileStream fs(fd);
	Writer wr(sd);
	cout<<"file size "<<fd.size()<<endl;
	//wr<<"f1 fetch \""<<argv[5]<<'\"'<<" \""<<argv[3]<<"\" "<<argv[4]<<crlf;
	wr<<"s1 store \""<<argv[4]<<'\"'<<' '<<lit(&fs, fd.size())<<crlf;
	wr<<"s2 logout"<<crlf;
	wr.flush();
	int rv;
	int rc = 0;
	const unsigned bufsz = 1024;
	char buf[bufsz];
	while((rv = read(sd, buf, bufsz)) > 0){
		rc += rv;
		cout.write(buf, rv);
	}
	cout<<endl;
	Thread::waitAll();
	return 0;
}
