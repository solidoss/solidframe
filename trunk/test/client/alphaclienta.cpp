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
//#include "common/utils.h"
#include "writer.hpp"

using namespace std;
//typedef unsigned long long uint64;
enum {BSIZE = 1024};
static int sleeptout = 0;

class Info{
public:
	void update(unsigned _pos, ulong _v);
	unsigned pushBack();
	void print();
	TimeSpec		ft;
private:
	vector<ulong>   v;
	Mutex           m;
	TimeSpec		ct;
};

unsigned Info::pushBack(){
	v.push_back(0xffffffff);
	return v.size() - 1;
}

void Info::update(unsigned _pos, ulong _v){
	//m.lock();
	v[_pos] = _v;
	//m.unlock();
}

void Info::print(){
	uint64	tot = 0;
	uint32	mn = 0xffffffff;
	uint32	mncnt = 0;
	uint32	mx = 0;
	uint32	mxcnt = 0;
	uint32	notconnected = 0;
	//m.lock();
	for(vector<ulong>::const_iterator it(v.begin()); it != v.end(); ++it){
		//cout<<(*it/1024)<<'k'<<' ';
		const uint32 t = *it;
		if(t == 0xffffffff){ ++notconnected; continue;}
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
	clock_gettime(CLOCK_MONOTONIC, &ct);
	cout<<"speed = "<<tot/(ct.seconds() - ft.seconds() + 1)<<"k/s avg = "<<tot/v.size()<<"k min = "<<mn<<"k ("<<mncnt<<") max = "<<mx<<"k ("<<mxcnt<<')';
	if(notconnected) cout<<" notconected = "<<notconnected<<'\r'<<flush;
	else cout<<'\r'<<flush;
	//m.unlock();
}

static Info inf;

class AlphaThread: public Thread{
public:
	AlphaThread(
		const char *_node, 
		const char* _svice, 
		const char *_path,
		unsigned _pos,
		int _cnt = ((unsigned)(0xfffffff)),
		int _sleep = 1):wr(-1),sd(-1),ai(_node, _svice), pos(_pos), cnt(_cnt),slp(_sleep),path(_path){}
	void run();
private:
	enum {BufLen = 2*1024};
	int list(char *_pb);
	int fetch(unsigned _idx, char *_pb);
	typedef std::deque<string> StrDqTp;
	AddrInfo    ai;
	Writer      wr;
	int         sd;
	int         cnt;
	int         slp;
	const char  *path;
	StrDqTp     sdq;
	unsigned    pos;
	ulong       readc;
};

void AlphaThread::run(){
	if(ai.empty()){
		idbg("No such address");
		return;
	}
	AddrInfoIterator it(ai.begin());
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
	timeval tv;
// 	memset(&tv, 0, sizeof(timeval));
// 	tv.tv_sec = 30;
// 	setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
// 	setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	readc = 0;
	wr.reinit(sd);
	char buf[BufLen];
	int rv = list(buf);
	idbg("return value "<<rv);
	inf.update(pos, readc);
	ulong m = sdq.size() - 1;
	cout<<"connected "<<pos<<" filecnt "<<m<<endl;
// 	for(StrDqTp::const_iterator it(sdq.begin()); it != sdq.end(); ++it){
// 		cout<<'['<<*it<<']'<<endl;
// 	}
	if(rv) return;
	ulong ul = pos;
	while(!(rv = fetch(ul % m, buf))){
		++ul;
		Thread::sleep(sleeptout);
	}
	idbg("return value "<<rv);
	cout<<endl<<"return value"<<rv<<endl;
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
	wr<<"s1 list \""<<path<<'\"'<<crlf;
	if(wr.flush()) return -1;
	enum {
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
	int state = StartLine;
	const char *bpos;
	const char *bend;
	const char *bp;
	while((rc = read(sd, _pb, BufLen - 1)) > 0){
		bool b = true;
		readc += rc;
		bpos = _pb;
		bend = bpos + rc;
		_pb[rc] = '\0';
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
	wr<<"s2 fetch "<<sdq[_idx]<<crlf;
	//cout<<_idx<<" "<<sdq[_idx]<<endl;
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
	ulong litlen;
	
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
						if(toread > litlen) toread = litlen;
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

int main(int argc, char *argv[]){
	if(argc != 6){
		cout<<"Usage: alphaclient thcnt addr port path tout"<<endl;
		cout<<"Where:"<<endl;
		cout<<"tout is the amount of time in msec between commands"<<endl;
		return 0;
	}
	signal(SIGPIPE, SIG_IGN);
	Thread::init();
#ifdef UDEBUG
	{
	string s = "dbg/";
	s+= argv[0]+2;
	initDebug(s.c_str());
	}
#endif
	sleeptout = atoi(argv[5]);
	int cnt = atoi(argv[1]);
	for(int i = 0; i < cnt; ++i){
		AlphaThread *pt = new AlphaThread(argv[2], argv[3], argv[4],inf.pushBack());
		pt->start(true, true, 24*1024);
	}
	clock_gettime(CLOCK_MONOTONIC, &inf.ft);
	while(true){
		inf.print();
		Thread::sleep(500);
	}
	Thread::waitAll();
	cout<<"done"<<endl;
	return 0;
}


