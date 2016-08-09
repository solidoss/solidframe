#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aiotimer.hpp"
#include "solid/frame/aio/aiostream.hpp"
#include "solid/frame/aio/aiodatagram.hpp"
#include "solid/frame/aio/aiosocket.hpp"

#include <mutex>
#include <thread>
#include <condition_variable>

#include "solid/system/socketaddress.hpp"
#include "solid/system/socketdevice.hpp"
#include "solid/system/debug.hpp"

#include "solid/utility/event.hpp"

#include <signal.h>
#include <iostream>
#include <functional>

using namespace std;
using namespace solid;

using AioSchedulerT = frame::Scheduler<frame::aio::Reactor>;

//------------------------------------------------------------------
//------------------------------------------------------------------

namespace{

struct Params{
	int			listener_port;
	int			talker_port;
};
}//namespace

//------------------------------------------------------------------
//------------------------------------------------------------------

class Listener: public frame::aio::Object{
public:
	Listener(
		frame::Service &_rsvc,
		AioSchedulerT &_rsched,
		SocketDevice &&_rsd
	):
		rservice(_rsvc), rscheduler(_rsched), sock(this->proxy(), std::move(_rsd)), timer(this->proxy())
	{}
private:
	void onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent) override;
	void onAccept(frame::aio::ReactorContext &_rctx, SocketDevice &_rsd);
	
	using ListenerSocketT = frame::aio::Listener;
	using TimerT = frame::aio::Timer;
	
	frame::Service		&rservice;
	AioSchedulerT		&rscheduler;
	ListenerSocketT		sock;
	TimerT				timer;
};


class Connection: public frame::aio::Object{
public:
	Connection(SocketDevice &&_rsd):sock(this->proxy(), std::move(_rsd)), recvcnt(0), sendcnt(0){}
private:
	void onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent) override;
	static void onRecv(frame::aio::ReactorContext &_rctx, size_t _sz);
	static void onSend(frame::aio::ReactorContext &_rctx);
private:
	using  StreamSocketT = frame::aio::Stream<frame::aio::Socket>;
	enum {BufferCapacity = 1024 * 2};
	
	char			buf[BufferCapacity];
	StreamSocketT	sock;
	uint64_t 		recvcnt;
	uint64_t		sendcnt;
	size_t			sendcrt;
};

class Talker: public frame::aio::Object{
public:
	Talker(SocketDevice &&_rsd):sock(this->proxy(), std::move(_rsd)){}
private:
	void onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent) override;
	void onRecv(frame::aio::ReactorContext &_rctx, SocketAddress &_raddr, size_t _sz);
	void onSend(frame::aio::ReactorContext &_rctx);
private:
	using DatagramSocketT = frame::aio::Datagram<frame::aio::Socket>;
	
	enum {BufferCapacity = 1024 * 2 };
	
	char			buf[BufferCapacity];
	DatagramSocketT	sock;
};

bool parseArguments(Params &_par, int argc, char *argv[]);

int main(int argc, char *argv[]){
	Params p;
	if(not parseArguments(p, argc, argv)) return 0;
	
	signal(SIGPIPE, SIG_IGN);
	
	AioSchedulerT		scheduler;
	
	
	frame::Manager		manager;
	frame::Service		service(manager);
	
	if(scheduler.start(1)){
		cout<<"Error starting scheduler"<<endl;
		return 0;
	}
	
	{
		ResolveData		rd =  synchronous_resolve("0.0.0.0", p.listener_port, 0, SocketInfo::Inet4, SocketInfo::Stream);
		SocketDevice	sd;
		
		sd.create(rd.begin());
		sd.prepareAccept(rd.begin(), 2000);
		
		if(sd.ok()){
			
			{
				SocketAddress	sa;
				sd.localAddress(sa);
				cout<<"Listening for TCP connections on port: "<<sa<<endl;
			}
			
			DynamicPointer<frame::aio::Object>	objptr(new Listener(service, scheduler, std::move(sd)));
			solid::ErrorConditionT				error;
			solid::frame::ObjectIdT				objuid;
			
			objuid = scheduler.startObject(objptr, service, generic_event_category.event(GenericEvents::Start), error);
			(void)objuid;
		}else{
			cout<<"Error creating listener socket"<<endl;
			return 0;
		}
	}

	{
		ResolveData		rd =  synchronous_resolve("0.0.0.0", p.talker_port, 0, SocketInfo::Inet4, SocketInfo::Datagram);
		SocketDevice	sd;
		
		sd.create(rd.begin());
		sd.bind(rd.begin());
		
		if(sd.ok()){
			
			{
				SocketAddress	sa;
				sd.localAddress(sa);
				cout<<"Listening for UDP datagrams on port: "<<sa<<endl;
			}
			
			DynamicPointer<frame::aio::Object>	objptr(new Talker(std::move(sd)));
			
			solid::ErrorConditionT				error;
			solid::frame::ObjectIdT				objuid;
			
			objuid = scheduler.startObject(objptr, service, generic_event_category.event(GenericEvents::Start), error);
			
			(void)objuid;
			
		}else{
			cout<<"Error creating talker socket"<<endl;
			return 0;
		}
	}
	
	cout<<"Press any key and ENTER to terminate..."<<endl;
	char c;
	cin>>c;
	//manager.stop();
	return 0;
}

//-----------------------------------------------------------------------------
bool parseArguments(Params &_par, int argc, char *argv[]){
	cout<<"Usage:"<<endl<<"\t$ ./tutorial_aio_echo_server [tcp_port] [udp_port]"<<endl;
	cout<<"Example: "<<endl<<"\t$ ./tutorial_aio_echo_server 4444 5555"<<endl<<"\t$ nc localhost 4444"<<endl<<"\t$ nc -u4 localhost 5555"<<endl;
	_par.listener_port = 0;
	_par.talker_port = 0;
	if(argc > 1){
		_par.listener_port = atoi(argv[1]);
	}
	if(argc > 2){
		_par.talker_port = atoi(argv[2]);
	}
	return true;
}

//-----------------------------------------------------------------------------
//		Listener
//-----------------------------------------------------------------------------

/*virtual*/ void Listener::onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent){
	if(generic_event_category.event(GenericEvents::Start) == _revent){
		sock.postAccept(_rctx, [this](frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){return onAccept(_rctx, _rsd);});
	}else if(generic_event_category.event(GenericEvents::Kill) == _revent){
		postStop(_rctx);
	}
}

void Listener::onAccept(frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){
	unsigned	repeatcnt = 4;
	
	do{
		if(!_rctx.error()){
			DynamicPointer<frame::aio::Object>	objptr(new Connection(std::move(_rsd)));
			solid::ErrorConditionT				err;
			
			rscheduler.startObject(objptr, rservice, generic_event_category.event(GenericEvents::Start), err);
		}else{
			//e.g. a limit of open file descriptors was reached - we sleep for 10 seconds
			timer.waitFor(
				_rctx,
				 NanoTime(10),
				[this](frame::aio::ReactorContext &_rctx){
					sock.postAccept(_rctx, [this](frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){return onAccept(_rctx, _rsd);});
				}
			);
			break;
		}
		--repeatcnt;
	}while(repeatcnt && sock.accept(_rctx, [this](frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){return onAccept(_rctx, _rsd);}, _rsd));
	
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
/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent){
	if(generic_event_category.event(GenericEvents::Start) == _revent){
		sock.postRecvSome(_rctx, buf, BufferCapacity, Connection::onRecv);//fully asynchronous call
	}else if(generic_event_category.event(GenericEvents::Kill) == _revent){
		sock.shutdown(_rctx);
		postStop(_rctx);
	}
}

/*static*/ void Connection::onRecv(frame::aio::ReactorContext &_rctx, size_t _sz){
	unsigned	repeatcnt = 2;
	Connection	&rthis = static_cast<Connection&>(_rctx.object());
	do{
		if(!_rctx.error()){
			rthis.recvcnt += _sz;
			rthis.sendcrt = _sz;
			if(rthis.sock.sendAll(_rctx, rthis.buf, _sz, Connection::onSend)){
				if(_rctx.error()){
					rthis.postStop(_rctx);
					break;
				}
				rthis.sendcnt += rthis.sendcrt;
			}else{
				break;
			}
		}else{
			rthis.postStop(_rctx);
			break;
		}
		--repeatcnt;
	}while(repeatcnt && rthis.sock.recvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv, _sz));
	
	if(repeatcnt == 0){
		bool rv = rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv);//fully asynchronous call
		SOLID_ASSERT(!rv);
	}
}

/*static*/ void Connection::onSend(frame::aio::ReactorContext &_rctx){
	Connection &rthis = static_cast<Connection&>(_rctx.object());
	if(!_rctx.error()){
		rthis.sendcnt += rthis.sendcrt;
		rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv);//fully asynchronous call
	}else{
		rthis.postStop(_rctx);
	}
}

//-----------------------------------------------------------------------------
//		Talker
//-----------------------------------------------------------------------------

/*virtual*/ void Talker::onEvent(frame::aio::ReactorContext &_rctx, Event &&_revent){
	if(generic_event_category.event(GenericEvents::Start) == _revent){
		sock.postRecvFrom(
			_rctx, buf, BufferCapacity,
			[this](frame::aio::ReactorContext &_rctx, SocketAddress &_raddr, size_t _sz){onRecv(_rctx, _raddr, _sz);}
		);//fully asynchronous call
	}else if(generic_event_category.event(GenericEvents::Kill) == _revent){
		postStop(_rctx);
	}
}


void Talker::onRecv(frame::aio::ReactorContext &_rctx, SocketAddress &_raddr, size_t _sz){
	unsigned	repeatcnt = 10;
	do{
		if(!_rctx.error()){
			if(sock.sendTo(_rctx, buf, _sz, _raddr, [this](frame::aio::ReactorContext &_rctx){onSend(_rctx);})){
				if(_rctx.error()){
					postStop(_rctx);
					break;
				}
			}else{
				break;
			}
		}else{
			postStop(_rctx);
			break;
		}
		--repeatcnt;
	}while(
		repeatcnt and
		sock.recvFrom(
			_rctx, buf, BufferCapacity,
			[this](frame::aio::ReactorContext &_rctx, SocketAddress &_raddr, size_t _sz){onRecv(_rctx, _raddr, _sz);}, _raddr, _sz
		)
	);
	
	if(repeatcnt == 0){
		sock.postRecvFrom(
			_rctx, buf, BufferCapacity,
			[this](frame::aio::ReactorContext &_rctx, SocketAddress &_raddr, size_t _sz){onRecv(_rctx, _raddr, _sz);}
		);//fully asynchronous call
	}
}

void Talker::onSend(frame::aio::ReactorContext &_rctx){
	if(!_rctx.error()){
		sock.postRecvFrom(
			_rctx, buf, BufferCapacity,
			[this](frame::aio::ReactorContext &_rctx, SocketAddress &_raddr, size_t _sz){onRecv(_rctx, _raddr, _sz);}
		);//fully asynchronous call
	}else{
		postStop(_rctx);
	}
}
