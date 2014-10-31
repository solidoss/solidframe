#include "frame/manager.hpp"
#include "frame/scheduler.hpp"

#include "frame/aio2/aioreactor.hpp"
#include "frame/aio2/aioobject.hpp"
#include "frame/aio2/aiolistener.hpp"

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
	bool					running(false);
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
		frame::Manager &_rm,
		AioSchedulerT &_rsched,
		const SocketDevice &_rsd
	):rm(_rm), rsched(_rsched), sock(this->proxy()){}
private:
	/*virtual*/ bool onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent);
	bool onAccept(frame::aio::ReactorContext &_rctx, SocketDevice &_rsd);
	
	typedef frame::aio::Listener		ListenerSocketT;
	//typedef frame::aio::Timer			TimerT;
	
	frame::Manager		&rm;
	AioSchedulerT		&rsched;
	ListenerSocketT		sock;
	//TimerT				timer;
};

#ifdef USE_CONNECTION
class Connection: public Dynamic<Connection, frame::aio::Object>{
public:
	Connection(const SocketDevice &_rsd);
	~Connection();
private:
	/*virtual*/ bool onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent);
	bool onRecv(frame::aio::ReactorContext &_rctx, size_t _sz);
	bool onSend(frame::aio::ReactorContext &_rctx);
	bool onTimer(frame::aio::ReactorContext &_rctx);
private:
	typedef frame::aio::Stream<frame::aio::Socket>	StreamSocketT;
	typedef frame::aio::Timer						TimerT;
	enum {BufferCapacity = 1024};
	
	char			buf[BufferCapacity];
	StreamSocketT	sock;
	TimerT			timer;
};
#endif

#ifdef USE_TALKER
class Talker: public Dynamic<Talker, frame::aio::Object>{
public:
	Talker(const SocketDevice &_rsd);
	~Talker();
private:
	/*virtual*/ bool onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent);
	bool onRecv(frame::aio::ReactorContext &_rctx, SocketAddressInet &_raddr, size_t _sz);
	bool onSend(frame::aio::ReactorContext &_rctx);
private:
	typedef frame::aio::Datagram<frame::aio::Socket>	DatagramSocketT;
	typedef frame::aio::Timer							TimerT;
	enum {BufferCapacity = 1024};
	
	char			buf[BufferCapacity];
	DatagramSocketT	sock;
};
#endif

bool parseArguments(Params &_par, int argc, char *argv[]);

int main(int argc, char *argv[]){
	Params p;
	if(parseArguments(p, argc, argv)) return 0;
	
	signal(SIGINT, term_handler); /* Die on SIGTERM */
	
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
		frame::Manager		m;
		AioSchedulerT		s(m);
		
		if(!s.start(0)){
			running = false;
			cout<<"Error starting scheduler"<<endl;
		}else{
			ResolveData		rd =  synchronous_resolve("0.0.0.0", p.listener_port, 0, SocketInfo::Inet4, SocketInfo::Stream);
	
			SocketDevice	sd;
			
			sd.create(rd.begin());
			sd.prepareAccept(rd.begin(), 100);
			
			if(sd.ok()){
				DynamicPointer<frame::aio::Object>	objptr(new Listener(m, s, sd));
				
				m.registerObject(objptr, s, frame::Event(EventStartE));

			}else{
				cout<<"Error creating listener socket"<<endl;
				running = false;
			}
#ifdef USE_TALKER			
			rd = synchronous_resolve("0.0.0.0", p.talker_port, 0, SocketInfo::Inet4, SocketInfo::Datagram);
			
			sd.create(rd.begin());
			sd.bind(rd.begin());
			
			if(sd.ok()){
				DynamicPointer<frame::aio::Object>	objptr(new Talker(sd));
				
				m.registerObject(objptr, s, frame::Event(EventStartE));
			}else{
				cout<<"Error creating talker socket"<<endl;
				running = false;
			}
#endif
		}
		
		{
			Locker<Mutex>	lock(mtx);
			while(running){
				cnd.wait(lock);
			}
		}
		m.stop(frame::Event(EventStopE));
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
			("debug-levels,L", value<string>(&_par.dbg_levels)->default_value("iew"),"Debug logging levels")
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

/*virtual*/ bool Listener::onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
	if(_revent.id == EventStartE){
		//sock.postAccept(_rctx, std::bind(&Listener::onAccept, this, _1, _2));
		sock.postAccept(_rctx, [this](frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){return onAccept(_rctx, _rsd);});
	}else if(_revent.id == EventStopE){
		return false;
	}
	return true;
}

bool Listener::onAccept(frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){
	unsigned	repeatcnt = 10;
	
	do{
		if(!_rctx.error()){
#ifdef USE_CONNECTION			
			DynamicPointer<frame::aio::Object>	objptr(new Connection(_rsd));
			rm.registerObject(objptr, rsched, frame::Event(EventStartE));
#else
			cout<<"Accepted connection"<<endl;
#endif
		}else{
			//e.g. a limit of open file descriptors was reached - we sleep for 10 seconds
			//timer.waitFor(_rctx, TimeSpec(10), std::bind(&Listener::onEvent, this, _1, frame::Event(EventStartE)));
			break;
		}
		--repeatcnt;
	}while(repeatcnt && sock.accept(_rctx, std::bind(&Listener::onAccept, this, _1, _2), _rsd));
	
	if(!repeatcnt){
		sock.postAccept(_rctx, std::bind(&Listener::onAccept, this, _1, _2));//fully asynchronous call
	}
	return true;
}

//-----------------------------------------------------------------------------
//		Connection
//-----------------------------------------------------------------------------
#ifdef USE_CONNECTION
/*virtual*/ bool Connection::onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
	if(_rctx.event().id == EventStartE){
		sock.scheduleRecvSome(_rctx, buf, BufferCapacity, std::bind(&Connection::onRecv, this, _1, _2));//fully asynchronous call
		timer.waitFor(_rctx, TimeSpec(30), std::bind(&Connection::onTimer, this, _1));
	}else if(_rctx.event().id == EventStopE){
		return false;
	}
	return true;
}

bool Connection::onRecv(frame::aio::ReactorContext &_rctx, size_t _sz){
	unsigned	repeatcnt = 10;
	do{
		if(!_rctx.error()){
			if(sock.sendAll(_rctx, buf, _sz, std::bind(&Connection::onSend, this, _1))){
				if(_rctx.error()){
					return false;
				}
			}else{
				break;
			}
		}else{
			return false;
		}
		--repeatcnt;
	}while(repeatcnt && sock.recvSome(_rctx, buf, BufferCapacity, std::bind(&Connection::onRecv, this, _1, _2), _sz));
	
	timer.waitFor(_rctx, TimeSpec(30), std::bind(&Connection::onTimer, this, _1));
	return true;
}

bool Connection::onSend(frame::aio::ReactorContext &_rctx){
	if(!_rctx.error()){
		sock.scheduleRecvSome(_rctx, buf, BufferCapacity, std::bind(&Connection::onRecv, this, _1, _2));//fully asynchronous call
		timer.waitFor(_rctx, TimeSpec(30), std::bind(&Connection::onTimer, this, _1));
	}else{
		return false;
	}
	return true;
}

bool Connection::onTimer(frame::aio::ReactorContext &_rctx){
	return false;
}
#endif

//-----------------------------------------------------------------------------
//		Talker
//-----------------------------------------------------------------------------
#ifdef USE_TALKER
/*virtual*/ bool Talker::onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
	if(_rctx.event().id == EventStartE){
		sock.scheduleRecvFrom(_rctx, buf, BufferCapacity, std::bind(&Talker::onRecv, this, _1, _2, _3));//fully asynchronous call
	}else if(_rctx.event().id == EventStopE){
		return false;
	}
	return true;
}


bool Talker::onRecv(frame::aio::ReactorContext &_rctx, SocketAddressInet &_raddr, size_t _sz){
	unsigned	repeatcnt = 10;
	do{
		if(!_rctx.error()){
			if(sock.sendTo(_rctx, buf, _sz, _raddr, std::bind(&Talker::onRecv, this, _1, _2))){
				if(_rctx.error()){
					return false;
				}
			}else{
				break;
			}
		}else{
			return false;
		}
		--repeatcnt;
	}while(repeatcnt && sock.recvFrom(_rctx, buf, BufferCapacity, std::bind(&Talker::onRecv, this, _1, _2), _raddr, _sz));
	
	return true;
}

bool Talker::onSend(frame::aio::ReactorContext &_rctx){
	if(!_rctx.error()){
		sock.scheduleRecvFrom(_rctx, buf, BufferCapacity, std::bind(&Talker::onRecv, this, _1, _2, _3));//fully asynchronous call
	}else{
		return false;
	}
	return true;
}

#endif