#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
#include "frame/objectselector.hpp"

#include "frame/aio/aioselector.hpp"
#include "frame/aio/aiosingleobject.hpp"
#include "frame/aio/openssl/opensslsocket.hpp"

#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"

#include "boost/program_options.hpp"

#include <signal.h>
#include <iostream>

using namespace std;
using namespace solid;

typedef frame::IndexT							IndexT;
typedef frame::Scheduler<frame::aio::Selector>	AioSchedulerT;
typedef frame::Scheduler<frame::ObjectSelector>	SchedulerT;

//------------------------------------------------------------------
//------------------------------------------------------------------

struct Params{
	int			start_port;
	string		dbg_levels;
	string		dbg_modules;
	string		dbg_addr;
	string		dbg_port;
	bool		dbg_console;
	bool		dbg_buffered;
	bool		log;
};

namespace{
	Mutex					mtx;
	Condition				cnd;
	bool					run(true);
}

static void term_handler(int signum){
    switch(signum) {
		case SIGINT:
		case SIGTERM:{
			if(run){
				Locker<Mutex>  lock(mtx);
				run = false;
				cnd.broadcast();
			}
		}
    }
}
//------------------------------------------------------------------
//------------------------------------------------------------------

class Listener: public Dynamic<Listener, frame::aio::SingleObject>{
public:
	Listener(frame::Manager &_rm, AioSchedulerT &_rsched, const SocketDevice &_rsd, frame::aio::openssl::Context *_pctx = NULL);
	~Listener();
private:
	/*virtual*/ void execute(ExecuteContext &_rexectx);
	
	typedef std::auto_ptr<frame::aio::openssl::Context> SslContextPtrT;
	int					state;
	SocketDevice		sd;
	SslContextPtrT		pctx;
	frame::Manager		&rm;
	AioSchedulerT		&rsched;
};

//------------------------------------------------------------------
//------------------------------------------------------------------
class Connection: public Dynamic<Connection, frame::aio::SingleObject>{
public:
	Connection(const char *_node, const char *_srv);
	Connection(const SocketDevice &_rsd);
	~Connection();
private:
	/*virtual*/ void execute(ExecuteContext &_rexectx);
	
private:
	enum {BUFSZ = 1024};
	enum {INIT,READ, READ_TOUT, WRITE, WRITE_TOUT, CONNECT, CONNECT_TOUT};
	int							state;
	char						bbeg[BUFSZ];
	const char					*bend;
	char						*brpos;
	const char					*bwpos;
	ResolveData					rd;
	ResolveIterator				it;
	bool						b;
};

//------------------------------------------------------------------
//------------------------------------------------------------------

class Talker: public Dynamic<Talker, frame::aio::SingleObject>{
public:
	Talker(const SocketDevice &_rsd);
	~Talker();
private:
	/*virtual*/ void execute(ExecuteContext &_rexectx);
private:
	enum {BUFSZ = 1024};
	enum {INIT,READ, READ_TOUT, WRITE, WRITE_TOUT, WRITE2, WRITE_TOUT2};
	int				state;
	char			bbeg[BUFSZ];
	solid::uint		sz;
	ResolveData		rd;
};

//------------------------------------------------------------------
//------------------------------------------------------------------

bool parseArguments(Params &_par, int argc, char *argv[]);
void insertListener(frame::Manager &_rm, AioSchedulerT &_rsched, const char *_addr, int _port, bool _secure);
void insertTalker(frame::Manager &_rm, AioSchedulerT &_rsched, const char *_addr, int _port);
//------------------------------------------------------------------
//------------------------------------------------------------------

int main(int argc, char *argv[]){
	Params p;
	if(parseArguments(p, argc, argv)) return 0;
	
	signal(SIGINT,term_handler); /* Die on SIGTERM */
	
	/*solid::*/Thread::init();
	
#ifdef UDEBUG
	{
	string dbgout;
	Debug::the().levelMask(p.dbg_levels.c_str());
	Debug::the().moduleMask(p.dbg_modules.c_str());
	if(p.dbg_addr.size() && p.dbg_port.size()){
		Debug::the().initSocket(
			p.dbg_addr.c_str(),
			p.dbg_port.c_str(),
			p.dbg_buffered,
			&dbgout
		);
	}else if(p.dbg_console){
		Debug::the().initStdErr(
			p.dbg_buffered,
			&dbgout
		);
	}else{
		Debug::the().initFile(
			*argv[0] == '.' ? argv[0] + 2 : argv[0],
			p.dbg_buffered,
			3,
			1024 * 1024 * 64,
			&dbgout
		);
	}
	cout<<"Debug output: "<<dbgout<<endl;
	dbgout.clear();
	Debug::the().moduleNames(dbgout);
	cout<<"Debug modules: "<<dbgout<<endl;
	}
#endif
	
	{
		frame::Manager	m;
		AioSchedulerT	aiosched(m);
		
		insertListener(m, aiosched, "0.0.0.0", p.start_port + 111, false);
		insertTalker(m, aiosched, "0.0.0.0", p.start_port + 112);
		
		
		cout<<"Here some examples how to test: "<<endl;
		cout<<"For tcp:"<<endl;
		cout<<"\t$ nc localhost 2111"<<endl;
		cout<<"For udp:"<<endl;
		cout<<"\t$ nc -u localhost 2112"<<endl;
		
		if(0){
			Locker<Mutex>	lock(mtx);
			while(run){
				cnd.wait(lock);
			}
		}else{
			char c;
			cin>>c;
		}
		m.stop();
		
	}
	/*solid::*/Thread::waitAll();
	
	return 0;
}
//------------------------------------------------------------------
//------------------------------------------------------------------

void insertListener(frame::Manager &_rm, AioSchedulerT &_rsched, const char *_addr, int _port, bool _secure){
	ResolveData		rd =  synchronous_resolve(_addr, _port, 0, SocketInfo::Inet4, SocketInfo::Stream);
	
	SocketDevice	sd;
	
	sd.create(rd.begin());
	sd.makeNonBlocking();
	sd.prepareAccept(rd.begin(), 100);
	if(!sd.ok()){
		cout<<"error creating listener"<<endl;
		return;
	}
	
	frame::aio::openssl::Context *pctx(NULL);
	
	if(_secure){
		pctx = frame::aio::openssl::Context::create();
	}
	if(pctx){
		const char *pcertpath(OSSL_SOURCE_PATH"ssl_/certs/A-server.pem");
		
		pctx->loadCertificateFile(pcertpath);
		pctx->loadPrivateKeyFile(pcertpath);
	}
	
	DynamicPointer<Listener> lsnptr(new Listener(_rm, _rsched, sd, pctx));
	
	_rm.registerObject(*lsnptr);
	_rsched.schedule(lsnptr);
	
	cout<<"inserted listener on port "<<_port<<endl;
}
void insertTalker(frame::Manager &_rm, AioSchedulerT &_rsched, const char *_addr, int _port){
	ResolveData		rd =  synchronous_resolve(_addr, _port, 0, SocketInfo::Inet4, SocketInfo::Datagram);
	SocketDevice	sd;
	
	sd.create(rd.begin());
	sd.bind(rd.begin());
	
	if(!sd.ok()){
		cout<<"error creating talker"<<endl;
		return;
	}
	
	DynamicPointer<Talker> tkrptr(new Talker(sd));
	
	_rm.registerObject(*tkrptr);
	_rsched.schedule(tkrptr);
	
	cout<<"inserted talker on port "<<_port<<endl;
}
//------------------------------------------------------------------
//------------------------------------------------------------------
bool parseArguments(Params &_par, int argc, char *argv[]){
	using namespace boost::program_options;
	try{
		options_description desc("SolidFrame concept application");
		desc.add_options()
			("help,h", "List program options")
			("base_port,b", value<int>(&_par.start_port)->default_value(2000),"Base port")
			("debug-levels,L", value<string>(&_par.dbg_levels)->default_value("iew"),"Debug logging levels")
			("debug-modules,M", value<string>(&_par.dbg_modules),"Debug logging modules")
			("debug-address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")
			("debug-port,P", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")
			("debug-console,C", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")
			("debug-unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered")
			("use-log,l", value<bool>(&_par.log)->implicit_value(true)->default_value(false), "Debug buffered")
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
//------------------------------------------------------------------
//------------------------------------------------------------------
Listener::Listener(
	frame::Manager &_rm,
	AioSchedulerT &_rsched,
	const SocketDevice &_rsd,
	frame::aio::openssl::Context *_pctx
):BaseT(_rsd, true), pctx(_pctx), rm(_rm), rsched(_rsched){
	state = 0;
}

Listener::~Listener(){
}

/*virtual*/ void Listener::execute(ExecuteContext &_rexectx){
	idbg("here");
	cassert(this->socketOk());
	if(notified()){
		//Locker<Mutex>	lock(this->mutex());
		ulong sm = this->grabSignalMask();
		if(sm & frame::S_KILL){
			_rexectx.close();
			return;
		}
	}
	solid::uint cnt(10);
	while(cnt--){
		if(state == 0){
			switch(this->socketAccept(sd)){
				case frame::aio::AsyncError:
					_rexectx.close();
					return;
				case frame::aio::AsyncSuccess:break;
				case frame::aio::AsyncWait:
					state = 1;
					return;
			}
		}
		state = 0;
		cassert(sd.ok());
		//TODO: one may do some filtering on sd based on sd.remoteAddress()
		if(pctx.get()){
			DynamicPointer<Connection> conptr(new Connection(sd));
			rm.registerObject(*conptr);
			rsched.schedule(conptr);
		}else{
			DynamicPointer<Connection> conptr(new Connection(sd));
			rm.registerObject(*conptr);
			rsched.schedule(conptr);
		}
	}
	_rexectx.reschedule();
}

//------------------------------------------------------------------
//------------------------------------------------------------------
static const char	*hellostr = "Welcome to echo service!!!\r\n"; 

Connection::Connection(const char *_node, const char *_srv): 
	BaseT(), bend(bbeg + BUFSZ), brpos(bbeg), bwpos(bbeg), b(false)
{
	cassert(_node && _srv);
	rd = synchronous_resolve(_node, _srv);
	it = rd.begin();
	state = CONNECT;
	
}
Connection::Connection(const SocketDevice &_rsd):
	BaseT(_rsd), bend(bbeg + BUFSZ), brpos(bbeg), bwpos(bbeg), b(false)
{
	state = INIT;
}
Connection::~Connection(){
	//state(-1);
	idbg("");
}

/*virtual*/ void Connection::execute(ExecuteContext &_rexectx){
	idbg("time.sec "<<_rexectx.currentTime().seconds()<<" time.nsec = "<<_rexectx.currentTime().nanoSeconds());
	if(_rexectx.eventMask() & (frame::EventTimeout | frame::EventDoneError | frame::EventTimeoutRecv | frame::EventTimeoutSend)){
		idbg("connecton timeout or error");
// 		if(state == CONNECT_TOUT){
// 			if(++it){
// 				state = CONNECT;
// 				return frame::UNREGISTER;
// 			}
// 		}
		_rexectx.close();
		return;
	}

	if(notified()){
		//Locker<Mutex>	lock(mutex());
		ulong sm = grabSignalMask();
		if(sm & frame::S_KILL){
			_rexectx.close();
			return;
		}
	}
	const uint32 sevs(socketEventsGrab());
	if(sevs){
		if(sevs & frame::EventDoneError){
			_rexectx.close();
			return;
		}
		if(state == READ_TOUT){	
			cassert(sevs & frame::EventDoneRecv);
		}else if(state == WRITE_TOUT){	
			cassert(sevs & frame::EventDoneSend);
		}
		
	}
	int rc = 512 * 1024;
	do{
		switch(state){
			case READ:
				switch(socketRecv(bbeg, BUFSZ)){
					case frame::aio::AsyncError:
						_rexectx.close();
						return;
					case frame::aio::AsyncSuccess: break;
					case frame::aio::AsyncWait: 
						state = READ_TOUT; 
						socketTimeoutRecv(30);
						return;
				}
			case READ_TOUT:
				state = WRITE;
			case WRITE:
				switch(socketSend(bbeg, socketRecvSize())){
					case frame::aio::AsyncError:
						_rexectx.close();
						return;
					case frame::aio::AsyncSuccess: break;
					case frame::aio::AsyncWait: 
						state = WRITE_TOUT;
						socketTimeoutSend(10);
						return;
				}
			case WRITE_TOUT:
				state = READ;
				break;
			case CONNECT:
				switch(socketConnect(it)){
					case frame::aio::AsyncError:
						++it;
						if(it != rd.end()){
							state = CONNECT;
							_rexectx.reschedule();
							return;
						}
						_rexectx.close();
						return;
					case frame::aio::AsyncSuccess:  state = INIT;break;
					case frame::aio::AsyncWait: state = CONNECT_TOUT; return;
				};
				break;
			case CONNECT_TOUT:
				rd.clear();
			case INIT:
				//socketSend(hellostr, strlen(hellostr));
				state = READ;
				break;
		}
		rc -= socketRecvSize();
	}while(rc > 0);
	_rexectx.reschedule();
}

//------------------------------------------------------------------
//------------------------------------------------------------------
/*
Talker::Talker(const char *_node, const char *_srv){
	if(_node){
		rd = synchronous_resolve(_node, _srv);
		strcpy(bbeg, hellostr);
		sz = strlen(hellostr);
		state(INIT);
	}
}*/

Talker::Talker(const SocketDevice &_rsd):BaseT(_rsd){
	state = READ;
}

Talker::~Talker(){
}

/*virtual*/ void Talker::execute(ExecuteContext &_rexectx){
	idbg("time.sec "<<_rexectx.currentTime().seconds()<<" time.nsec = "<<_rexectx.currentTime().nanoSeconds());
	if(_rexectx.eventMask() & (frame::EventTimeout | frame::EventDoneError | frame::EventTimeoutRecv | frame::EventTimeoutSend)){
		_rexectx.close();
		return;
	}
	if(notified()){
		//Locker<Mutex>	lock(mutex());
		ulong sm = grabSignalMask(0);
		if(sm & frame::S_KILL){
			_rexectx.close();
			return;
		}
	}
	const uint32 sevs(socketEventsGrab());
	if(sevs & frame::EventDoneError){
		_rexectx.close();
		return;
	}
	int rc = 512 * 1024;
	do{
		switch(state){
			case READ:
				switch(socketRecvFrom(bbeg, BUFSZ)){
					case frame::aio::AsyncError:
						_rexectx.close();
						return;
					case frame::aio::AsyncSuccess: state = READ_TOUT;break;
					case frame::aio::AsyncWait:
						socketTimeoutRecv(5 * 60);
						state = READ_TOUT; 
						return;
				}
			case READ_TOUT:
				state = WRITE;
			case WRITE:
				//sprintf(bbeg + socketRecvSize() - 1," [%u:%d]\r\n", (unsigned)_tout.seconds(), (int)_tout.nanoSeconds());
				switch(socketSendTo(bbeg, socketRecvSize(), socketRecvAddr())){
					case frame::aio::AsyncError:
						_rexectx.close();
						return;
					case frame::aio::AsyncSuccess: break;
					case frame::aio::AsyncWait: state = WRITE_TOUT;
						socketTimeoutSend(5 * 60);
						return;
				}
			case WRITE_TOUT:
				if(socketHasPendingSend()){
					socketTimeoutSend(_rexectx.currentTime(), 5 * 60);
					return;
				}else{
					state = READ;
					_rexectx.reschedule();
					return;
				}
			case INIT:
				if(rd.empty()){
					idbg("Invalid address");
					_rexectx.close();
					return;
				}
				ResolveIterator it(rd.begin());
				switch(socketSendTo(bbeg, sz, SocketAddressStub(it))){
					case frame::aio::AsyncError:
						_rexectx.close();
						return;
					case frame::aio::AsyncSuccess: state = READ; break;
					case frame::aio::AsyncWait:
						state = WRITE_TOUT;
						return;
				}
				break;
		}
		rc -= socketRecvSize();
	}while(rc > 0);
	_rexectx.reschedule();
}


//------------------------------------------------------------------
//------------------------------------------------------------------

