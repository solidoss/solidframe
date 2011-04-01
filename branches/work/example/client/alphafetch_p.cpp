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
#include "writer.hpp"

using namespace std;

//----------------------------------------------------------------------------
template <class T, T C1>
inline T* find(T *_pc){
	while(*_pc != C1) ++_pc;
	return _pc;
}

template <class T, T C1, T C2>
inline T* find(T *_pc){
	while(*_pc != C2 && *_pc != C1) ++_pc;
	return _pc;
}

template <class T, T C1, T C2, T C3>
inline T* find(T *_pc){
	while(*_pc != C2 && *_pc != C1 && *_pc != C3) ++_pc;
	return _pc;
}
template <class T, T C1, T C2, T C3>
inline T* findNot(T *_pc){
	while(*_pc == C2 || *_pc == C1 || *_pc == C3) ++_pc;
	return _pc;
}

int main(int argc, char *argv[]){
	if(argc != 7){
		cout<<"Plain alpha connection fetch test"<<endl;
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
	if(*argv[3] && *argv[4]){
		wr<<"f1 fetch \""<<argv[5]<<'\"'<<" \""<<argv[3]<<"\" "<<argv[4]<<crlf;
	}else{
		wr<<"f1 fetch \""<<argv[5]<<'\"'<<crlf;
	}
	wr<<"f2 logout"<<crlf;
	wr.flush();
	char buf[4*1024];
	char *bpos;
	char *bend;
	char c;
	int rv;
	//int rc = 0;
	enum States{
		ReadBanner,
		BeginLiteral,
		FetchLiteral,
		EndLiteral,
		WriteLiteral,
		ReadEnd
	};
	States state = ReadBanner;
	uint32	litsz = 0;
	uint32	filesz = 0;
	uint32	towrite = 0;
	string	litstr;
	while((rv = read(sd, buf, 4*1024)) > 0){
		bool refill = false;
		//cout<<"refilled"<<endl;
		bpos = buf;
		bend = buf + rv - 1;
		c = buf[rv - 1];
		buf[rv - 1] = 0;
		while(!refill){
			switch(state){
				case ReadBanner:
					//cout<<"ReadBanner"<<endl;
					bpos = find<char, '\n', '\0'>(bpos);
					if(bpos == bend){
						if(c == '\n'){
							state = BeginLiteral;
						}
						refill = true;
						break;
					}
					++bpos;
					state = BeginLiteral;
				case BeginLiteral:
					//cout<<"BeginLiteral"<<endl;
					bpos = find<char, '{', '\0'>(bpos);
					if(bpos == bend){
						if(c == '{'){
							state = FetchLiteral;
						}
						refill = true;
						break;
					}
					++bpos;
					state = FetchLiteral;
				case FetchLiteral:{
					//cout<<"FetchLiteral"<<endl;
					char *bsec = find<char, '}', '\0'>(bpos);
					*bend = c;
					litstr.append(bpos, bsec - bpos);
					if(bsec == bend and c != '}'){
						refill = true;
						break;
					}
					*bend = 0;
					//cout<<"Literal string = ["<<litstr<<']'<<endl;
					litsz = strtoul(litstr.c_str(), NULL, 10);
					//cout<<"Literal size = "<<litsz<<endl;
					state = EndLiteral;
					if(bsec == bend){
						refill = true;
						break;
					}
					bpos = bsec + 1;
				}
				case EndLiteral:
					//cout<<"EndLiteral"<<endl;
					bpos = find<char, '\n', '\0'>(bpos);
					if(bpos == bend){
						if(c == '\n'){
							state = WriteLiteral;
						}
						refill = true;
						break;
					}
					*bend = c;
					towrite = bend - bpos;
					if(towrite > litsz) towrite = litsz;
					fd.write(bpos + 1, towrite);
					filesz += towrite;
					litsz -= towrite;
					if(!litsz){
						/*cout<<'('<<endl;
						cout.write(buf + towrite, rv - towrite);
						cout<<')'<<endl*/;
						state = ReadEnd;
					}else{
						refill = true;
						state = WriteLiteral;
					}
					break;
				case WriteLiteral:
					//cout<<"WriteLiteral"<<endl;
					towrite = rv;
					if(towrite > litsz) towrite = litsz;
					*bend = c;
					fd.write(buf, towrite);
					filesz += towrite;
					litsz -= towrite;
					if(!litsz){
/*						cout<<'['<<endl;
						cout.write(buf + towrite, rv - towrite);
						cout<<']'<<endl;*/
						state = ReadEnd;
					}
					refill = true;
					break;
				case ReadEnd:
// 					cout<<"ReadEnd"<<endl;
// 					cout<<'{'<<endl;
// 					cout.write(buf, rv).flush();
// 					cout<<'}'<<endl;
					refill = true;
					break;
			}
		}
	}
	cout<<endl;
	cout<<"Output file size = "<<filesz<<endl;
	Thread::waitAll();
	return 0;
}
