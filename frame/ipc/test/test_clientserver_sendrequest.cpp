#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
#include "frame/service.hpp"

#include "frame/aio/aioreactor.hpp"
#include "frame/aio/aioobject.hpp"
#include "frame/aio/aiolistener.hpp"
#include "frame/aio/aiotimer.hpp"
#include "frame/aio/aioresolver.hpp"

#include "frame/aio/openssl/aiosecurecontext.hpp"
#include "frame/aio/openssl/aiosecuresocket.hpp"

#include "frame/ipc/ipcservice.hpp"
#include "frame/ipc/ipcconfiguration.hpp"


#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/exception.hpp"

#include "system/debug.hpp"

#include <iostream>

using namespace std;
using namespace solid;

typedef frame::Scheduler<frame::aio::Reactor>	AioSchedulerT;
typedef frame::aio::openssl::Context			SecureContextT;

namespace{

struct InitStub{
	size_t		size;
	ulong		flags;
};

InitStub initarray[] = {
	{100000, 0},
	{2000, 0},
	{4000, 0},
	{8000, 0},
	{16000, 0},
	{32000, 0},
	{64000, 0},
	{128000, 0},
	{256000, 0},
	{512000, 0},
	{1024000, 0},
	{2048000, 0},
	{4096000, 0},
	{8192000, 0},
	{16384000, 0}
};

std::string						pattern;
const size_t					initarraysize = sizeof(initarray)/sizeof(InitStub);

std::atomic<size_t>				crtwriteidx(0);
std::atomic<size_t>				crtreadidx(0);
std::atomic<size_t>				crtbackidx(0);
std::atomic<size_t>				crtackidx(0);
std::atomic<size_t>				writecount(0);

size_t							connection_count(0);

bool							running = true;
Mutex							mtx;
Condition						cnd;
frame::ipc::Service				*pipcclient = nullptr;
std::atomic<uint64>				transfered_size(0);
std::atomic<size_t>				transfered_count(0);


size_t real_size(size_t _sz){
	//offset + (align - (offset mod align)) mod align
	return _sz + ((sizeof(uint64) - (_sz % sizeof(uint64))) % sizeof(uint64));
}

struct Request: Dynamic<Request, frame::ipc::Message>{
	uint32							idx;
    std::string						str;
	
	Request(uint32 _idx):idx(_idx){
		idbg("CREATE ---------------- "<<(void*)this<<" idx = "<<idx);
		init();
		
	}
	Request(){
		idbg("CREATE ---------------- "<<(void*)this);
	}
	~Request(){
		idbg("DELETE ---------------- "<<(void*)this);
	}

	template <class S>
	void serialize(S &_s, frame::ipc::ConnectionContext &_rctx){
		_s.push(str, "str");
		_s.push(idx, "idx");
		
	}
	
	void init(){
		const size_t	sz = real_size(initarray[idx % initarraysize].size);
		str.resize(sz);
		const size_t	count = sz / sizeof(uint64);
		uint64			*pu  = reinterpret_cast<uint64*>(const_cast<char*>(str.data()));
		const uint64	*pup = reinterpret_cast<const uint64*>(pattern.data());
		const size_t	pattern_size = pattern.size() / sizeof(uint64);
		for(uint64 i = 0; i < count; ++i){
			pu[i] = pup[i % pattern_size];//pattern[i % pattern.size()];
		}
	}
	
	
	bool check()const{
		const size_t	sz = real_size(initarray[idx % initarraysize].size);
		idbg("str.size = "<<str.size()<<" should be equal to "<<sz);
		if(sz != str.size()){
			return false;
		}
		//return true;
		const size_t	count = sz / sizeof(uint64);
		const uint64	*pu = reinterpret_cast<const uint64*>(str.data());
		const uint64	*pup = reinterpret_cast<const uint64*>(pattern.data());
		const size_t	pattern_size = pattern.size() / sizeof(uint64);
		
		for(uint64 i = 0; i < count; ++i){
			if(pu[i] != pup[i % pattern_size]) return false;
		}
		return true;
	}
	
};

struct Response: Dynamic<Response, frame::ipc::Message>{
	uint32							idx;
	std::string						str;
	
	Response(const Request &_rreq): BaseT(_rreq), idx(_rreq.idx), str(_rreq.str){
		idbg("CREATE ---------------- "<<(void*)this);
	}
	
	Response(){
		idbg("CREATE ---------------- "<<(void*)this);
	}
	
	~Response(){
		idbg("DELETE ---------------- "<<(void*)this);
	}
	
	template <class S>
	void serialize(S &_s, frame::ipc::ConnectionContext &_rctx){
		_s.push(idx, "idx");
		_s.push(str, "str");
	}
};

void client_connection_stop(frame::ipc::ConnectionContext &_rctx, ErrorConditionT const&){
	idbg(_rctx.connectionId());
	if(!running){
		++connection_count;
	}
}

void client_connection_start(frame::ipc::ConnectionContext &_rctx){
	idbg(_rctx.connectionId());
	_rctx.service().activateConnection(_rctx.connectionId());
}

void server_connection_stop(frame::ipc::ConnectionContext &_rctx, ErrorConditionT const&){
	idbg(_rctx.connectionId());
}

void server_connection_start(frame::ipc::ConnectionContext &_rctx){
	idbg(_rctx.connectionId());
	_rctx.service().activateConnection(_rctx.connectionId());
}


void client_receive_request(frame::ipc::ConnectionContext &_rctx, DynamicPointer<Request> &_rmsgptr){
	idbg(_rctx.connectionId());
	THROW_EXCEPTION("Received request on client.");
}

void client_receive_response(frame::ipc::ConnectionContext &_rctx, DynamicPointer<Response> &_rmsgptr){
	idbg(_rctx.connectionId());
	
	transfered_size += _rmsgptr->str.size();
	++transfered_count;
	
	if(!_rmsgptr->isBackOnSender()){
		THROW_EXCEPTION("Message not back on sender!.");
	}
	
	//++crtbackidx; //NOTE: incremented on per-response callback
	
	if(crtbackidx == writecount){
		Locker<Mutex> lock(mtx);
		running = false;
		cnd.signal();
	}
}


void client_complete_request(frame::ipc::ConnectionContext &_rctx, DynamicPointer<Request> &_rmsgptr, ErrorConditionT const &_rerr){
	idbg(_rctx.connectionId());
}

void client_complete_response(frame::ipc::ConnectionContext &_rctx, DynamicPointer<Response> &_rmsgptr, ErrorConditionT const &_rerr){
	idbg(_rctx.connectionId());
}


void on_receive_response(frame::ipc::ConnectionContext &_rctx, DynamicPointer<Response> &_rmsgptr, ErrorConditionT const &_rerr){
	idbg(_rctx.connectionId());
	++crtbackidx;
	++crtackidx;
}

struct ResponseHandler{
	void operator()(frame::ipc::ConnectionContext &_rctx, DynamicPointer<Response> &_rmsgptr, ErrorConditionT const &_rerr){
		on_receive_response(_rctx, _rmsgptr, _rerr);
	}
};


void server_receive_request(frame::ipc::ConnectionContext &_rctx, DynamicPointer<Request> &_rmsgptr){
	idbg(_rctx.connectionId()<<" message id on sender "<<_rmsgptr->idOnSender());
	if(not _rmsgptr->check()){
		THROW_EXCEPTION("Message check failed.");
	}
	
	if(!_rmsgptr->isOnPeer()){
		THROW_EXCEPTION("Message not on peer!.");
	}
	
	//send message back
	frame::ipc::MessagePointerT	msgptr(new Response(*_rmsgptr));
	_rctx.service().sendMessage(_rctx.connectionId(), msgptr);
	
	++crtreadidx;
	idbg(crtreadidx);
	if(crtwriteidx < writecount){
		frame::ipc::MessagePointerT	msgptr(new Request(crtwriteidx));
		++crtwriteidx;
		pipcclient->sendRequest(
				"localhost:6666", msgptr,
				//on_receive_response
				ResponseHandler()
				/*[](frame::ipc::ConnectionContext &_rctx, DynamicPointer<Response> &_rmsgptr, ErrorConditionT const &_rerr)->void{
					on_receive_response(_rctx, _rmsgptr, _rerr);
				}*/,
				initarray[crtwriteidx % initarraysize].flags
			);
	}
}

void server_receive_response(frame::ipc::ConnectionContext &_rctx, DynamicPointer<Response> &_rmsgptr){
	idbg(_rctx.connectionId());
	THROW_EXCEPTION("Received response on server.");
}

void server_complete_request(frame::ipc::ConnectionContext &_rctx, DynamicPointer<Request> &_rmsgptr, ErrorConditionT const &_rerr){
	idbg(_rctx.connectionId());
	THROW_EXCEPTION("Complete request on server.");
}

void server_complete_response(frame::ipc::ConnectionContext &_rctx, DynamicPointer<Response> &_rmsgptr, ErrorConditionT const &_rerr){
	idbg(_rctx.connectionId());
}

}//namespace

int test_clientserver_sendrequest(int argc, char **argv){
	Thread::init();
#ifdef UDEBUG
	Debug::the().levelMask("view");
	Debug::the().moduleMask("all");
	Debug::the().initStdErr(false, nullptr);
#endif
	
	size_t max_per_pool_connection_count = 1;
	
	if(argc > 1){
		max_per_pool_connection_count = atoi(argv[1]);
	}
	
	for(int i = 0; i < 127; ++i){
		if(isprint(i) and !isblank(i)){
			pattern += static_cast<char>(i);
		}
	}
	
	size_t	sz = real_size(pattern.size());
	
	if(sz > pattern.size()){
		pattern.resize(sz - sizeof(uint64));
	}else if(sz < pattern.size()){
		pattern.resize(sz);
	}
	
	
	{
		AioSchedulerT			sch_client;
		AioSchedulerT			sch_server;
			
			
		frame::Manager			m;
		frame::ipc::Service		ipcserver(m);
		frame::ipc::Service		ipcclient(m);
		ErrorConditionT			err;
		
		frame::aio::Resolver	resolver;
		
		err = sch_client.start(1);
		
		if(err){
			edbg("starting aio client scheduler: "<<err.message());
			return 1;
		}
		
		err = sch_server.start(1);
		
		if(err){
			edbg("starting aio server scheduler: "<<err.message());
			return 1;
		}
		
		err = resolver.start(1);
		
		if(err){
			edbg("starting aio resolver: "<<err.message());
			return 1;
		}
		
		
		{//ipc client initialization
			frame::ipc::Configuration	cfg(sch_client);
			
			cfg.protocolCallback(
				[&ipcclient](frame::ipc::ServiceProxy& _rsp){
					_rsp.registerType<Request>(
						serialization::basic_factory<Request>,
						client_receive_request, client_complete_request
					);
					_rsp.registerType<Response>(
						serialization::basic_factory<Response>,
						client_receive_response, client_complete_response
					);
				}
			);
			
			
			cfg.connection_stop_fnc = client_connection_stop;
			cfg.outgoing_connection_start_fnc = client_connection_start;
			
			cfg.max_per_pool_connection_count = max_per_pool_connection_count;
			
			cfg.name_resolve_fnc = frame::ipc::ResolverF(resolver, "6666"/*, SocketInfo::Inet4*/);
			
			err = ipcclient.reconfigure(cfg);
			
			if(err){
				edbg("starting client ipcservice: "<<err.message());
				Thread::waitAll();
				return 1;
			}
		}
		
		{//ipc server initialization
			frame::ipc::Configuration	cfg(sch_server);
			
			cfg.protocolCallback(
				[&ipcserver](frame::ipc::ServiceProxy& _rsp){
					_rsp.registerType<Request>(
						serialization::basic_factory<Request>,
						server_receive_request, server_complete_request
					);
					_rsp.registerType<Response>(
						serialization::basic_factory<Response>,
						server_receive_response, server_complete_response
					);
				}
			);
			
			
			cfg.connection_stop_fnc = server_connection_stop;
			cfg.incoming_connection_start_fnc = server_connection_start;
			
			cfg.listen_address_str = "0.0.0.0:6666";
			
			err = ipcserver.reconfigure(cfg);
			
			if(err){
				edbg("starting server ipcservice: "<<err.message());
				Thread::waitAll();
				return 1;
			}
		}
		
		pipcclient  = &ipcclient;
		
		const size_t		start_count = initarraysize/2;
		
		writecount = initarraysize;
		
		for(; crtwriteidx < start_count;){
			frame::ipc::MessagePointerT	msgptr(new Request(crtwriteidx));
			++crtwriteidx;
			ipcclient.sendRequest(
				"localhost:6666", msgptr,
				
				//ResponseHandler()
				[](frame::ipc::ConnectionContext &_rctx, DynamicPointer<Response> &_rmsgptr, ErrorConditionT const &_rerr)->void{
					on_receive_response(_rctx, _rmsgptr, _rerr);
				},
				initarray[crtwriteidx % initarraysize].flags
			);
		}
		
		Locker<Mutex>	lock(mtx);
		
		while(running){
			//cnd.wait(lock);
			TimeSpec	abstime = TimeSpec::createRealTime();
			abstime += (120 * 1000);//ten seconds
			cnd.wait(lock);
			bool b = true;//cnd.wait(lock, abstime);
			if(!b){
				//timeout expired
				THROW_EXCEPTION("Process is taking too long.");
			}
		}
		
		if(crtwriteidx != crtackidx){
			THROW_EXCEPTION("Not all messages were completed");
		}
		
		m.stop();
	}
	
	Thread::waitAll();
	
	std::cout<<"Transfered size = "<<(transfered_size * 2)/1024<<"KB"<<endl;
	std::cout<<"Transfered count = "<<transfered_count<<endl;
	std::cout<<"Connection count = "<<connection_count<<endl;
	
	return 0;
}