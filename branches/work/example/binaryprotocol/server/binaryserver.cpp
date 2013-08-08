#include "protocol/binary/client/clientsession.hpp"

#include "frame/aio/aiosingleobject.hpp"
#include "frame/aio/aioselector.hpp"
#include "frame/aio/openssl/opensslsocket.hpp"

#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
#include "frame/service.hpp"
#include "system/thread.hpp"
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"
#include "example/binaryprotocol/core/messages.hpp"

#include <signal.h>

#include "boost/program_options.hpp"

#include <iostream>

using namespace solid;
using namespace std;

typedef frame::Scheduler<frame::aio::Selector>	AioSchedulerT;

namespace{
	struct Params{
		int			port;
		string		addr_str;
		string		dbg_levels;
		string		dbg_modules;
		string		dbg_addr;
		string		dbg_port;
		bool		dbg_console;
		bool		dbg_buffered;
		bool		log;
	};

	Mutex					mtx;
	Condition				cnd;
	bool					run(true);
	Params					p;

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
	
}

class Listener;
class Connection;

class Service: public solid::Dynamic<Service, frame::Service>{
public:
	Service(frame::Manager &_rm, AioSchedulerT &_rsched):BaseT(_rm), rsched(_rsched){}
	~Service(){}
private:
	friend class Listener;
	friend class Connection;
	void insertConnection(
		const solid::SocketDevice &_rsd,
		solid::frame::aio::openssl::Context *_pctx = NULL,
		bool _secure = false
	);
private:
	AioSchedulerT	&rsched;
};

class Listener: public Dynamic<Listener, frame::aio::SingleObject>{
public:
	Listener(
		Service &_rs,
		const SocketDevice &_rsd,
		frame::aio::openssl::Context *_pctx = NULL
	);
	~Listener();
	/*virtual*/ int execute(ulong, TimeSpec&);
private:
	typedef std::auto_ptr<frame::aio::openssl::Context> SslContextPtrT;
	int					state;
	SocketDevice		sd;
	SslContextPtrT		pctx;
	Service				&rsvc;
};

typedef DynamicPointer<Listener>	ListenerPointerT;

bool parseArguments(Params &_par, int argc, char *argv[]);

int main(int argc, char *argv[]){
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
			1024 * 1024 * 1,
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
		frame::Manager			m;
		AioSchedulerT			aiosched(m);
		Service					svc(m, aiosched);
		
		m.registerService(svc);
		{
			
			ResolveData		rd =  synchronous_resolve(p.addr_str.c_str(), p.port, 0, SocketInfo::Inet4, SocketInfo::Stream);
	
			SocketDevice	sd;
	
			sd.create(rd.begin());
			sd.makeNonBlocking();
			sd.prepareAccept(rd.begin(), 100);
			if(!sd.ok()){
				cout<<"Error creating listener"<<endl;
				return 0;
			}
	
			frame::aio::openssl::Context *pctx = NULL;
#if 0
			if(p.secure){
				pctx = frame::aio::openssl::Context::create();
			}
			if(pctx){
				const char *pcertpath(OSSL_SOURCE_PATH"ssl_/certs/A-server.pem");
				
				pctx->loadCertificateFile(pcertpath);
				pctx->loadPrivateKeyFile(pcertpath);
			}
#endif
			ListenerPointerT lsnptr(new Listener(svc, sd, pctx));
	
			m.registerObject(*lsnptr);
			aiosched.schedule(lsnptr);
		}
		
		idbg("");
		if(1){
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
//--------------------------------------------------------------------------
bool parseArguments(Params &_par, int argc, char *argv[]){
	using namespace boost::program_options;
	try{
		options_description desc("SolidFrame concept application");
		desc.add_options()
			("help,h", "List program options")
			("port,p", value<int>(&_par.port)->default_value(2000),"Listen port")
			("address,a", value<string>(&_par.addr_str)->default_value("0.0.0.0"),"Listen address")
			("debug_levels,L", value<string>(&_par.dbg_levels)->default_value("view"),"Debug logging levels")
			("debug_modules,M", value<string>(&_par.dbg_modules)->default_value("any"),"Debug logging modules")
			("debug_address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")
			("debug_port,P", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")
			("debug_console,C", value<bool>(&_par.dbg_console)->implicit_value(false)->default_value(true), "Debug console")
			("debug_unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered")
			("use_log,l", value<bool>(&_par.log)->implicit_value(true)->default_value(false), "Debug buffered")
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
//--------------------------------------------------------------------------
Listener::Listener(
	Service &_rsvc,
	const SocketDevice &_rsd,
	frame::aio::openssl::Context *_pctx
):BaseT(_rsd), pctx(_pctx), rsvc(_rsvc){
	state = 0;
}
Listener::~Listener(){
	idbg("");
}
int Listener::execute(ulong, TimeSpec&){
	cassert(this->socketOk());
	if(notified()){
		ulong sm = this->grabSignalMask();
		if(sm & frame::S_KILL) return BAD;
	}
	uint cnt(10);
	while(cnt--){
		if(state == 0){
			switch(this->socketAccept(sd)){
				case BAD: return BAD;
				case OK:break;
				case NOK:
					state = 1;
					return NOK;
			}
		}
		state = 0;
		cassert(sd.ok());
		rsvc.insertConnection(sd);
	}
	return OK;
}

//--------------------------------------------------------------------------
class Connection: public solid::Dynamic<Connection, solid::frame::aio::SingleObject>{
public:
	Connection(const SocketDevice &_rsd):BaseT(_rsd){
		idbg((void*)this);
	}
	~Connection(){
		idbg((void*)this);
	}
private:
	/*virtual*/ int execute(ulong _sig, TimeSpec &_tout);
private:
};

void Service::insertConnection(
	const SocketDevice &_rsd,
	frame::aio::openssl::Context *_pctx,
	bool _secure
){
	DynamicPointer<frame::aio::Object>	conptr(new Connection(_rsd));
	frame::ObjectUidT rv = this->registerObject(*conptr);
	rsched.schedule(conptr);
}
//--------------------------------------------------------------------------
/*virtual*/ int Connection::execute(ulong _evs, TimeSpec &_tout){
	idbg((void*)this);
	if(notified()){
		ulong sm = this->grabSignalMask();
		if(sm & frame::S_KILL) return BAD;
	}
	if(_evs & frame::ERRDONE){
		idbg("ioerror "<<_evs<<' '<<socketEventsGrab());
		return BAD;
	}
	if(_evs & frame::INDONE){
		idbg("indone");
		
	}
	return NOK;
}