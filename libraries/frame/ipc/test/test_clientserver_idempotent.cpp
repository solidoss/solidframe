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

#include <signal.h>
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
	{8192000, 0},
	{16384000, 0},
	{2000, 0},
	{16384000, 0},
	{8192000, 0}
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
bool							start_sleep = false;
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
		idbg("DELETE ---------------- "<<(void*)this<<" idx = "<<idx<<" str.size = "<<str.size());
// 		if(not serialized and not this->isBackOnSender() and idx != 0){
// 			THROW_EXCEPTION("Message not serialized.");
// 		}
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
				THROW_EXCEPTION("Message check failed.");
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
	auto lambda =  [](frame::ipc::ConnectionContext&, ErrorConditionT const& _rerror){
		idbg("enter active error: "<<_rerror.message());
		return frame::ipc::MessagePointerT();
	};
	_rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId(), lambda);
}

void server_connection_stop(frame::ipc::ConnectionContext &_rctx, ErrorConditionT const&){
	idbg(_rctx.recipientId());
}

void server_connection_start(frame::ipc::ConnectionContext &_rctx){
	idbg(_rctx.recipientId());
	auto lambda =  [](frame::ipc::ConnectionContext&, ErrorConditionT const& _rerror){
		idbg("enter active error: "<<_rerror.message());
		return frame::ipc::MessagePointerT();
	};
	_rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId(), lambda);
}


void client_complete_message(
	frame::ipc::ConnectionContext &_rctx,
	DynamicPointer<Message> &_rsent_msg_ptr, DynamicPointer<Message> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	idbg(_rctx.recipientId());
	
	if(_rsent_msg_ptr.get()){
		idbg("idx = "<<_rsent_msg_ptr->idx);
		if(!_rerror){
			++crtackidx;
		}else{
			idbg("send message complete: "<<_rerror.message());
			cassert(_rsent_msg_ptr->idx == 0);
		}
	}
	if(_rrecv_msg_ptr.get()){
		idbg("idx = "<<_rrecv_msg_ptr->idx);
		if(not _rrecv_msg_ptr->check()){
			THROW_EXCEPTION("Message check failed.");
		}
		
		cassert(not _rerror);
		
		//cout<< _rmsgptr->str.size()<<'\n';
		
		if(!_rrecv_msg_ptr->isBackOnSender()){
			THROW_EXCEPTION("Message not back on sender!.");
		}
		
		cassert(_rrecv_msg_ptr->idx == 1 or _rrecv_msg_ptr->idx == 3 or _rrecv_msg_ptr->idx == 4);
		
		transfered_size += _rrecv_msg_ptr->str.size();
		++transfered_count;
		++crtbackidx;
		
		if(crtbackidx == writecount){
			Locker<Mutex> lock(mtx);
			running = false;
			cnd.signal();
		}
	}
}

void server_complete_message(
	frame::ipc::ConnectionContext &_rctx,
	DynamicPointer<Message> &_rsent_msg_ptr, DynamicPointer<Message> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	if(_rrecv_msg_ptr.get()){
		idbg(_rctx.recipientId()<<" received message with id on sender "<<_rrecv_msg_ptr->requestId());
		
		if(not _rrecv_msg_ptr->check()){
			THROW_EXCEPTION("Message check failed.");
		}
		
		if(!_rrecv_msg_ptr->isOnPeer()){
			THROW_EXCEPTION("Message not on peer!.");
		}
		
		if(_rrecv_msg_ptr->idx == 2){
			Locker<Mutex> lock(mtx);
			start_sleep = true;
			cnd.signal();
			return;
		}
		
		//send message back
		if(_rctx.recipientId().isInvalidConnection()){
			THROW_EXCEPTION("Connection id should not be invalid!");
		}
		ErrorConditionT err = _rctx.service().sendMessage(_rctx.recipientId(), std::move(_rrecv_msg_ptr));
		
		if(err){
			THROW_EXCEPTION_EX("Connection id should not be invalid!", err.message());
		}
	}
	if(_rsent_msg_ptr.get()){
		idbg(_rctx.recipientId()<<" done sent message "<<_rsent_msg_ptr.get());
	}
}

}//namespace

int test_clientserver_idempotent(int argc, char **argv){
	
	signal(SIGPIPE, SIG_IGN);
	
	Thread::init();
#ifdef SOLID_HAS_DEBUG
	Debug::the().levelMask("view");
	Debug::the().moduleMask("frame_ipc:view any:view");
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
		
		std::string		server_port("39999");
		
		{//ipc server initialization
			frame::ipc::serialization_v1::Protocol	*proto = new frame::ipc::serialization_v1::Protocol;
			frame::ipc::Configuration				cfg(sch_server, proto);
			
			proto->registerType<Message>(
				serialization::basic_factory<Message>,
				server_complete_message
			);
			
			//cfg.recv_buffer_capacity = 1024;
			//cfg.send_buffer_capacity = 1024;
			
			cfg.connection_stop_fnc = server_connection_stop;
			cfg.connection_start_incoming_fnc = server_connection_start;
			
			cfg.listener_address_str = "0.0.0.0:";
			cfg.listener_address_str += server_port;
			
			err = ipcserver.reconfigure(std::move(cfg));
			
			if(err){
				edbg("starting server ipcservice: "<<err.message());
				Thread::waitAll();
				return 1;
			}
			
			{
				std::ostringstream oss;
				oss<<ipcserver.configuration().listenerPort();
				server_port = oss.str();
				idbg("server listens on port: "<<server_port);
			}
		}
		
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
			
			cfg.name_resolve_fnc = frame::ipc::InternetResolverF(resolver, server_port.c_str()/*, SocketInfo::Inet4*/);
			
			err = ipcclient.reconfigure(std::move(cfg));
			
			if(err){
				edbg("starting client ipcservice: "<<err.message());
				Thread::waitAll();
				return 1;
			}
		}
		
		pipcclient  = &ipcclient;
		
		{
			frame::ipc::MessagePointerT	msgptr(new Message(0));
			ipcclient.sendMessage(
				"localhost", msgptr,
				 frame::ipc::Message::WaitResponseFlagE
			);
		}
		
		{
			frame::ipc::MessagePointerT	msgptr(new Message(1));
			ipcclient.sendMessage(
				"localhost", msgptr,
				 frame::ipc::Message::WaitResponseFlagE | frame::ipc::Message::IdempotentFlagE
			);
		}
		
		{
			frame::ipc::MessagePointerT	msgptr(new Message(2));
			ipcclient.sendMessage(
				"localhost", msgptr,
				 frame::ipc::Message::OneShotSendFlagE
			);
		}
		
		{
			frame::ipc::MessagePointerT	msgptr(new Message(3));
			ipcclient.sendMessage(
				"localhost", msgptr,
				 frame::ipc::Message::IdempotentFlagE | frame::ipc::Message::SynchronousFlagE
			);
		}
		
		{
			frame::ipc::MessagePointerT	msgptr(new Message(4));
			ipcclient.sendMessage(
				"localhost", msgptr,
				 frame::ipc::Message::WaitResponseFlagE | frame::ipc::Message::SynchronousFlagE
			);
		}
		
		writecount = 3;
		
		{
			Locker<Mutex>	lock(mtx);
			while(!start_sleep){
				TimeSpec	abstime = TimeSpec::createRealTime();
				abstime += (10 * 1000);
				
				bool b = cnd.wait(lock, abstime);
				if(!b){
					//timeout expired
					THROW_EXCEPTION("Process is taking too long.");
				}
			}
		}
		
		idbg("---- Before server stopping ----");
		ipcserver.stop();
		idbg("---- After server stopped ----");
		
		Thread::sleep(5 * 1000);
		
		idbg("---- Before server start ----");
		ipcserver.start();
		idbg("---- After server started ----");
		
		Locker<Mutex>	lock(mtx);
		
		while(running){
			//cnd.wait(lock);
			TimeSpec	abstime = TimeSpec::createRealTime();
			abstime += (30 * 1000);
			cnd.wait(lock);
			bool b = true;// cnd.wait(lock, abstime);
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
