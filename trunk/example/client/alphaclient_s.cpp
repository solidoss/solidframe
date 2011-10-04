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

#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"

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

static SSL_CTX *sslctx = NULL;

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
		int _sleep = 1):ai(_node, _svice), wr(-1),sd(-1), cnt(_cnt), slp(_sleep),path(_path),pos(_pos),addr(_addr?_addr:""),
		port(_port),repeatcnt(_repeatcnt),pssl(NULL){}
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
	StrDqT     sdq;
	unsigned    pos;
	ulong       readc;
	string		addr;
	int 		port;
	int			repeatcnt;
	SSL			*pssl;
};

void AlphaThread::run(){
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
	
	//do ssl stuffs
	pssl = SSL_new(sslctx);
	SSL_set_fd(pssl, sd);
	int rv = SSL_connect(pssl);
	if(rv <= 0){
		cout<<"error ssl connect"<<endl;
		return;
	}
	
	
	//timeval tv;
// 	memset(&tv, 0, sizeof(timeval));
// 	tv.tv_sec = 30;
// 	setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
// 	setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	readc = 0;
	wr.reinit(sd, pssl);
	char buf[BufLen];
	rv = list(buf);
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
	while((rc = SSL_read(pssl, _pb, BufLen - 1)) > 0){
		bool b = true;
		readc += rc;
		inf.update(pos, readc);
		bpos = _pb;
		bend = bpos + rc;
		_pb[rc] = '\0';
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
	
	while((rc = SSL_read(pssl, _pb, BufLen - 1)) > 0){
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


int main(int argc, char *argv[]){
	if(argc != 7 && argc != 9){
		cout<<"Secure (SSL) alpha connection stress test"<<endl;
		cout<<"Usage: alphaclient thcnt addr port path tout repeat_count [peer_addr peer_port]"<<endl;
		cout<<"Where:"<<endl;
		cout<<"tout is the amount of time in msec between commands"<<endl;
		return 0;
	}
	signal(SIGPIPE, SIG_IGN);
	Thread::init();
	
#ifdef UDEBUG
	{
	string s;
	Dbg::instance().levelMask();
	Dbg::instance().moduleMask();
	Dbg::instance().initStdErr(false, &s);
	cout<<"Debug output: "<<s<<endl;
	s.clear();
	Dbg::instance().moduleBits(s);
	cout<<"Debug bits: "<<s<<endl;
	}
#endif
	
	//Initializing OpenSSL
	//------------------------------------------
	SSL_library_init();
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	OpenSSL_add_all_algorithms();
	
	sslctx = SSL_CTX_new(SSLv23_client_method());
	
	//const char *pcertpath = "../../../../extern/linux/openssl/demos/tunala/A-client.pem";
	const char *pcertpath = OSSL_SOURCE_PATH"ssl_/certs/A-client.pem";
	cout<<"Client certificate path: "<<pcertpath<<endl;
	
	if(!sslctx){
		cout<<"failed SSL_CTX_new: "<<ERR_error_string(ERR_get_error(), NULL)<<endl;
		return 0;
	}
	if(!SSL_CTX_load_verify_locations(sslctx, pcertpath, NULL)){
    	cout<<"failed SSL_CTX_load_verify_locations 1 "<<ERR_error_string(ERR_get_error(), NULL)<<endl;;
    	return 0;
	}
	system("mkdir certs");
	
	if(!SSL_CTX_load_verify_locations(sslctx, NULL, "certs")){
		cout<<"failed SSL_CTX_load_verify_locations 2 "<<ERR_error_string(ERR_get_error(), NULL)<<endl;;
		return 0;
	}
	//------------------------------------------
	//done with ssl context stuff
	
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
	inf.ft.currentMonotonic();
	while(inf.print()){
		Thread::sleep(500);
	}
	Thread::waitAll();
	cout<<"done"<<endl;
	return 0;
}


