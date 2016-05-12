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
#include "frame/ipc/ipcprotocol_serialization_v1.hpp"


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
	{100000, 0}
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

struct Message: Dynamic<Message, frame::ipc::Message>{
	uint32							idx;
    std::string						str;
	bool							serialized;
	
	Message(uint32 _idx):idx(_idx), serialized(false){
		idbg("CREATE ---------------- "<<(void*)this<<" idx = "<<idx);
		init();
		
	}
	Message(): serialized(false){
		idbg("CREATE ---------------- "<<(void*)this);
	}
	~Message(){
		idbg("DELETE ---------------- "<<(void*)this);
		if(serialized){
			SOLID_THROW("Message serialized.");
		}
	}

	template <class S>
	void serialize(S &_s, frame::ipc::ConnectionContext &_rctx){
		_s.push(str, "str");
		_s.push(idx, "idx");
		
		if(S::IsSerializer){
			serialized = true;
		}
	}
	
	void init(){
		const size_t	sz = real_size(initarray[idx % initarraysize].size);
		str.resize(sz);
		const size_t	count = sz / sizeof(uint64);
		uint64			*pu  = reinterpret_cast<uint64*>(const_cast<char*>(str.data()));
		const uint64	*pup = reinterpret_cast<const uint64*>(pattern.data());
		const size_t	pattern_size = pattern.size() / sizeof(uint64);
		for(uint64 i = 0; i < count; ++i){
			pu[i] = pup[(idx + i) % pattern_size];//pattern[i % pattern.size()];
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
			if(pu[i] != pup[(i + idx) % pattern_size]){
				SOLID_THROW("Message check failed.");
				return false;
			}
		}
		return true;
	}
	
};

void client_connection_stop(frame::ipc::ConnectionContext &_rctx, ErrorConditionT const&){
	idbg(_rctx.recipientId());
	if(!running){
		++connection_count;
	}
}

void client_connection_start(frame::ipc::ConnectionContext &_rctx){
	idbg(_rctx.recipientId());
}

void client_complete_message(
	frame::ipc::ConnectionContext &_rctx,
	DynamicPointer<Message> &_rsent_msg_ptr, DynamicPointer<Message> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	idbg(_rctx.recipientId());
	
	SOLID_CHECK(_rrecv_msg_ptr.empty());
	SOLID_CHECK(_rsent_msg_ptr.get() != nullptr);
	
	SOLID_CHECK(_rerror == frame::ipc::error_connection_message_canceled);
	
	{
		Locker<Mutex> lock(mtx);
		running = false;
		cnd.signal();
	}
}

}//namespace

int test_clientserver_noserver(int argc, char **argv){
	Thread::init();
#ifdef SOLID_HAS_DEBUG
	Debug::the().levelMask("ew");
	Debug::the().moduleMask("frame_ipc:ew any:ew");
	Debug::the().initStdErr(false, nullptr);
	//Debug::the().initFile("test_clientserver_basic", false);
#endif
	
	size_t max_per_pool_connection_count = 1;
	
	if(argc > 1){
		max_per_pool_connection_count = atoi(argv[1]);
		if(max_per_pool_connection_count == 0){
			max_per_pool_connection_count = 1;
		}
		if(max_per_pool_connection_count > 100){
			max_per_pool_connection_count = 100;
		}
	}
	
	for(int j = 0; j < 1; ++j){
		for(int i = 0; i < 127; ++i){
			int c = (i + j) % 127;
			if(isprint(c) and !isblank(c)){
				pattern += static_cast<char>(c);
			}
		}
	}
	
	const size_t	sz = real_size(pattern.size());
	
	if(sz > pattern.size()){
		pattern.resize(sz - sizeof(uint64));
	}else if(sz < pattern.size()){
		pattern.resize(sz);
	}
	
	{
		AioSchedulerT			sch_client;
			
			
		frame::Manager			m;
		frame::ipc::Service		ipcclient(m);
		ErrorConditionT			err;
		
		frame::aio::Resolver	resolver;
		
		err = sch_client.start(1);
		
		if(err){
			edbg("starting aio client scheduler: "<<err.message());
			return 1;
		}
		
		err = resolver.start(1);
		
		if(err){
			edbg("starting aio resolver: "<<err.message());
			return 1;
		}
		
		std::string		server_port;
		
		{//ipc client initialization
			frame::ipc::serialization_v1::Protocol	*proto = new frame::ipc::serialization_v1::Protocol;
			frame::ipc::Configuration				cfg(sch_client, proto);
			
			proto->registerType<Message>(
				serialization::basic_factory<Message>,
				client_complete_message
			);
			
			//cfg.recv_buffer_capacity = 1024;
			//cfg.send_buffer_capacity = 1024;
			
			cfg.connection_stop_fnc = client_connection_stop;
			cfg.connection_start_outgoing_fnc = client_connection_start;
			
			cfg.pool_max_active_connection_count = max_per_pool_connection_count;
			
			cfg.connection_start_state = frame::ipc::ConnectionState::Active;
			
			cfg.name_resolve_fnc = frame::ipc::InternetResolverF(resolver, server_port.c_str()/*, SocketInfo::Inet4*/);
			
			err = ipcclient.reconfigure(std::move(cfg));
			
			if(err){
				edbg("starting client ipcservice: "<<err.message());
				Thread::waitAll();
				return 1;
			}
		}
		
		pipcclient  = &ipcclient;
		
		frame::ipc::RecipientId		recipient_id;
		frame::ipc::MessageId		message_id;
		{
			frame::ipc::MessagePointerT		msgptr(new Message(0));
			
			err = ipcclient.sendMessage(
				"localhost", msgptr,
				recipient_id, message_id,
				frame::ipc::Message::WaitResponseFlagE
			);
			SOLID_CHECK(not err);
		}
		
		Thread::sleep(30 * 1000);
		
		err = ipcclient.cancelMessage(recipient_id, message_id);
		
		SOLID_CHECK(not err);
		
		Locker<Mutex>	lock(mtx);
		
		while(running){
			//cnd.wait(lock);
			TimeSpec	abstime = TimeSpec::createRealTime();
			abstime += (120 * 1000);//ten seconds
			cnd.wait(lock);
			bool b = true;//cnd.wait(lock, abstime);
			if(!b){
				//timeout expired
				SOLID_THROW("Process is taking too long.");
			}
		}
		
		if(crtwriteidx != crtackidx){
			SOLID_THROW("Not all messages were completed");
		}
		
		m.stop();
	}
	
	Thread::waitAll();
	
	std::cout<<"Transfered size = "<<(transfered_size * 2)/1024<<"KB"<<endl;
	std::cout<<"Transfered count = "<<transfered_count<<endl;
	std::cout<<"Connection count = "<<connection_count<<endl;
	
	return 0;
}
