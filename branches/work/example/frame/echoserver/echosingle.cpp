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

using namespace std;
using namespace solid;

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

class Listener: public Dynamic<Listener, frame::aio::Object>{
public:
	Listener(frame::Manager &_rm, AioSchedulerT &_rsched, const SocketDevice &_rsd);
	~Listener();
private:
	/*virtual*/ void onEvent(ReactorContext &_rctx);
	/*virtual*/ void onEvent(ReactorContext &_rctx, SocketDevice &_rdev);
	
	typedef frame::aio::Listener<frame::aio::Socket>	ListenerSocketT;
	
	frame::Manager		&rm;
	AioSchedulerT		&rsched;
	ListenerSocketT		sock;
};

class Connection: public Dynamic<Connection, frame::aio::Object>{
public:
	Connection(const SocketDevice &_rsd);
	~Connection();
private:
	/*virtual*/ void onEvent(ReactorContext &_rexectx);
	/*virtual*/ void onEvent(ReactorContext &_rexectx, size_t _data);
	
private:
	typedef frame::aio::Stream<frame::aio::Socket>	StreamSocketT;
	enum {BUFCP = 1024};
	char			buf[BUFCP];
	StreamSocketT	sock;
};


int main(int argc, char *argv[]){
	{
		frame::Manager		m;
		AioSchedulerT		s(m);
		
		if(!s.start(0)){
			running = false;
			cout<<"Error starting scheduler"<<endl;
		}
		{
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
		m.stop(Event(EventStopE));
	}
	Thread::waitAll();
	return 0;
}

//-----------------------------------------------------------------------------

/*virtual*/ void Listener::onEvent(ReactorContext &_rctx){
	if(_rctx.event().id == EventStartE){
		sock.asyncAccept(_rctx, Event());//fully asynchronous call
	}else if(_rctx.event().id == EventStopE){
		this->stop(_rctx);
	}
}
/*virtual*/ void Listener::onEvent(ReactorContext &_rctx, SocketDevice &_rsd){
	unsigned		repeatcnt = 10;
	do{
		if(!_rctx.error()){
			DynamicPointer<frame::aio::Object>	objptr(new Connection(sd));
			rm.registerObject(objptr, rsched, frame::Event(EventStartE));
		}else{
			//timer.waitFor(_rctx, 10);
			break;
		}
		--repeatcnt;
	}while(repeatcnt && sock.accept(_rctx, _rsd, Event()));
	if(!repeatcnt){
		sock.accept(_rctx, Event());//fully asynchronous call
	}
}

//-----------------------------------------------------------------------------

/*virtual*/ void Connection::onEvent(ReactorContext &_rctx){
	if(_rctx.event().id == EventStartE){
		sock.asyncRecvSome(_rctx, buf, BUFCP, Event());//fully asynchronous call
	}else if(_rctx.event().id == EventSendE){
		if(!_rctx.error()){
			sock.asyncRecvSome(_rctx, buf, BUFCP, Event());//fully asynchronous call
		}else{
			this->stop(_rctx);
		}
	}else if(_rctx.event().id == EventStopE){
		this->stop(_rctx);
	}
}

/*virtual*/ void Connection::onEvent(ReactorContext &_rctx, size_t _sz){
	unsigned		repeatcnt = 10;
	do{
		if(!_rctx.error()){
			if(sock.writeAll(_rctx, buf, _sz, Event(EventSendE))){
				if(_rctx.error()){
					this->stop(_rctx);
				}
			}else{
				break;
			}
		}else{
			this->stop(_rctx);
		}
		--repeatcnt;
	}while(repeatcnt && sock.recvSome(_rctx, buf, BUFCP, _sz, Event()));
}

//-----------------------------------------------------------------------------

