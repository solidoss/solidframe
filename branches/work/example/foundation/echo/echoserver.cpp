#include "foundation/manager.hpp"
#include "foundation/service.hpp"
#include "foundation/scheduler.hpp"
#include "foundation/objectselector.hpp"

#include "foundation/aio/aioselector.hpp"
#include "foundation/aio/aiosingleobject.hpp"
#include "foundation/aio/openssl/opensslsocket.hpp"

#include "foundation/ipc/ipcservice.hpp"

#include "system/thread.hpp"
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"

#include "boost/program_options.hpp"

#include <iostream>

using namespace std;

namespace fdt=foundation;

typedef foundation::IndexT									IndexT;
typedef foundation::Scheduler<foundation::aio::Selector>	AioSchedulerT;
typedef foundation::Scheduler<foundation::ObjectSelector>	SchedulerT;

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
//------------------------------------------------------------------
//------------------------------------------------------------------

class Listener: public Dynamic<Listener, foundation::aio::SingleObject>{
public:
	typedef foundation::Service		ServiceT;
	Listener(const SocketDevice &_rsd, foundation::aio::openssl::Context *_pctx = NULL);
	~Listener();
	virtual int execute(ulong, TimeSpec&);
private:
	typedef std::auto_ptr<foundation::aio::openssl::Context> SslContextPtrT;
	SocketDevice		sd;
	SslContextPtrT		pctx;
};

//------------------------------------------------------------------
//------------------------------------------------------------------
class Connection: public Dynamic<Connection, foundation::aio::SingleObject>{
public:
	typedef foundation::Service	ServiceT;
	
	Connection(const char *_node, const char *_srv);
	Connection(const SocketDevice &_rsd);
	~Connection();
	int execute(ulong _sig, TimeSpec &_tout);
	
private:
	enum {BUFSZ = 4*1024};
	enum {INIT,READ, READ_TOUT, WRITE, WRITE_TOUT, CONNECT, CONNECT_TOUT};
	char						bbeg[BUFSZ];
	const char					*bend;
	char						*brpos;
	const char					*bwpos;
	SocketAddressInfo			*pai;
	SocketAddressInfoIterator	it;
	bool						b;
};

//------------------------------------------------------------------
//------------------------------------------------------------------

class Talker: public Dynamic<Talker, foundation::aio::SingleObject>{
public:
	Talker(const char *_node, const char *_srv);
	Talker(const SocketDevice &_rsd);
	~Talker();
	int execute(ulong _sig, TimeSpec &_tout);
private:
	enum {BUFSZ = 1024};
	enum {INIT,READ, READ_TOUT, WRITE, WRITE_TOUT, WRITE2, WRITE_TOUT2};
	char			bbeg[BUFSZ];
	uint			sz;
	SocketAddressInfo		*pai;
};

//------------------------------------------------------------------
//------------------------------------------------------------------

bool parseArguments(Params &_par, int argc, char *argv[]);

void insertListener(const char *_name, IndexT _idx, const char *_addr, int _port, bool _secure);

void insertTalker(const char *_name, IndexT _idx, const char *_addr, int _port);

int main(int argc, char *argv[]){
	
	Params p;
	if(parseArguments(p, argc, argv)) return 0;
	
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
	
	{
		
		foundation::Manager 	m(16);
		
		m.registerScheduler(new SchedulerT(m));
		m.registerScheduler(new AioSchedulerT(m));
		
		
		const IndexT svcidx = m.registerService<SchedulerT>(new foundation::Service, 0);
		
		m.start();
		
		insertListener("echo", svcidx, "0.0.0.0", p.start_port + 111, false);
		insertTalker("echo", svcidx, "0.0.0.0", p.start_port + 112);
		
		cout<<"Here some examples how to test: "<<endl;
		cout<<"For tcp:"<<endl;
		cout<<"\t$ telnet localhost 2111"<<endl;
		cout<<"For udp:"<<endl;
		cout<<"\t$ nc -u localhost 2112"<<endl;
		
		char c;
		cout<<"> "<<flush;
		cin>>c;
		//m.stop(true);
	}
	Thread::waitAll();
	return 0;
}

void insertListener(const char *_name, IndexT _idx, const char *_addr, int _port, bool _secure){
	SocketAddressInfo		ai(_addr, _port, 0, SocketAddressInfo::Inet4, SocketAddressInfo::Stream);
	SocketDevice	sd;
	
	sd.create(ai.begin());
	sd.makeNonBlocking();
	sd.prepareAccept(ai.begin(), 100);
	if(!sd.ok()){
		cout<<"error creating listener for "<<_name<<endl;
		return;
	}
	
	foundation::aio::openssl::Context *pctx(NULL);
	
	if(_secure){
		pctx = foundation::aio::openssl::Context::create();
	}
	if(pctx){
		const char *pcertpath(OSSL_SOURCE_PATH"ssl_/certs/A-server.pem");
		
		pctx->loadCertificateFile(pcertpath);
		pctx->loadPrivateKeyFile(pcertpath);
	}
	
	fdt::ObjectPointer<Listener> lsnptr(new Listener(sd, pctx));
	fdt::m().service(_idx).insert<AioSchedulerT>(lsnptr, 0);
	cout<<"inserted listener on port "<<_port<<" for "<<_name<<endl;
}

void insertTalker(const char *_name, IndexT _idx, const char *_addr, int _port){
	SocketAddressInfo		ai(_addr, _port, 0, SocketAddressInfo::Inet4, SocketAddressInfo::Datagram);
	SocketDevice	sd;
	
	sd.create(ai.begin());
	sd.bind(ai.begin());
	
	if(!sd.ok()){
		cout<<"error creating talker for "<<_name<<endl;
		return;
	}
	
	foundation::ObjectPointer<Talker> tkrptr(new Talker(sd));
	fdt::m().service(_idx).insert<AioSchedulerT>(tkrptr, 0);
	cout<<"inserted talker on port "<<_port<<" for "<<_name<<endl;
}

//------------------------------------------------------------------
//------------------------------------------------------------------

bool parseArguments(Params &_par, int argc, char *argv[]){
	using namespace boost::program_options;
	try{
		options_description desc("SolidFrame concept application");
		desc.add_options()
			("help,h", "List program options")
			("base_port,b", value<int>(&_par.start_port)->default_value(2000),
					"Base port")
			("debug_levels,l", value<string>(&_par.dbg_levels)->default_value("iew"),"Debug logging levels")
			("debug_modules,m", value<string>(&_par.dbg_modules),"Debug logging modules")
			("debug_address,a", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")
			("debug_port,p", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")
			("debug_console,c", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")
			("debug_unbuffered,s", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered")
			("use_log,L", value<bool>(&_par.log)->implicit_value(true)->default_value(false), "Debug buffered")
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
	const SocketDevice &_rsd,
	foundation::aio::openssl::Context *_pctx
):BaseT(_rsd), pctx(_pctx){
	state(0);
}

Listener::~Listener(){
}

int Listener::execute(ulong, TimeSpec&){
	idbg("here");
	cassert(this->socketOk());
	if(signaled()){
		{
		Locker<Mutex>	lock(this->mutex());
		ulong sm = this->grabSignalMask();
		if(sm & foundation::S_KILL) return BAD;
		}
	}
	uint cnt(10);
	while(cnt--){
		if(state() == 0){
			switch(this->socketAccept(sd)){
				case BAD: return BAD;
				case OK:break;
				case NOK:
					state(1);
					return NOK;
			}
		}
		state(0);
		cassert(sd.ok());
		//TODO: one may do some filtering on sd based on sd.remoteAddress()
		if(pctx.get()){
			fdt::ObjectPointer<Connection> conptr(new Connection(sd));
			fdt::m().service(*this).insert<AioSchedulerT>(conptr, 0);
		}else{
			fdt::ObjectPointer<Connection> conptr(new Connection(sd));
			fdt::m().service(*this).insert<AioSchedulerT>(conptr, 0);
		}
	}
	return OK;
}

//------------------------------------------------------------------
//------------------------------------------------------------------
static const char	*hellostr = "Welcome to echo service!!!\r\n"; 

Connection::Connection(const char *_node, const char *_srv): 
	BaseT(),bend(bbeg + BUFSZ),brpos(bbeg),bwpos(bbeg),
	pai(NULL),b(false)
{
	cassert(_node && _srv);
	pai = new SocketAddressInfo(_node, _srv);
	it = pai->begin();
	state(CONNECT);
	
}
Connection::Connection(const SocketDevice &_rsd):
	BaseT(_rsd),bend(bbeg + BUFSZ),brpos(bbeg),bwpos(bbeg),
	pai(NULL),b(false)
{
	state(INIT);
}
Connection::~Connection(){
	state(-1);
	delete pai;
	idbg("");
}

int Connection::execute(ulong _sig, TimeSpec &_tout){
	idbg("time.sec "<<_tout.seconds()<<" time.nsec = "<<_tout.nanoSeconds());
	if(_sig & (fdt::TIMEOUT | fdt::ERRDONE | fdt::TIMEOUT_RECV | fdt::TIMEOUT_SEND)){
		idbg("connecton timeout or error");
		if(state() == CONNECT_TOUT){
			if(++it){
				state(CONNECT);
				return fdt::UNREGISTER;
			}
		}
		return BAD;
	}

	if(signaled()){
		{
		Locker<Mutex>	lock(mutex());
		ulong sm = grabSignalMask();
		if(sm & fdt::S_KILL) return BAD;
		}
	}
	const uint32 sevs(socketEventsGrab());
	if(sevs){
		if(sevs == fdt::ERRDONE){
			
			return BAD;
		}
		if(state() == READ_TOUT){	
			cassert(sevs & fdt::INDONE);
		}else if(state() == WRITE_TOUT){	
			cassert(sevs & fdt::OUTDONE);
		}
		
	}
	int rc = 512 * 1024;
	do{
		switch(state()){
			case READ:
				switch(socketRecv(bbeg, BUFSZ)){
					case BAD: return BAD;
					case OK: break;
					case NOK: 
						state(READ_TOUT); b=true; 
						socketTimeoutRecv(30);
						return NOK;
				}
			case READ_TOUT:
				state(WRITE);
			case WRITE:
				switch(socketSend(bbeg, socketRecvSize())){
					case BAD: return BAD;
					case OK: break;
					case NOK: 
						state(WRITE_TOUT);
						socketTimeoutSend(10);
						return NOK;
				}
			case WRITE_TOUT:
				state(READ);
				break;
			case CONNECT:
				switch(socketConnect(it)){
					case BAD:
						if(++it){
							state(CONNECT);
							return OK;
						}
						return BAD;
					case OK:  state(INIT);break;
					case NOK: state(CONNECT_TOUT); return fdt::REGISTER;
				};
				break;
			case CONNECT_TOUT:
				delete pai; pai = NULL;
			case INIT:
				socketSend(hellostr, strlen(hellostr));
				state(READ);
				break;
		}
		rc -= socketRecvSize();
	}while(rc > 0);
	return OK;
}

//------------------------------------------------------------------
//------------------------------------------------------------------

Talker::Talker(const char *_node, const char *_srv):pai(NULL){
	if(_node){
		pai = new SocketAddressInfo(_node, _srv);
		strcpy(bbeg, hellostr);
		sz = strlen(hellostr);
		state(INIT);
	}
}

Talker::Talker(const SocketDevice &_rsd):BaseT(_rsd), pai(NULL){
	state(READ);
}

Talker::~Talker(){
	delete pai;
}
int Talker::execute(ulong _sig, TimeSpec &_tout){
	if(_sig & (fdt::TIMEOUT | fdt::ERRDONE | fdt::TIMEOUT_SEND | fdt::TIMEOUT_RECV)){
		return BAD;
	}
	if(signaled()){
		{
		Locker<Mutex>	lock(mutex());
		ulong sm = grabSignalMask(0);
		if(sm & fdt::S_KILL) return BAD;
		}
	}
	const uint32 sevs(socketEventsGrab());
	if(sevs & fdt::ERRDONE) return BAD;
	int rc = 512 * 1024;
	do{
		switch(state()){
			case READ:
				switch(socketRecvFrom(bbeg, BUFSZ)){
					case BAD: return BAD;
					case OK: state(READ_TOUT);break;
					case NOK:
						socketTimeoutRecv(5 * 60);
						state(READ_TOUT); 
						return NOK;
				}
			case READ_TOUT:
				state(WRITE);
			case WRITE:
				//sprintf(bbeg + socketRecvSize() - 1," [%u:%d]\r\n", (unsigned)_tout.seconds(), (int)_tout.nanoSeconds());
				switch(socketSendTo(bbeg, socketRecvSize(), socketRecvAddr())){
					case BAD: return BAD;
					case OK: break;
					case NOK: state(WRITE_TOUT);
						socketTimeoutSend(5 * 60);
						return NOK;
				}
			case WRITE_TOUT:
				if(socketHasPendingSend()){
					socketTimeoutSend(_tout, 5 * 60);
					return NOK;
				}else{
					state(READ);
					return OK;
				}
			case INIT:
				if(pai->empty()){
					idbg("Invalid address");
					return BAD;
				}
				SocketAddressInfoIterator it(pai->begin());
				switch(socketSendTo(bbeg, sz, SocketAddressPair(it))){
					case BAD: return BAD;
					case OK: state(READ); break;
					case NOK:
						state(WRITE_TOUT);
						return NOK;
				}
				break;
		}
		rc -= socketRecvSize();
	}while(rc > 0);
	return OK;
}


//------------------------------------------------------------------
//------------------------------------------------------------------

