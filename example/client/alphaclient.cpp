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

#include "boost/program_options.hpp"

using namespace std;
//typedef unsigned long long uint64;
enum {BSIZE = 1024};

static SSL_CTX *sslctx = NULL;

struct Params{
	string		dbg_levels;
	string		dbg_modules;
	string		dbg_addr;
	string		dbg_port;
	bool		dbg_buffered;
	bool		dbg_console;
	
	uint32		con_cnt;
	string		srv_addr;
	string		srv_port;
	string		path;
	uint32		sleep;
	uint32		repeat;
	string		proxy_addr;
	string		proxy_port;
	string		peer_addr;
	string		peer_port;
	string		gateway_addr;
	string		gateway_port;
	uint32		gateway_id;
	bool		ssl;
};

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
		const Params &_rp,
		unsigned _pos,
		int _thrid = 0,
		int _cnt = ((unsigned)(0xfffffff))
	):
		rp(_rp),
		rd(synchronous_resolve(
			_rp.proxy_addr.size() ? _rp.proxy_addr.c_str() : _rp.srv_addr.c_str(),
		   _rp.proxy_port.size() ? _rp.proxy_port.c_str() : _rp.srv_port.c_str(),
			0, SocketInfo::Inet4, SocketInfo::Stream
		)),
		wr(-1),sd(-1),cnt(_cnt),
		pos(_pos), pssl(NULL){
			repeatcnt = _rp.repeat;
			thrid = _thrid;
		}
	void run();
private:
	int read(char *_pb, unsigned _bl){
		if(pssl){
			return SSL_read(pssl, _pb, _bl);
		}else{
			return ::read(sd, _pb, _bl);
		}
	}
private:
	enum {BufLen = 2*1024};
	int list(char *_pb);
	int fetch(unsigned _idx, char *_pb);
	typedef std::deque<string> StrDqT;
	const Params		&rp;
	ResolveData			rd;
	Writer      		wr;
	int					sd;
	int					cnt;
	int					slp;
	StrDqT				sdq;
	unsigned			pos;
	ulong				readc;
	int					repeatcnt;
	SSL					*pssl;
	int					thrid;
	deque<int>			states;
	deque<uint32>		literals;
};

void AlphaThread::run(){
	if(rd.empty()){
		idbg("No such address");
		return;
	}
	ResolveIterator it(rd.begin());
	sd = socket(it.family(), it.type(), it.protocol());
	if(sd < 0){
		idbg("error creating socket");
		cout<<"failed socket "<<pos<<endl;
		return;
	}
	//cout<<"before connect "<<pos<<endl;
	if(connect(sd, it.sockAddr(), it.size())){
		idbg("failed connect");
		cout<<"failed connect "<<pos<<": "<<strerror(errno)<<endl;
		return;
	}
	inf.lock();
	cout<<pos<<" connected"<<endl;
	inf.doneConnect();
	inf.unlock();
	int rv;
	//do ssl stuffs
	if(rp.ssl){
		pssl = SSL_new(sslctx);
		SSL_set_fd(pssl, sd);
		rv = SSL_connect(pssl);
		if(rv <= 0){
			cout<<"error ssl connect"<<endl;
			return;
		}
	}
	
	//timeval tv;
// 	memset(&tv, 0, sizeof(timeval));
// 	tv.tv_sec = 30;
// 	setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
// 	setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	readc = 0;
	wr.reinit(sd, pssl);
	char buf[BufLen];
	if(rp.proxy_addr.size()){
		cout<<"Using proxy..."<<endl;
		wr<<rp.srv_addr.c_str()<<' ';
		wr<<rp.srv_port.c_str()<<crlf;
		wr.flush();
	}
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
			Thread::sleep(rp.sleep);
		}
	}
	idbg("return value "<<rv);
	cout<<endl<<"return value"<<rv<<endl;
	inf.subWait();
	close(sd);
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
	if(rp.peer_addr.size()){
		//remote list
		wr<<"s1 remotelist \""<<rp.path.c_str()<<"\" \""<<rp.peer_addr.c_str()<<"\" "<<rp.peer_port.c_str()<<crlf;
	}else if(rp.gateway_addr.size()){
		wr<<"s1 remotelist \""<<rp.path.c_str()<<"\" \"";
		wr<<rp.peer_addr.c_str()<<"\" \""<<rp.peer_port.c_str()<<"\" \"";
		wr<<rp.gateway_addr.c_str()<<"\" \""<<rp.gateway_port.c_str()<<"\" ";
		wr<<rp.gateway_id<<crlf;
	}else{
		//local list
		wr<<"s1 list \""<<rp.path.c_str()<<'\"'<<crlf;
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
	while((rc = this->read(_pb, BufLen - 1)) > 0){
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
	if(rp.peer_addr.size()){
		//remote list
		wr<<" \""<<rp.peer_addr.c_str()<<"\" "<<rp.peer_port.c_str();
	}else if(rp.gateway_addr.size()){
		wr<<" \""<<rp.peer_addr.c_str()<<"\" \""<<rp.peer_port.c_str()<<"\" \"";
		wr<<rp.gateway_addr.c_str()<<"\" \""<<rp.gateway_port.c_str()<<"\" ";
		wr<<rp.gateway_id;
	}else{
	}
	wr<<crlf;
	if(wr.flush()) return -1;
	enum{
		StartLine, FirstSpace, SecondSpace, LiteralStart,LiteralNumber,
		ReadCR, ReadLF, ReadLit/*7*/, ReadLitFinalCR, ReadLitFinalLF,
		SkipLine/*10*/, SkipLineLF, ReadFinalCR/*12*/, ReadFinalLF
	};
	int rc;
	int state = StartLine;
	const char *bpos;
	const char *bend;
	const char *bp;
	string lit;
	ulong litlen = 0;
	states.push_back(-1);
	while((rc = this->read(_pb, BufLen - 1)) > 0){
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
			if(states.empty() || states.back() != state){
				states.push_back(state);
			}
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
						literals.push_back(litlen);
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
	//if(thrid == 1) cout<<"exit"<<endl;
	cout<<"err "<<strerror(errno)<<endl;
	cout.write(_pb, BufLen);
	cout.flush();
	return -9;
}

const char *certificate_path(){
	const char	*path = OSSL_SOURCE_PATH;
	const size_t path_len = strlen(path);
	if(path_len){
		if(path[path_len - 1] == '/'){
			return OSSL_SOURCE_PATH"ssl_/certs/A-client.pem";
		}else{
			return OSSL_SOURCE_PATH"/ssl_/certs/A-client.pem";
		}
	}else return "A-client.pem";
}

bool parseArguments(Params &_par, int argc, char *argv[]);

int main(int argc, char *argv[]){
	Params	p;
	
	if(parseArguments(p, argc, argv)) return 0;
	
	signal(SIGPIPE, SIG_IGN);
	Thread::init();
	
#ifdef UDEBUG
	{
	string dbgout;
	Dbg::instance().levelMask(p.dbg_levels.c_str());
	Dbg::instance().moduleMask(p.dbg_modules.c_str());
	if(p.dbg_addr.size() && p.dbg_port.size()){
		Dbg::instance().initSocket(
			p.dbg_addr.c_str(),
			p.dbg_port.c_str(),
			p.dbg_buffered,
			&dbgout
		);
	}else if(p.dbg_console){
		Dbg::instance().initStdErr(
			p.dbg_buffered,
			&dbgout
		);
	}else{
		Dbg::instance().initFile(
			*argv[0] == '.' ? argv[0] + 2 : argv[0],
			p.dbg_buffered,
			3,
			1024 * 1024 * 64,
			&dbgout
		);
	}
	cout<<"Debug output: "<<dbgout<<endl;
	dbgout.clear();
	Dbg::instance().moduleBits(dbgout);
	cout<<"Debug modules: "<<dbgout<<endl;
	}
#endif
	if(p.ssl){
		//Initializing OpenSSL
		//------------------------------------------
		SSL_library_init();
		SSL_load_error_strings();
		ERR_load_BIO_strings();
		OpenSSL_add_all_algorithms();
		
		sslctx = SSL_CTX_new(SSLv23_client_method());
		
		//const char *pcertpath = "../../../../extern/linux/openssl/demos/tunala/A-client.pem";
		const char *pcertpath = certificate_path();
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
	}
	//------------------------------------------
	//done with ssl context stuff
	
	for(int i = 0; i < p.con_cnt; ++i){
		AlphaThread *pt = new AlphaThread(p, inf.pushBack(), i);
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

bool parseArguments(Params &_par, int argc, char *argv[]){
	using namespace boost::program_options;
	try{
		options_description desc("alphaclient_p application");
		desc.add_options()
			("help,h", "List program options")
			//("base_port,b", value<int>(&_par.start_port)->default_value(1000), "Base port")
			("debug_levels,L", value<string>(&_par.dbg_levels)->default_value("view"),"Debug logging levels")
			("debug_modules,M", value<string>(&_par.dbg_modules),"Debug logging modules")
			("debug_addr,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")
			("debug_port,P", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")
			("debug_console,C", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")
			("debug_unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered")
			("connection_count,c", value<uint32>(&_par.con_cnt)->default_value(1), "The number of connections to create")
			("server_addr,s", value<string>(&_par.srv_addr)->default_value("localhost"), "The address of the server")
			("server_port,p", value<string>(&_par.srv_port)->default_value("1114"), "The port of the server")
			("path", value<string>(&_par.path), "The path on the server")
			("sleep", value<uint32>(&_par.sleep)->default_value(1), "The number of milliseconds to sleep between commands")
			("repeat", value<uint32>(&_par.repeat)->default_value(1), "Repeat count")
			("proxy_addr", value<string>(&_par.proxy_addr), "The address of the proxy server")
			("proxy_port", value<string>(&_par.proxy_port), "The port of the proxy server")
			("peer_addr", value<string>(&_par.peer_addr), "The ipc address of the peer alpha server")
			("peer_port", value<string>(&_par.peer_port), "The ipc port of the  peer alpha server")
			("gateway_addr", value<string>(&_par.gateway_addr), "The address of the ipc gateway")
			("gateway_port", value<string>(&_par.gateway_port), "The port of the ipc gateway")
			("gateway_id", value<uint32>(&_par.gateway_id)->default_value(1), "The id of the network id on the gateway")
			("ssl", value<bool>(&_par.ssl)->implicit_value(true)->default_value(false), "Use SSL")
	/*		("verbose,v", po::value<int>()->implicit_value(1),
					"enable verbosity (optionally specify level)")*/
	/*		("listen,l", po::value<int>(&portnum)->implicit_value(1001)
					->default_value(0,"no"),
					"listen on a port.")
			("include-path,I", po::value< vector<string> >(),
					"include path")
			("input-file", po::value< vector<string> >(), "input file")*/
		;
		variables_map vm;
		store(parse_command_line(argc, argv, desc), vm);
		notify(vm);
		if (vm.count("help")) {
			cout << desc << "\n";
			return true;
		}
		return false;
	}catch(exception& e){
		cout << e.what() << "\n";
		return true;
	}
}

