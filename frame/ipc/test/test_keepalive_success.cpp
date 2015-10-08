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
#include "frame/ipc/ipcerror.hpp"


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

int								test_scenario = 0;

/*
 * test_scenario == 0: test for error_too_many_keepalive_packets_received
 * test_scenario == 1: test for error_inactivity_timeout
*/

size_t real_size(size_t _sz){
	//offset + (align - (offset mod align)) mod align
	return _sz + ((sizeof(uint64) - (_sz % sizeof(uint64))) % sizeof(uint64));
}

struct Message: Dynamic<Message, frame::ipc::Message>{
	uint32							idx;
    std::string						str;
	
	Message(uint32 _idx):idx(_idx){
		idbg("CREATE ---------------- "<<(void*)this<<" idx = "<<idx);
		init();
		
	}
	Message(){
		idbg("CREATE ---------------- "<<(void*)this);
	}
	~Message(){
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

void server_connection_stop(frame::ipc::ConnectionContext &_rctx, ErrorConditionT const& _error){
	idbg(_rctx.connectionId()<<" error = "<<_error.message());
}

void server_connection_start(frame::ipc::ConnectionContext &_rctx){
	idbg(_rctx.connectionId());
	_rctx.service().activateConnection(_rctx.connectionId());
}


void client_receive_message(frame::ipc::ConnectionContext &_rctx, DynamicPointer<Message> &_rmsgptr){
	idbg(_rctx.connectionId());
	
	if(not _rmsgptr->check()){
		THROW_EXCEPTION("Message check failed.");
	}
	
	//cout<< _rmsgptr->str.size()<<'\n';
	transfered_size += _rmsgptr->str.size();
	++transfered_count;
	
	if(!_rmsgptr->isBackOnSender()){
		THROW_EXCEPTION("Message not back on sender!.");
	}
	
	++crtbackidx;
	
	if(crtbackidx == writecount){
		idbg("done running "<<crtackidx<<" "<<writecount);
		Locker<Mutex> lock(mtx);
		running = false;
		cnd.signal();
	}
}

void client_complete_message(frame::ipc::ConnectionContext &_rctx, DynamicPointer<Message> &_rmsgptr, ErrorConditionT const &_rerr){
	idbg(_rctx.connectionId());
	if(!_rerr){
		++crtackidx;
	}
}

void server_receive_message(frame::ipc::ConnectionContext &_rctx, DynamicPointer<Message> &_rmsgptr){
	idbg(_rctx.connectionId()<<" message id on sender "<<_rmsgptr->requestId());
	if(not _rmsgptr->check()){
		THROW_EXCEPTION("Message check failed.");
	}
	
	if(!_rmsgptr->isOnPeer()){
		THROW_EXCEPTION("Message not on peer!.");
	}
	
	//send message back
	_rctx.service().sendMessage(_rctx.connectionId(), _rmsgptr);
/*	
	++crtreadidx;
	idbg(crtreadidx);
	if(crtwriteidx < writecount){
		frame::ipc::MessagePointerT	msgptr(new Message(crtwriteidx));
		++crtwriteidx;
		pipcclient->sendMessage(
			"localhost:6666", msgptr,
			initarray[crtwriteidx % initarraysize].flags | frame::ipc::Message::WaitResponseFlagE
		);
	}*/
}

void server_complete_message(frame::ipc::ConnectionContext &_rctx, DynamicPointer<Message> &_rmsgptr, ErrorConditionT const &_rerr){
	idbg(_rctx.connectionId());
}


}//namespace

int test_keepalive_success(int argc, char **argv){
	Thread::init();
#ifdef SOLID_HAS_DEBUG
	Debug::the().levelMask("view");
	Debug::the().moduleMask("all");
	Debug::the().initStdErr(false, nullptr);
#endif
	
	size_t	max_per_pool_connection_count = 1;
	
	if(argc > 1){
		test_scenario = atoi(argv[1]);
		if(test_scenario != 0){
			THROW_EXCEPTION("Invalid test scenario.");
		}
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
					_rsp.registerType<Message>(
						serialization::basic_factory<Message>,
						client_receive_message, client_complete_message
					);
				}
			);
			
			if(test_scenario == 0){
				cfg.keepalive_timeout_seconds = 10;
			}
			
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
					_rsp.registerType<Message>(
						serialization::basic_factory<Message>,
						server_receive_message, server_complete_message
					);
				}
			);
			
			
			cfg.connection_stop_fnc = server_connection_stop;
			cfg.incoming_connection_start_fnc = server_connection_start;
			
			if(test_scenario == 0){
				cfg.inactivity_timeout_seconds = 20;
				cfg.inactivity_keepalive_count = 4;
			}
			
			cfg.listen_address_str = "0.0.0.0:6666";
			
			err = ipcserver.reconfigure(cfg);
			
			if(err){
				edbg("starting server ipcservice: "<<err.message());
				Thread::waitAll();
				return 1;
			}
		}
		
		pipcclient  = &ipcclient;
		
		writecount = 2;
		
		{
			frame::ipc::MessagePointerT	msgptr(new Message(crtwriteidx));
			++crtwriteidx;
			ipcclient.sendMessage(
				"localhost:6666", msgptr,
				initarray[crtwriteidx % initarraysize].flags | frame::ipc::Message::WaitResponseFlagE
			);
		}
		idbg("before sleep");
		Thread::sleep(1000 * 60);
		idbg("after sleep");
		{
			frame::ipc::MessagePointerT	msgptr(new Message(crtwriteidx));
			++crtwriteidx;
			ipcclient.sendMessage(
				"localhost:6666", msgptr,
				initarray[crtwriteidx % initarraysize].flags | frame::ipc::Message::WaitResponseFlagE
			);
		}
		
		
		Locker<Mutex>	lock(mtx);
		
		while(running){
			//cnd.wait(lock);
			TimeSpec	abstime = TimeSpec::createRealTime();
			abstime += (120 * 1000);
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