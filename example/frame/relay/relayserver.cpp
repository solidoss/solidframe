#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
#include "frame/service.hpp"

#include "frame/aio/aioreactor.hpp"
#include "frame/aio/aioobject.hpp"
#include "frame/aio/aiolistener.hpp"
#include "frame/aio/aiotimer.hpp"
#include "frame/aio/aiostream.hpp"
#include "frame/aio/aiosocket.hpp"
#include "frame/aio/aioresolver.hpp"

#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"
#include "system/debug.hpp"


#include "boost/program_options.hpp"

#include <signal.h>
#include <iostream>
#include <functional>
#include <unordered_map>

using namespace std;
using namespace solid;
using namespace std::placeholders;

typedef frame::Scheduler<frame::aio::Reactor>	AioSchedulerT;
typedef ATOMIC_NS::atomic<uint32>				AtomicUint32T;
typedef std::unordered_map<
		uint32,
		solid::frame::ObjectUidT
	>											UniqueMapT;
enum Events{
	EventStartE = 0,
	EventRunE,
	EventStopE,
	EventSendE,
};


//------------------------------------------------------------------
//------------------------------------------------------------------

namespace{

struct Params{
	int			listener_port;
	int			talker_port;
	string		destination_addr_str;
	string		destination_port_str;
	
	string		dbg_levels;
	string		dbg_modules;
	string		dbg_addr;
	string		dbg_port;
	bool		dbg_console;
	bool		dbg_buffered;
	bool		log;
};

Params params;

Mutex					mtx;
Condition				cnd;
bool					running(true);

static void term_handler(int signum){
    switch(signum) {
		case SIGINT:
		case SIGTERM:{
			if(running){
				Locker<Mutex>  lock(mtx);
				running = false;
				cnd.broadcast();
			}
		}
    }
}

AtomicUint32T		crt_id(0);

UniqueMapT			umap;

struct ResolvMessage: frame::Message{
	ResolveData		resolv_data;
};

frame::aio::Resolver& async_resolver(){
	static frame::aio::Resolver r;
	return r;
}

}//namespace

//------------------------------------------------------------------
//------------------------------------------------------------------

class Listener: public Dynamic<Listener, frame::aio::Object>{
public:
	Listener(
		frame::Service &_rsvc,
		AioSchedulerT &_rsched,
		SocketDevice &_rsd
	):rsvc(_rsvc), rsch(_rsched), sock(this->proxy(), _rsd){}
	~Listener(){
	}
private:
	/*virtual*/ void onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent);
	void onAccept(frame::aio::ReactorContext &_rctx, SocketDevice &_rsd);
	
	void onTimer(frame::aio::ReactorContext &_rctx);
	
	typedef frame::aio::Listener		ListenerSocketT;
	typedef frame::aio::Timer			TimerT;
	
	frame::Service		&rsvc;
	AioSchedulerT		&rsch;
	ListenerSocketT		sock;
};

class Connection: public Dynamic<Connection, frame::aio::Object>{
public:
	Connection(SocketDevice &_rsd):sock1(this->proxy(), _rsd), sock2(this->proxy(), _rsd), recvcnt(0), sendcnt(0){}
	~Connection(){}
protected:
	/*virtual*/ void onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent);
	static void onRecv(frame::aio::ReactorContext &_rctx, size_t _sz);
	static void onSend(frame::aio::ReactorContext &_rctx);
protected:
	typedef frame::aio::Stream<frame::aio::Socket>		StreamSocketT;
	//typedef frame::aio::Timer							TimerT;
	enum {BufferCapacity = 1024 * 2};
	
	char					buf1[BufferCapacity];
	char					buf2[BufferCapacity];
	StreamSocketT			sock1;
	StreamSocketT			sock2;
	uint64 					recvcnt;
	uint64					sendcnt;
	size_t					sendcrt;
	frame::MessagePointerT	resolv_msgptr;
	//TimerT			timer;
};

bool parseArguments(Params &_par, int argc, char *argv[]);

int main(int argc, char *argv[]){
	
	if(parseArguments(params, argc, argv)) return 0;
	
	signal(SIGINT, term_handler); /* Die on SIGTERM */
	//signal(SIGPIPE, SIG_IGN);
	
	/*solid::*/Thread::init();
	
#ifdef UDEBUG
	{
	string dbgout;
	Debug::the().levelMask(params.dbg_levels.c_str());
	Debug::the().moduleMask(params.dbg_modules.c_str());
	if(params.dbg_addr.size() && params.dbg_port.size()){
		Debug::the().initSocket(
			params.dbg_addr.c_str(),
			params.dbg_port.c_str(),
			params.dbg_buffered,
			&dbgout
		);
	}else if(params.dbg_console){
		Debug::the().initStdErr(
			params.dbg_buffered,
			&dbgout
		);
	}else{
		Debug::the().initFile(
			*argv[0] == '.' ? argv[0] + 2 : argv[0],
			params.dbg_buffered,
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
		
		AioSchedulerT		sch;
		
		
		frame::Manager		m;
		frame::Service		svc(m, frame::Event(EventStopE));
		
		if(sch.start(1)){
			running = false;
			cout<<"Error starting scheduler"<<endl;
		}else{
			ResolveData		rd =  synchronous_resolve("0.0.0.0", params.listener_port, 0, SocketInfo::Inet4, SocketInfo::Stream);
	
			SocketDevice	sd;
			
			sd.create(rd.begin());
			sd.prepareAccept(rd.begin(), 2000);
			
			if(sd.ok()){
				DynamicPointer<frame::aio::Object>	objptr(new Listener(svc, sch, sd));
				solid::ErrorConditionT				err;
				solid::frame::ObjectUidT			objuid;
				
				objuid = sch.startObject(objptr, svc, frame::Event(EventStartE), err);
				idbg("Started Listener object: "<<objuid.index<<','<<objuid.unique);
			}else{
				cout<<"Error creating listener socket"<<endl;
				running = false;
			}
			frame::Event ev;
		}
		
		if(0){
			Locker<Mutex>	lock(mtx);
			while(running){
				cnd.wait(lock);
			}
		}else{
			char c;
			cin>>c;
			//exit(0);
		}
		
		
		
		m.stop();
	}
	Thread::waitAll();
	return 0;
}

//-----------------------------------------------------------------------------
bool parseArguments(Params &_par, int argc, char *argv[]){
	using namespace boost::program_options;
	try{
		options_description desc("SolidFrame concept application");
		desc.add_options()
			("help,h", "List program options")
			("listen-port,l", value<int>(&_par.listener_port)->default_value(2000),"Listener port")
			("destination-addr,d", value<string>(&_par.destination_addr_str)->default_value(""), "Destination address")
			("debug-levels,L", value<string>(&_par.dbg_levels)->default_value("view"),"Debug logging levels")
			("debug-modules,M", value<string>(&_par.dbg_modules),"Debug logging modules")
			("debug-address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")
			("debug-port,P", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")
			("debug-console,C", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")
			("debug-unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered")
		;
		variables_map vm;
		store(parse_command_line(argc, argv, desc), vm);
		notify(vm);
		if (vm.count("help")) {
			cout << desc << "\n";
			return true;
		}
		
		size_t			pos;
		
		pos = _par.destination_addr_str.rfind(':');
		if(pos != string::npos){
			_par.destination_addr_str[pos] = '\0';
			
			_par.destination_port_str.assign(_par.destination_addr_str.c_str() + pos + 1);
			
			_par.destination_addr_str.resize(pos);
		}else{
			_par.destination_port_str = _par.listener_port;
		}
		
		return false;
	}catch(exception& e){
		cout << e.what() << "\n";
		return true;
	}
}
//-----------------------------------------------------------------------------
//		Listener
//-----------------------------------------------------------------------------

/*virtual*/ void Listener::onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
	idbg("event = "<<_revent.id);
	if(_revent.id == EventStartE){
		sock.postAccept(_rctx, std::bind(&Listener::onAccept, this, _1, _2));
	}else if(_revent.id == EventStopE){
		postStop(_rctx);
	}
}

void Listener::onAccept(frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){
	idbg("");
	unsigned	repeatcnt = 4;
	
	do{
		if(!_rctx.error()){
			_rsd.recvBufferSize(1024 * 64);
			_rsd.sendBufferSize(1024 * 32);
			DynamicPointer<frame::aio::Object>	objptr(new Connection(_rsd));
			solid::ErrorConditionT				err;
			
			rsch.startObject(objptr, rsvc, frame::Event(EventStartE), err);
		}else{
			//e.g. a limit of open file descriptors was reached - we sleep for 10 seconds
			//timer.waitFor(_rctx, TimeSpec(10), std::bind(&Listener::onEvent, this, _1, frame::Event(EventStartE)));
			break;
		}
		--repeatcnt;
	}while(repeatcnt && sock.accept(_rctx, std::bind(&Listener::onAccept, this, _1, _2), _rsd));
	
	if(!repeatcnt){
		sock.postAccept(
			_rctx,
			[this](frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){onAccept(_rctx, _rsd);}
		);//fully asynchronous call
	}
}

//-----------------------------------------------------------------------------
//		Connection
//-----------------------------------------------------------------------------
struct ResolvFunc{
	void operator()(ResolveData &_rrd, ERROR_NS::error_code const &_rerr){
		
	}
};
/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
	edbg(this<<" "<<_revent.id);
	if(_revent.id == EventStartE){
		//sock.postRecvSome(_rctx, buf, BufferCapacity, Connection::onRecv);//fully asynchronous call
		if(params.destination_addr_str.size()){
			//we must resolve the address then connect
			async_resolver().requestResolve(
				ResolvFunc(), params.destination_addr_str.c_str(),
				params.destination_port_str.c_str(), 0, SocketInfo::Inet4, SocketInfo::Stream
			);
		}else{
			uint32 id = crt_id++;
			snprintf(buf1, BufferCapacity, "%ul", id);
			
		}
	}else if(_revent.id == EventStopE){
		edbg(this<<" postStop");
		//sock.shutdown(_rctx);
		postStop(_rctx);
	}
}

/*static*/ void Connection::onRecv(frame::aio::ReactorContext &_rctx, size_t _sz){
	unsigned	repeatcnt = 2;
	Connection	&rthis = static_cast<Connection&>(_rctx.object());
	idbg(&rthis<<" "<<_sz);
	
}

/*static*/ void Connection::onSend(frame::aio::ReactorContext &_rctx){
	Connection &rthis = static_cast<Connection&>(_rctx.object());
	
}


//-----------------------------------------------------------------------------

