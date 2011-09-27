#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <cerrno>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <deque>
#include <cerrno>

#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "system/socketaddress.hpp"
#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/timespec.hpp"
#include "writer.hpp"

using namespace std;
//typedef unsigned long long uint64;
enum {BSIZE = 1024};
static int sleeptout = 0;
///\cond 0
class Info{
public:
	Info():concnt(0), liscnt(0){}
	void update(unsigned _pos, ulong _v);
	unsigned pushBack();
	int print();
	TimeSpec		ft;
	void lock(){m.lock();}
	void unlock(){m.unlock();}
	void doneConnect(){++concnt;}
	void doneList(){++liscnt;}
	void addWait(){lock();++waitcnt;unlock();}
	void subWait(){lock();--waitcnt;unlock();}
	int minId();
private:
	vector<ulong>   v;
	uint32			concnt;
	uint32			liscnt;
	uint32			waitcnt;
	Mutex           m;
	TimeSpec		ct;
};
///\endcond

unsigned Info::pushBack(){
	v.push_back(0xffffffff);
	return v.size() - 1;
}

void Info::update(unsigned _pos, ulong _v){
	//m.lock();
	v[_pos] = _v;
	//m.unlock();
}

int Info::print(){
	uint64	tot = 0;
	uint32	mn = 0xffffffff;
	uint32	mncnt = 0;
	uint32	mx = 0;
	uint32	mxcnt = 0;
	//uint32	notconnected = 0;
	//m.lock();
	for(vector<ulong>::const_iterator it(v.begin()); it != v.end(); ++it){
		//cout<<(*it/1024)<<'k'<<' ';
		const uint32 t = *it;
		if(t == 0xffffffff){continue;}
		tot += t;
		//t /= 1024;
		if(t < mn){
			mn = t;
			mncnt = 1;
		}else if(t == mn){
			++mncnt;
		}
		if(t > mx){
			mx = t;
			mxcnt = 1;
		}else if(t == mxcnt){
			++mxcnt;
		}
	}
	tot >>= 10;
	mn >>= 10;
	mx >>= 10;
	ct.currentMonotonic();
	cout<<"speed = "<<tot/(ct.seconds() - ft.seconds() + 1)<<"k/s avg = "<<tot/v.size()<<"k min = "<<mn<<"k ("<<mncnt<<") max = "<<mx<<"k ("<<mxcnt<<')';
	if(concnt != v.size()) cout<<" conected = "<<concnt;
	if(liscnt != v.size()) cout<<" listed = "<<liscnt;
	cout<<'\r'<<flush;
	return waitcnt;
}

int Info::minId(){
	uint32 mn(0xffffffff);
	int idx = -1;
	for(vector<ulong>::const_iterator it(v.begin()); it != v.end(); ++it){
		const uint32 t = *it;
		if(t == 0xffffffff){continue;}
		if(t < mn)
		{
			mn = t;
			idx = it - v.begin();
		}
	}
	return idx;
}

static Info inf;

class AlphaThread: public Thread{
public:
	AlphaThread(
		const char *_node, 
		const char* _svice, 
		const char *_path,
		unsigned _pos,
		const char* _addr = NULL,
		int _port = -1,
		int _repeatcnt= 0,
		int _cnt = ((unsigned)(0xfffffff)),
                int _sleep = 1):ai(_node, _svice, 0, SocketAddressInfo::Inet4, SocketAddressInfo::Stream), wr(-1),sd(-1),cnt(_cnt),slp(_sleep),
			path(_path), pos(_pos),addr(_addr?_addr:""),port(_port),repeatcnt(_repeatcnt){}
	void run();
private:
	enum {BufLen = 2*1024};
	int list(char *_pb);
	int fetch(unsigned _idx, char *_pb);
	typedef std::deque<string> StrDqT;
	SocketAddressInfo    ai;
	Writer      wr;
	int         sd;
	int         cnt;
	int         slp;
	const char  *path;
	StrDqT		sdq;
	unsigned    pos;
	ulong       readc;
	string		addr;
	int 		port;
	int			repeatcnt;
};

void AlphaThread::run(){
	idbg(":::::::::::::::::::::::: "<<pos);
	if(ai.empty()){
		idbg("No such address");
		return;
	}
	SocketAddressInfoIterator it(ai.begin());
	sd = socket(it.family(), it.type(), it.protocol());
	if(sd < 0){
		idbg("error creating socket");
		cout<<"failed socket "<<pos<<endl;
		return;
	}
	//cout<<"before connect "<<pos<<endl;
	if(connect(sd, it.addr(), it.size())){
		idbg("failed connect");
		cout<<"failed connect "<<pos<<": "<<strerror(errno)<<endl;
		return;
	}
	inf.lock();
	cout<<pos<<" connected"<<endl;
	inf.doneConnect();
	inf.unlock();
	//timeval tv;
// 	memset(&tv, 0, sizeof(timeval));
// 	tv.tv_sec = 30;
// 	setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
// 	setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	readc = 0;
	wr.reinit(sd);
	char buf[BufLen];
	if(port>0 && port < 1200){
		cout<<"Using proxy..."<<endl;
		wr<<"localhost ";
		wr<<(uint32)port<<crlf;
		wr.flush();
	}
	int rv = list(buf);
	idbg("return value "<<rv);
	inf.update(pos, readc);
	ulong m = sdq.size();
	inf.lock();
	inf.doneList();
	cout<<pos<<" fetched file list: "<<m<<" files "<<endl;
	inf.unlock();
// 	for(StrDqT::const_iterator it(sdq.begin()); it != sdq.end(); ++it){
// 		cout<<'['<<*it<<']'<<endl;
// 	}
	if(rv) return;
	if(repeatcnt == 0) repeatcnt = -1;
	//if(repeatcnt > 0) ++repeatcnt;
	ulong ul = pos;
	while(repeatcnt < 0 || repeatcnt--){
		ulong cnt = m;
		while(cnt--  && !(rv = fetch(ul % m, buf))){
			++ul;
			Thread::sleep(sleeptout);
		}
	}
	idbg("return value "<<rv);
	cout<<endl<<"return value"<<rv<<endl;
	inf.subWait();
}

//----------------------------------------------------------------------------
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

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

int AlphaThread::list(char *_pb){
	if(addr.size()){
		//remote list
		wr<<"s1 remotelist \""<<path<<"\" \""<<addr<<"\" "<<(uint32)port<<crlf;
	}else{
		//local list
		wr<<"s1 list \""<<path<<'\"'<<crlf;
	}
	if(wr.flush()) return -1;
	enum {
		SkipFirstLine,
		StartLine,
		FirstSpace,
		SecondSpace,
		FindPathStart,
		ReadPath,
		ReadLF,
		SkipLine,
		SkipLineLF,
		ReadFinalCR,
		ReadFinalLF
	};
	int rc;
	int state = SkipFirstLine;
	const char *bpos;
	const char *bend;
	const char *bp;
	while((rc = read(sd, _pb, BufLen - 1)) > 0){
		bool b = true;
		readc += rc;
		inf.update(pos, readc);
		bpos = _pb;
		bend = bpos + rc;
		_pb[rc] = '\0';
		//cout<<'[';cout.write(_pb, rc);cout<<']'<<endl;
		while(b){
			switch(state){
				case SkipFirstLine:
					bp = find<const char,'\n','\0'>(bpos);
					if(*bp){
						bpos = bp + 1;
						state = StartLine;
					}else{
						b = false; break;
					}
				case StartLine:
					if(!*bpos){b = false; break;}
					if(*bpos != '*'){state = SkipLine; break;}
					++bpos;
					state = FirstSpace;
				case FirstSpace:
					if(!*bpos){b = false; break;}
					if(*bpos != ' '){state = SkipLine; break;}
					++bpos;
					state = SecondSpace;
				case SecondSpace://after the size
					bp = find<const char,' ','\0'>(bpos);
					if(*bp){
						bpos = bp + 1;
						state = FindPathStart;
					}else{
						b = false; break;
					}
				case FindPathStart:
					bp = find<const char,' ','\0'>(bpos);
					if(*bp){
						bpos = bp + 1;
						sdq.push_back("");
						state = ReadPath;
					}else{
						b = false; break;
					}
				case ReadPath:
					bp = find<const char,'\r','\0'>(bpos);
					sdq.back().append(bpos, bp - bpos);
					if(*bp){
						bpos = bp + 1;
						state = ReadLF;
					}else{
						b = false; break;
					}
				case ReadLF:
					if(!*bpos){b = false; break;}
					if(*bpos != '\n'){return -2;}
					++bpos;
					state = StartLine;
					break;
				case SkipLine:
					bp = find<const char, '\r', '@', '\0'>(bpos);
					if(!*bp){b = false; break;}
					bpos = bp + 1;
					if(*bp == '\r'){
						state = SkipLineLF;
						break;
					}
					if(*bp == '@'){
						state = ReadFinalCR;
						break;
					}
					break;
				case SkipLineLF:
					if(!*bpos){b = false; break;}
					if(*bpos != '\n'){return -3;}
					++bpos;
					state = StartLine;
					break;
				case ReadFinalCR:
					if(!*bpos){b = false; break;}
					if(*bpos != '\r'){return -4;}
					++bpos;
					state = ReadFinalLF;
				case ReadFinalLF:
					if(!*bpos){b = false; break;}
					if(*bpos != '\n'){return -5;}
					++bpos;
					return 0;
			}
		}
	}
	return -1;
}

int AlphaThread::fetch(unsigned _idx, char *_pb){
	wr<<"s2 fetch "<<sdq[_idx];
	//cout<<_idx<<" "<<sdq[_idx]<<endl;
	if(addr.size()){
		wr<<" \""<<addr<<"\" "<<(uint32)port;
	}
	wr<<crlf;
	if(wr.flush()) return -1;
	enum{
		StartLine, FirstSpace, SecondSpace, LiteralStart,LiteralNumber,
		ReadCR, ReadLF, ReadLit, ReadLitFinalCR, ReadLitFinalLF,
		SkipLine, SkipLineLF, ReadFinalCR, ReadFinalLF
	};
	int rc;
	int state = StartLine;
	const char *bpos;
	const char *bend;
	const char *bp;
	string lit;
	ulong litlen = 0;
	
	while((rc = read(sd, _pb, BufLen - 1)) > 0){
		readc += rc;
		inf.update(pos, readc);
/*		idbg("-----------------------------");
		writedbg(_pb, rc);
		idbg("=============================");*/
		bool b = true;
		bpos = _pb;
		bend = bpos + rc;
		_pb[rc] = '\0';
		//cout<<'[';cout.write(_pb, rc);cout<<']'<<endl;
		while(b){
			switch(state){
				case StartLine:
					if(!*bpos){b = false; break;}
					if(*bpos != '*'){state = SkipLine; break;}
					++bpos;
					state = FirstSpace;
				case FirstSpace:
					if(!*bpos){b = false; break;}
					if(*bpos != ' '){state = SkipLine; break;}
					++bpos;
					state = SecondSpace;
				case SecondSpace://after the size
					bp = find<const char,' ','\0'>(bpos);
					if(*bp){
						bpos = bp + 1;
						sdq.push_back("");
						state = LiteralStart;
					}else{
						b = false; break;
					}
				case LiteralStart:
					if(!*bpos){b = false; break;}
					if(*bpos != '{'){state = SkipLine; break;}
					++bpos;
					lit.clear();
					state = LiteralNumber;
				case LiteralNumber:
					bp = find<const char,'}','\0'>(bpos);
					lit.append(bpos, bp - bpos);
					if(*bp){
						bpos = bp + 1;
						state = ReadCR;
						litlen = atoi(lit.c_str());
					}else{
						b = false; break;
					}
				case ReadCR:
					if(!*bpos){b = false; break;}
					if(*bpos != '\r') return -2;
					++bpos;
					state = ReadLF;
				case ReadLF:
					if(!*bpos){b = false; break;}
					if(*bpos != '\n') return -3;
					++bpos;
					state = ReadLit;
				case ReadLit:
					if(litlen){
						if(bpos == bend){b = false; break;}
						int toread = bend - bpos;
						if(toread > (int)litlen) toread = litlen;
						//cout.write(bpos, toread);
						bpos += toread;
						litlen -= toread;
						break;
					}else{
						state = ReadFinalCR;
					}
				case ReadLitFinalCR:
					if(!*bpos){b = false; break;}
					if(*bpos != '\r'){cassert(false); return -4;}
					++bpos;
					state = ReadFinalLF;
				case ReadLitFinalLF:
					if(!*bpos){b = false; break;}
					if(*bpos != '\n') return -5;
					++bpos;
					state = StartLine;
					break;
				case SkipLine:
					bp = find<const char, '\r', '@', '\0'>(bpos);
					if(!*bp){b = false; break;}
					if(*bp == '\r'){
						bpos = bp + 1;
						state = SkipLineLF;
						break;
					}
					if(*bp == '@'){
						state = ReadFinalCR;
						bpos = bp + 1;
						break;
					}
					break;
				case SkipLineLF:
					if(!*bpos){b = false; break;}
					if(*bpos != '\n'){return -6;}
					++bpos;
					state = StartLine;
					break;
				case ReadFinalCR:
					if(!*bpos){b = false; break;}
					if(*bpos != '\r'){return -7;}
					++bpos;
					state = ReadFinalLF;
				case ReadFinalLF:
					if(!*bpos){b = false; break;}
					if(*bpos != '\n'){return -8;}
					++bpos;
					//state = ReadFinalLF;
					return 0;
			}
		}
	}
	cout<<"err "<<strerror(errno)<<endl;
	cout.write(_pb, BufLen);
	cout.flush();
	return -9;
}

// int test_a(){
// 	cout<<"first function"<<endl;
// 	return 0;
// }
// int test_b(){
// 	cout<<"second function"<<endl;
// 	return 1;
// }
// 
// void test(int _a, int _b){
// 	cout<<"test"<<endl;
// }
// 
// struct A{
// 	A(int _v):v(_v){
// 		cout<<"A("<<_v<<")"<<endl;
// 	}
// 	~A(){
// 		cout<<"~A("<<v<<")"<<endl;
// 	}
// 	int v;
// };
// 
// A operator+(const A& a1, const A &a2){
// 	return A(a1.v + a2.v);
// }

int main(int argc, char *argv[]){
	if(argc != 7 && argc != 9){
//		test(test_a(), test_b());
// 		int c(test_a() + test_b());
// 		cout<<(A(3) + A(4)).v<<endl;
		cout<<"Plain alpha connection stress test"<<endl;
		cout<<"Usage: alphaclient thcnt addr port path tout repeat_count [peer_addr peer_port]"<<endl;
		cout<<"Where:"<<endl;
		cout<<"tout is the amount of time in msec between commands"<<endl;
		cout<<"if the given port is < 1200 then the connection is considered through a proxy server"<<endl;
		return 0;
	}
	signal(SIGPIPE, SIG_IGN);
	Thread::init();
	
#ifdef UDEBUG
	{
	string s;
	Dbg::instance().levelMask("iew");
	Dbg::instance().moduleMask();
	Dbg::instance().initStdErr(false, &s);
	cout<<"Debug output: "<<s<<endl;
	s.clear();
	Dbg::instance().moduleBits(s);
	cout<<"Debug bits: "<<s<<endl;
	}
#endif
	
	sleeptout = atoi(argv[5]);
	int cnt = atoi(argv[1]);
	int repeatcnt = atoi(argv[6]);
	const char* addr = NULL;
	int port = -1;
	if(argc == 9){
		addr = argv[7];
		port = atoi(argv[8]);
	}
	for(int i = 0; i < cnt; ++i){
		AlphaThread *pt = new AlphaThread(argv[2], argv[3], argv[4], inf.pushBack(), addr, port, repeatcnt);
		inf.addWait();
		pt->start(true, true, 24*1024);
	}
	//clock_gettime(CLOCK_MONOTONIC, &inf.ft);
	inf.ft.currentMonotonic();
	while(inf.print()){
		Thread::sleep(500);
	}
	Thread::waitAll();
	cout<<"done"<<endl;
	return 0;
}


