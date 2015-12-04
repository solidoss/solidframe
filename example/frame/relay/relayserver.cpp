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
		solid::frame::ObjectIdT
	>											UniqueMapT;
enum Events{
	EventStartE = 0,
	EventRunE,
	EventStopE,
	EventMoveE,
	EventResolveE,
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

frame::aio::Resolver& async_resolver(){
	static frame::aio::Resolver r;
	return r;
}

void connection_register(uint32 _id, frame::ObjectIdT const &_ruid){
	umap[_id] = _ruid;
}

frame::ObjectIdT connection_uid(uint32 _id){
	frame::ObjectIdT rv;
	auto it = umap.find(_id);
	if(it != umap.end()){
		rv = it->second;
		umap.erase(it);
	}
	return rv;
}

void connection_unregister(uint32 _id){
	connection_uid(_id);
}

}//namespace

//------------------------------------------------------------------
//------------------------------------------------------------------

class Listener: public Dynamic<Listener, frame::aio::Object>{
public:
	Listener(
		frame::Service &_rsvc,
		AioSchedulerT &_rsched,
		SocketDevice &&_rsd
	):rsvc(_rsvc), rsch(_rsched), sock(this->proxy(), std::move(_rsd)){}
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
	Connection(SocketDevice &&_rsd):sock1(this->proxy(), std::move(_rsd)), sock2(this->proxy()), recvcnt(0), sendcnt(0), crtid(-1){}
	~Connection(){}
protected:
	/*virtual*/ void onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent);
	/*virtual*/void doStop(frame::Manager &_rm);
	static void onRecvSock1(frame::aio::ReactorContext &_rctx, size_t _sz);
	static void onRecvSock2(frame::aio::ReactorContext &_rctx, size_t _sz);
	static void onSendSock1(frame::aio::ReactorContext &_rctx);
	static void onSendSock2(frame::aio::ReactorContext &_rctx);
	
	void onRecvId(frame::aio::ReactorContext &_rctx, size_t _off, size_t _sz);
	static void onSendId(frame::aio::ReactorContext &_rctx);
	
	void onConnect(frame::aio::ReactorContext &_rctx);
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
	uint32					crtid;
	//TimerT			timer;
};

bool parseArguments(Params &_par, int argc, char *argv[]);

int main(int argc, char *argv[]){
	
	if(parseArguments(params, argc, argv)) return 0;
	
	signal(SIGINT, term_handler); /* Die on SIGTERM */
	//signal(SIGPIPE, SIG_IGN);
	
	/*solid::*/Thread::init();
	
#ifdef SOLID_HAS_DEBUG
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
	async_resolver().start(1);
	{
		
		AioSchedulerT		sch;
		
		
		frame::Manager		m;
		frame::Service		svc(m);
		
		if(sch.start(1)){
			running = false;
			cout<<"Error starting scheduler"<<endl;
		}else{
			ResolveData		rd =  synchronous_resolve("0.0.0.0", params.listener_port, 0, SocketInfo::Inet4, SocketInfo::Stream);
	
			SocketDevice	sd;
			
			sd.create(rd.begin());
			sd.prepareAccept(rd.begin(), 2000);
			
			if(sd.ok()){
				DynamicPointer<frame::aio::Object>	objptr(new Listener(svc, sch, std::move(sd)));
				solid::ErrorConditionT				err;
				solid::frame::ObjectIdT			objuid;
				
				objuid = sch.startObject(objptr, svc, frame::EventCategory::createStart(), err);
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
		
		
		async_resolver().stop();
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
	idbg("event = "<<_revent);
	if(frame::EventCategory::isStart(_revent)){
		sock.postAccept(_rctx, std::bind(&Listener::onAccept, this, _1, _2));
	}else if(frame::EventCategory::isKill(_revent)){
		postStop(_rctx);
	}
}

void Listener::onAccept(frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){
	idbg("");
	unsigned	repeatcnt = 4;
	
	do{
		if(!_rctx.error()){
			int sz = 1024 * 64;
			_rsd.recvBufferSize(sz);
			sz = 1024 * 32;
			_rsd.sendBufferSize(sz);
			_rsd.enableNoDelay();
			DynamicPointer<frame::aio::Object>	objptr(new Connection(std::move(_rsd)));
			solid::ErrorConditionT				err;
			
			rsch.startObject(objptr, rsvc, frame::EventCategory::createStart(), err);
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

struct ResolveMessage: Dynamic<ResolveMessage>{
	ResolveData		rd;
	
	ResolveMessage(ResolveData &_rd):rd(_rd){}
};


struct ResolvFunc{
	frame::Manager		&rm;
	frame::ObjectIdT	objuid;
	
	ResolvFunc(frame::Manager &_rm, frame::ObjectIdT const& _robjuid): rm(_rm), objuid(_robjuid){}
	
	void operator()(ResolveData &_rrd, ERROR_NS::error_code const &_rerr){
		frame::Event		ev(frame::EventCategory::createMessage());
		ev.msgptr = frame::MessagePointerT(new ResolveMessage(_rrd));
		idbg(this<<" send resolv_message");
		rm.notify(objuid, ev);
	}
};

struct MoveMessage: Dynamic<MoveMessage>{
	SocketDevice		sd;
	uint8				sz;
	char 				buf[12];
	
	MoveMessage(
		SocketDevice &&_rsd, 
		char *_buf, uint8 _buflen
	): sd(std::move(_rsd))
	{
		cassert(_buflen < 12);
		memcpy(buf, _buf, _buflen);
	}
};

/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
	edbg(this<<" "<<_revent);
	if(frame::EventCategory::isStart(_revent)){
		//sock.postRecvSome(_rctx, buf, BufferCapacity, Connection::onRecv);//fully asynchronous call
		if(params.destination_addr_str.size()){
			//we must resolve the address then connect
			idbg("async_resolve = "<<params.destination_addr_str<<" "<<params.destination_port_str);
			async_resolver().requestResolve(
				ResolvFunc(_rctx.service().manager(), _rctx.service().manager().id(*this)), params.destination_addr_str.c_str(),
				params.destination_port_str.c_str(), 0, SocketInfo::Inet4, SocketInfo::Stream
			);
		}else{
			const uint32 id = crtid = crt_id++;
			
			snprintf(buf1, BufferCapacity, "%lu\r\n", id);
			sock1.postSendAll(_rctx, buf1, strlen(buf1), Connection::onSendId);
			sock1.postRecvSome(_rctx, buf2, 12, [this](frame::aio::ReactorContext &_rctx, size_t _sz){return onRecvId(_rctx, 0, _sz);});
		}
	}else if(frame::EventCategory::isKill(_revent)){
		edbg(this<<" postStop");
		//sock.shutdown(_rctx);
		postStop(_rctx);
	}else if(frame::EventCategory::isMessage(_revent)){
		MoveMessage *pmovemsg = MoveMessage::cast(_revent.msgptr.get());
		if(pmovemsg){
			sock2.reset(_rctx, std::move(pmovemsg->sd));
			if(pmovemsg->sz){
				memcpy(buf2, pmovemsg->buf, pmovemsg->sz);
				sock1.postSendAll(_rctx, buf2, pmovemsg->sz, Connection::onSendSock1);
			}else{
				sock2.postRecvSome(_rctx, buf2, BufferCapacity, Connection::onRecvSock2);
			}
			if(buf1[0]){
				sock2.postSendAll(_rctx, buf1 + 1, buf1[0], Connection::onSendSock2);
			}else{
				sock1.postRecvSome(_rctx, buf1, BufferCapacity, Connection::onRecvSock1);
			}
		}
		ResolveMessage *presolvemsg = ResolveMessage::cast(_revent.msgptr.get());
		if(presolvemsg){
			if(presolvemsg->rd.empty()){
				edbg(this<<" postStop");
				//sock.shutdown(_rctx);
				postStop(_rctx);
			}else{
				if(sock2.connect(_rctx, presolvemsg->rd.begin(), std::bind(&Connection::onConnect, this, _1))){
					onConnect(_rctx);
				}
			}
		}
	}
}

void Connection::onConnect(frame::aio::ReactorContext &_rctx){
	if(!_rctx.error()){
		idbg(this<<" SUCCESS");
		sock2.device().enableNoDelay();
		sock1.postRecvSome(_rctx, buf1, BufferCapacity, Connection::onRecvSock1);
		sock2.postRecvSome(_rctx, buf2, BufferCapacity, Connection::onRecvSock2);
	}else{
		edbg(this<<" postStop "<<recvcnt<<" "<<sendcnt<<" "<<_rctx.systemError().message());
		postStop(_rctx);
	}
}

/*virtual*/void Connection::doStop(frame::Manager &_rm){
	connection_unregister(crtid);
}

/*static*/ void Connection::onRecvSock1(frame::aio::ReactorContext &_rctx, size_t _sz){
	unsigned	repeatcnt = 4;
	Connection	&rthis = static_cast<Connection&>(_rctx.object());
	idbg(&rthis<<" "<<_sz);
	do{
		if(!_rctx.error()){
			bool rv = rthis.sock2.sendAll(_rctx, rthis.buf1, _sz, Connection::onSendSock2);
			if(rv){
				if(_rctx.error()){
					edbg(&rthis<<" postStop");
					rthis.postStop(_rctx);
					break;
				}
			}else{
				break;
			}
		}else{
			edbg(&rthis<<" postStop");
			rthis.postStop(_rctx);
			break;
		}
		--repeatcnt;
	}while(repeatcnt && rthis.sock1.recvSome(_rctx, rthis.buf1, BufferCapacity, Connection::onRecvSock1, _sz));
	
	if(repeatcnt == 0){
		bool rv = rthis.sock1.postRecvSome(_rctx, rthis.buf1, BufferCapacity, Connection::onRecvSock1);//fully asynchronous call
		cassert(!rv);
	}
}

/*static*/ void Connection::onRecvSock2(frame::aio::ReactorContext &_rctx, size_t _sz){
	unsigned	repeatcnt = 4;
	Connection	&rthis = static_cast<Connection&>(_rctx.object());
	idbg(&rthis<<" "<<_sz);
	do{
		if(!_rctx.error()){
			bool rv = rthis.sock1.sendAll(_rctx, rthis.buf2, _sz, Connection::onSendSock1);
			if(rv){
				if(_rctx.error()){
					edbg(&rthis<<" postStop");
					rthis.postStop(_rctx);
					break;
				}
			}else{
				break;
			}
		}else{
			edbg(&rthis<<" postStop");
			rthis.postStop(_rctx);
			break;
		}
		--repeatcnt;
	}while(repeatcnt && rthis.sock2.recvSome(_rctx, rthis.buf2, BufferCapacity, Connection::onRecvSock2, _sz));
	
	if(repeatcnt == 0){
		bool rv = rthis.sock2.postRecvSome(_rctx, rthis.buf2, BufferCapacity, Connection::onRecvSock2);//fully asynchronous call
		cassert(!rv);
	}
}


/*static*/ void Connection::onSendSock1(frame::aio::ReactorContext &_rctx){
	Connection &rthis = static_cast<Connection&>(_rctx.object());
	if(!_rctx.error()){
		rthis.sock2.postRecvSome(_rctx, rthis.buf2, BufferCapacity, Connection::onRecvSock2);
	}else{
		edbg(&rthis<<" postStop");
		rthis.postStop(_rctx);
	}
}

/*static*/ void Connection::onSendSock2(frame::aio::ReactorContext &_rctx){
	Connection &rthis = static_cast<Connection&>(_rctx.object());
	if(!_rctx.error()){
		rthis.sock1.postRecvSome(_rctx, rthis.buf1, BufferCapacity, Connection::onRecvSock1);
	}else{
		edbg(&rthis<<" postStop");
		rthis.postStop(_rctx);
	}
}


void Connection::onRecvId(frame::aio::ReactorContext &_rctx, size_t _off, size_t _sz){
	idbg("sz = "<<_sz<<" off = "<<_off);
	_sz += _off;
	
	size_t	i = _off;
	bool 	found_eol = false;
	for(; i < _sz; ++i){
		if(buf2[i] == '\n'){
			buf2[i] = '\0';
			found_eol = true;
			break;
		}
		if(buf2[i] == '\r'){
			buf2[i] = '\0';
			found_eol = true;
			if((i + 1) < _sz && buf2[i + 1] == '\n'){
				++i;
			} 
		} 
	}
	
	if(found_eol){
		uint32 idx = InvalidIndex();
		if(strlen(buf2) >= 1){
			sscanf(buf2, "%ul", &idx);
			idbg(this<<" received idx = "<<idx);
		}
		if(idx == crtid || idx == InvalidIndex()){
			//wait for a peer connection
			buf1[0] = _sz - i;
			buf1[1] = i;
			memcpy(buf1 + 1, buf2 + i, _sz - i);
			connection_register(crtid, _rctx.service().manager().id(*this));
		}else{
			//move to a peer connection
			frame::ObjectIdT	objid = connection_uid(idx);
			frame::Event		ev(frame::EventCategory::createMessage());
			SocketDevice		sd(sock1.reset(_rctx));
			ev.msgptr = frame::MessagePointerT(new MoveMessage(std::move(sd), buf2 + i, _sz - i));
			idbg(this<<" send move_message with size = "<<(_sz - i));
			_rctx.service().manager().notify(objid, ev);
			edbg(this<<" postStop");
			postStop(_rctx);
		}
	}else{
		//not found
		if(_sz < 12){
			_off = _sz;
			sock1.postRecvSome(_rctx, buf2 + _sz, 12 - _sz, [this, _off](frame::aio::ReactorContext &_rctx, size_t _sz){return onRecvId(_rctx, _off, _sz);});
		}else{
			edbg(this<<" postStop");
			postStop(_rctx);
		}
	}
}

/*static*/ void Connection::onSendId(frame::aio::ReactorContext &_rctx){
	Connection &rthis = static_cast<Connection&>(_rctx.object());
	if(!_rctx.error()){
		idbg(&rthis<<"");
	}else{
		edbg(&rthis<<" postStop "<<rthis.recvcnt<<" "<<rthis.sendcnt);
		rthis.postStop(_rctx);
	}
}

//-----------------------------------------------------------------------------

