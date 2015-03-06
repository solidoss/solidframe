#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
#include "frame/service.hpp"

#include "frame/aio/aioreactor.hpp"
#include "frame/aio/aioobject.hpp"
#include "frame/aio/aiolistener.hpp"

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

using namespace std;
using namespace solid;
using namespace std::placeholders;

typedef frame::Scheduler<frame::aio::Reactor>	AioSchedulerT;

enum Events{
	EventStartE = 0,
	EventRunE,
	EventStopE,
	EventSendE,
};

//------------------------------------------------------------------
//------------------------------------------------------------------

struct Params{
	int			listener_port;
	int			talker_port;
	int			connect_port;
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
	bool					running(true);
}

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
//------------------------------------------------------------------
//------------------------------------------------------------------

class Listener: public Dynamic<Listener, frame::aio::Object>{
public:
	Listener(
		frame::Service &_rsvc,
		AioSchedulerT &_rsched,
		SocketDevice &_rsd
	):rsvc(_rsvc), rsch(_rsched), sock(this->proxy(), _rsd){}
private:
	/*virtual*/ void onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent);
	void onAccept(frame::aio::ReactorContext &_rctx, SocketDevice &_rsd);
	
	typedef frame::aio::Listener		ListenerSocketT;
	//typedef frame::aio::Timer			TimerT;
	
	frame::Service		&rsvc;
	AioSchedulerT		&rsch;
	ListenerSocketT		sock;
	//TimerT				timer;
};


#define USE_CONNECTION


#ifdef USE_CONNECTION

#include "frame/aio/aiostream.hpp"
#include "frame/aio/aiosocket.hpp"

class Connection: public Dynamic<Connection, frame::aio::Object>{
protected:
	Connection():sock(this->proxy()), recvcnt(0), sendcnt(0){}
public:
	Connection(SocketDevice &_rsd):sock(this->proxy(), _rsd), recvcnt(0), sendcnt(0){}
	~Connection(){}
protected:
	/*virtual*/ void onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent);
	static void onRecv(frame::aio::ReactorContext &_rctx, size_t _sz);
	static void onSend(frame::aio::ReactorContext &_rctx);
	void onTimer(frame::aio::ReactorContext &_rctx);
protected:
	typedef frame::aio::Stream<frame::aio::Socket>		StreamSocketT;
	//typedef frame::aio::Timer							TimerT;
	enum {BufferCapacity = 1024 * 2};
	
	char			buf[BufferCapacity];
	StreamSocketT	sock;
	uint64 			recvcnt;
	uint64			sendcnt;
	size_t			sendcrt;
	//TimerT			timer;
};

class ClientConnection: public Dynamic<Connection, Connection>{
	ResolveData		rd;
private:
	/*virtual*/ void onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent);
	void onConnect(frame::aio::ReactorContext &_rctx);
public:
	ClientConnection(ResolveData const & _rd): rd(_rd){}
};
#endif


#define USE_TALKER

#include "frame/aio/aiodatagram.hpp"
#include "frame/aio/aiosocket.hpp"

#ifdef USE_TALKER
class Talker: public Dynamic<Talker, frame::aio::Object>{
public:
	Talker(SocketDevice &_rsd):sock(this->proxy(), _rsd){}
	~Talker(){}
private:
	/*virtual*/ void onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent);
	void onRecv(frame::aio::ReactorContext &_rctx, SocketAddress &_raddr, size_t _sz);
	void onSend(frame::aio::ReactorContext &_rctx);
private:
	typedef frame::aio::Datagram<frame::aio::Socket>	DatagramSocketT;
	
	enum {BufferCapacity = 1024 * 2 };
	
	char			buf[BufferCapacity];
	DatagramSocketT	sock;
};
#endif

bool parseArguments(Params &_par, int argc, char *argv[]);

int main(int argc, char *argv[]){
	Params p;
	if(parseArguments(p, argc, argv)) return 0;
	
	signal(SIGINT, term_handler); /* Die on SIGTERM */
	signal(SIGPIPE, SIG_IGN);
	
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
		
		AioSchedulerT		sch;
		
		
		frame::Manager		m;
		frame::Service		svc(m, frame::Event(EventStopE));
		
		if(sch.start(1)){
			running = false;
			cout<<"Error starting scheduler"<<endl;
		}else{
			ResolveData		rd =  synchronous_resolve("0.0.0.0", p.listener_port, 0, SocketInfo::Inet4, SocketInfo::Stream);
	
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
#ifdef USE_CONNECTION
			{
				rd = synchronous_resolve("127.0.0.1", p.connect_port, 0, SocketInfo::Inet4, SocketInfo::Stream);
				
				DynamicPointer<frame::aio::Object>	objptr(new ClientConnection(rd));
				solid::ErrorConditionT				err;
				solid::frame::ObjectUidT			objuid;
				
				objuid = sch.startObject(objptr, svc, frame::Event(EventStartE), err);
				
				idbg("Started Client Connection object: "<<objuid.index<<','<<objuid.unique);
				
			}
#endif
#ifdef USE_TALKER			
			rd = synchronous_resolve("0.0.0.0", p.talker_port, 0, SocketInfo::Inet4, SocketInfo::Datagram);
			
			sd.create(rd.begin());
			sd.bind(rd.begin());
			
			if(sd.ok()){
				DynamicPointer<frame::aio::Object>	objptr(new Talker(sd));
				
				solid::ErrorConditionT				err;
				solid::frame::ObjectUidT			objuid;
				
				objuid = sch.startObject(objptr, svc, frame::Event(EventStartE), err);
				
				idbg("Started Talker object: "<<objuid.index<<','<<objuid.unique);
			}else{
				cout<<"Error creating talker socket"<<endl;
				running = false;
			}
#endif
		}
		
		if(0){
			Locker<Mutex>	lock(mtx);
			while(running){
				cnd.wait(lock);
			}
		}else{
			char c;
			cin>>c;
			exit(0);
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
			("talk-port,t", value<int>(&_par.talker_port)->default_value(3000),"Talker port")
			("connection-port,c", value<int>(&_par.connect_port)->default_value(5000),"Connection port")
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
		//sock.postAccept(_rctx, [this](frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){return onAccept(_rctx, _rsd);});
	}else if(_revent.id == EventStopE){
		postStop(_rctx);
	}
}

void Listener::onAccept(frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){
	idbg("");
	unsigned	repeatcnt = 4;
	
	do{
		if(!_rctx.error()){
			//_rsd.enableNoDelay();
#ifdef USE_CONNECTION
			//cout<<"recvbuffsz = "<<_rsd.recvBufferSize().second<<endl;
			//cout<<"sendbuffsz = "<<_rsd.sendBufferSize().second<<endl;
			_rsd.recvBufferSize(1024 * 64);
			_rsd.sendBufferSize(1024 * 32);
			DynamicPointer<frame::aio::Object>	objptr(new Connection(_rsd));
			solid::ErrorConditionT				err;
			
			rsch.startObject(objptr, rsvc, frame::Event(EventStartE), err);
#else
			cout<<"Accepted connection: "<<_rsd.descriptor()<<endl;
#endif
		}else{
			//e.g. a limit of open file descriptors was reached - we sleep for 10 seconds
			//timer.waitFor(_rctx, TimeSpec(10), std::bind(&Listener::onEvent, this, _1, frame::Event(EventStartE)));
			break;
		}
		--repeatcnt;
	}while(repeatcnt && sock.accept(_rctx, std::bind(&Listener::onAccept, this, _1, _2), _rsd));
	
	if(!repeatcnt){
#if 1
		//the normal way
		//sock.postAccept(_rctx, std::bind(&Listener::onAccept, this, _1, _2));//fully asynchronous call
		sock.postAccept(
			_rctx,
			[this](frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){onAccept(_rctx, _rsd);}
		);//fully asynchronous call
#else
		//using this->post(...) as an example
		this->post(
			_rctx,
			 [this](frame::aio::ReactorContext &_rctx, frame::Event const &_revent){onEvent(_rctx, _revent);},
			frame::Event(EventStartE)
		);
#endif
	}
}

//-----------------------------------------------------------------------------
//		Connection
//-----------------------------------------------------------------------------
#ifdef USE_CONNECTION
/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
	edbg(this<<" "<<_revent.id);
	if(_revent.id == EventStartE){
		sock.postRecvSome(_rctx, buf, BufferCapacity, Connection::onRecv/*std::bind(&Connection::onRecv, this, _1, _2)*/);//fully asynchronous call
		//timer.waitFor(_rctx, TimeSpec(30), std::bind(&Connection::onTimer, this, _1));
	}else if(_revent.id == EventStopE){
		edbg(this<<" postStop");
		postStop(_rctx);
	}
}

/*static*/ void Connection::onRecv(frame::aio::ReactorContext &_rctx, size_t _sz){
	unsigned	repeatcnt = 2;
	Connection	&rthis = static_cast<Connection&>(_rctx.object());
	idbg(&rthis<<" "<<_sz);
	do{
		if(!_rctx.error()){
			idbg(&rthis<<" write: "<<_sz);
			rthis.recvcnt += _sz;
			rthis.sendcrt = _sz;
			if(rthis.sock.sendAll(_rctx, rthis.buf, _sz, Connection::onSend/*std::bind(&Connection::onSend, this, _1)*/)){
				if(_rctx.error()){
					edbg(&rthis<<" postStop "<<rthis.recvcnt<<" "<<rthis.sendcnt);
					rthis.postStop(_rctx);
					break;
				}
				rthis.sendcnt += rthis.sendcrt;
			}else{
				idbg(&rthis<<"");
				break;
			}
		}else{
			edbg(&rthis<<" postStop "<<rthis.recvcnt<<" "<<rthis.sendcnt);
			rthis.postStop(_rctx);
			break;
		}
		--repeatcnt;
	}while(repeatcnt && rthis.sock.recvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv/*std::bind(&Connection::onRecv, this, _1, _2)*/, _sz));
	
	idbg(&rthis<<" "<<repeatcnt);
	//timer.waitFor(_rctx, TimeSpec(30), std::bind(&Connection::onTimer, this, _1));
	if(repeatcnt == 0){
		bool rv = rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv/*std::bind(&Connection::onRecv, this, _1, _2)*/);//fully asynchronous call
		cassert(!rv);
	}
}

/*static*/ void Connection::onSend(frame::aio::ReactorContext &_rctx){
	Connection &rthis = static_cast<Connection&>(_rctx.object());
	if(!_rctx.error()){
		idbg(&rthis<<" postRecvSome");
		rthis.sendcnt += rthis.sendcrt;
		rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv/*std::bind(&Connection::onRecv, this, _1, _2)*/);//fully asynchronous call
		//timer.waitFor(_rctx, TimeSpec(30), std::bind(&Connection::onTimer, this, _1));
	}else{
		edbg(&rthis<<" postStop "<<rthis.recvcnt<<" "<<rthis.sendcnt);
		rthis.postStop(_rctx);
	}
}

void Connection::onTimer(frame::aio::ReactorContext &_rctx){
	
}

//-----------------------------------------------------------------------------

/*virtual*/ void ClientConnection::onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
	edbg(this<<" "<<_revent.id);
	if(_revent.id == EventStartE){
		
		string				hoststr;
		string				servstr;
		
		synchronous_resolve(
			hoststr,
			servstr,
			rd.begin(),
			ReverseResolveInfo::Numeric
		);
		
		idbg("Connect to "<<hoststr<<":"<<servstr);
		
		if(sock.connect(_rctx, rd.begin(), std::bind(&ClientConnection::onConnect, this, _1))){
			onConnect(_rctx);
		}
		//timer.waitFor(_rctx, TimeSpec(30), std::bind(&Connection::onTimer, this, _1));
	}else if(_revent.id == EventStopE){
		edbg(this<<" postStop");
		postStop(_rctx);
	}
}
void ClientConnection::onConnect(frame::aio::ReactorContext &_rctx){
	if(!_rctx.error()){
		idbg(this<<" SUCCESS");
		sock.postRecvSome(_rctx, buf, BufferCapacity, Connection::onRecv/*std::bind(&Connection::onRecv, this, _1, _2)*/);//fully asynchronous call
	}else{
		edbg(this<<" postStop "<<recvcnt<<" "<<sendcnt<<" "<<_rctx.systemError().message());
		postStop(_rctx);
	}
}

#endif

//-----------------------------------------------------------------------------
//		Talker
//-----------------------------------------------------------------------------
#ifdef USE_TALKER
/*virtual*/ void Talker::onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
	idbg(this<<" "<<_revent.id);
	
	if(_revent.id == EventStartE){
		sock.postRecvFrom(_rctx, buf, BufferCapacity, std::bind(&Talker::onRecv, this, _1, _2, _3));//fully asynchronous call
	}else if(_revent.id == EventStopE){
		edbg(this<<" postStop");
		postStop(_rctx);
	}
}


void Talker::onRecv(frame::aio::ReactorContext &_rctx, SocketAddress &_raddr, size_t _sz){
	unsigned	repeatcnt = 10;
	do{
		if(!_rctx.error()){
			if(sock.sendTo(_rctx, buf, _sz, _raddr, std::bind(&Talker::onSend, this, _1))){
				if(_rctx.error()){
					edbg(this<<" postStop");
					postStop(_rctx);
					break;
				}
			}else{
				break;
			}
		}else{
			edbg(this<<" postStop");
			postStop(_rctx);
			break;
		}
		--repeatcnt;
	}while(repeatcnt && sock.recvFrom(_rctx, buf, BufferCapacity, std::bind(&Talker::onRecv, this, _1, _2, _3), _raddr, _sz));
	
	idbg(repeatcnt);
	if(repeatcnt == 0){
		bool rv = sock.postRecvFrom(_rctx, buf, BufferCapacity, std::bind(&Talker::onRecv, this, _1, _2, _3));//fully asynchronous call
		cassert(!rv);
	}
}

void Talker::onSend(frame::aio::ReactorContext &_rctx){
	if(!_rctx.error()){
		sock.postRecvFrom(_rctx, buf, BufferCapacity, std::bind(&Talker::onRecv, this, _1, _2, _3));//fully asynchronous call
	}else{
		postStop(_rctx);
	}
}

#endif