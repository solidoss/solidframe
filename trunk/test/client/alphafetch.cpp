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

#include "system/filedevice.h"
#include "system/debug.h"
#include "system/cassert.h"
#include "system/socketaddress.h"
#include "system/thread.h"
//#include "common/utils.h"
#include "writer.h"

using namespace std;


int main(int argc, char *argv[]){
	if(argc != 5){
		cout<<"Usage: alphafetch addr port path"<<endl;
		cassert(false);
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
	AddrInfo    ai("localhost","1114");
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
	fd.create(argv[4], FileDevice::WO);
	Writer wr(sd);
	wr<<"f1 fetch \""<<argv[3]<<'\"'<<" \""<<argv[1]<<"\" "<<argv[2]<<crlf;
	wr<<"f2 logout"<<crlf;
	wr.flush();
	char buf[4*1024];
	int rv;
	int rc = 0;
	while((rv = read(sd, buf, 4*1024)) > 0){
		fd.write(buf, rv);
		rc += rv;
		cout<<rc<<'\r'<<flush;
	}
	cout<<endl;
	Thread::waitAll();
	return 0;
}
