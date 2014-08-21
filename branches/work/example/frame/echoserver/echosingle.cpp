#include "frame/manager.hpp"
#include "frame/scheduler.hpp"

#include "frame/aio2/aioreactor.hpp"
#include "frame/aio/aioobject.hpp"

#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"

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
	Listener(frame::Manager &_rm, AioSchedulerT &_rsched, const SocketDevice &_rsd);
	~Listener();
private:
	/*virtual*/ bool onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent);
	bool onAccept(frame::aio::ReactorContext &_rctx, SocketDevice &_rsd);
	
	typedef frame::aio::ListenerSocket	ListenerSocketT;
	typedef frame::aio::Timer			TimerT;
	
	frame::Manager		&rm;
	AioSchedulerT		&rsched;
	ListenerSocketT		sock;
	TimerT				timer;
};

class Connection: public Dynamic<Connection, frame::aio::Object>{
public:
	Connection(const SocketDevice &_rsd);
	~Connection();
private:
	/*virtual*/ bool onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent);
	
private:
	typedef frame::aio::Stream<frame::aio::Socket>	StreamSocketT;
	typedef frame::aio::Timer						TimerT;
	enum {BUFCP = 1024};
	
	char			buf[BUFCP];
	StreamSocketT	sock;
	TimerT			timer;
};


int main(int argc, char *argv[]){
	{
		frame::Manager		m;
		AioSchedulerT		s(m);
		
		if(!s.start(0)){
			running = false;
			cout<<"Error starting scheduler"<<endl;
		}else{
			ResolveData		rd =  synchronous_resolve(_addr, _port, 0, SocketInfo::Inet4, SocketInfo::Stream);
	
			SocketDevice	sd;
			
			sd.create(rd.begin());
			sd.makeNonBlocking();
			sd.prepareAccept(rd.begin(), 100);
			if(sd.ok()){
				DynamicPointer<frame::aio::Object>	objptr(new Listener(m, s, sd));
				m.registerObject(objptr, s, frame::Event(EventStartE));

			}else{
				cout<<"Error creating listener socket"<<endl;
				running = false;
			}
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

/*virtual*/ bool Listener::onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
	if(_revent.id == EventStartE){
		sock.scheduleAccept(_rctx, std::bind(&Listener::onAccept, this, _1, _2));
	}else if(_revent.id == EventStopE){
		return false;
	}
	return true;
}

bool Listener::onAccept(frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){
	unsigned	repeatcnt = 10;
	
	do{
		if(!_rctx.error()){
			DynamicPointer<frame::aio::Object>	objptr(new Connection(_rsd));
			rm.registerObject(objptr, rsched, frame::Event(EventStartE));
		}else{
			timer.waitFor(_rctx, TimeSpec(10), std::bind(&Listener::onAccept, this, _1, frame::Event(EventStartE)));
			break;
		}
		--repeatcnt;
	}while(repeatcnt && sock.accept(_rctx, std::bind(&Listener::onAccept, this, _1, _2)), _rsd);
	
	if(!repeatcnt){
		sock.scheduleAccept(_rctx, std::bind(&Listener::onAccept, this, _1, _2));//fully asynchronous call
	}
	return true;
}

//-----------------------------------------------------------------------------

/*virtual*/ bool Connection::onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
	if(_rctx.event().id == EventStartE){
		sock.scheduleRecvSome(_rctx, buf, BUFCP, std::bind(&Listener::onRecv, this, _1, _2));//fully asynchronous call
		timer.waitFor(_rctx, TimeSpec(30), std::bind(&Listener::onTimer, this, _1));
	}else if(_rctx.event().id == EventStopE){
		return false;
	}
	return true;
}

bool Connection::onRecv(frame::aio::ReactorContext &_rctx, size_t _sz){
	unsigned	repeatcnt = 10;
	do{
		if(!_rctx.error()){
			if(sock.writeAll(_rctx, buf, _sz, Event(EventSendE))){
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
	}while(repeatcnt && sock.recvSome(_rctx, buf, BUFCP, std::bind(&Listener::onRecv, this, _1, _2), _sz));
	
	timer.waitFor(_rctx, TimeSpec(30), std::bind(&Listener::onTimer, this, _1));
	return true;
}

bool Connection::onSend(frame::aio::ReactorContext &_rctx){
	if(!_rctx.error()){
		sock.scheduleRecvSome(_rctx, buf, BUFCP, std::bind(&Listener::onRecv, this, _1, _2));//fully asynchronous call
		timer.waitFor(_rctx, TimeSpec(30), std::bind(&Listener::onTimer, this, _1));
	}else{
		return false;
	}
	return true;
}

bool Connection::onTimer(frame::aio::ReactorContext &_rctx){
	return false;
}

//-----------------------------------------------------------------------------

