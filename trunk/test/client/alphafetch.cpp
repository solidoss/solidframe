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
#include "writer.h"

using namespace std;


int main(int argc, char *argv[]){
	if(argc != 7){
		cout<<"Usage: alphafetch alpha_addr alpha_port ipc_addr ipc_port path local_path"<<endl;
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
	fd.create(argv[6], FileDevice::WO);
	Writer wr(sd);
	wr<<"f1 fetch \""<<argv[5]<<'\"'<<" \""<<argv[3]<<"\" "<<argv[4]<<crlf;
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
