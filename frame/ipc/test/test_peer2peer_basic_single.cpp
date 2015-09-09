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

std::atomic<size_t>				p1_crtwriteidx(0),	p2_crtwriteidx(0);
std::atomic<size_t>				p1_crtreadidx(0),	p2_crtreadidx(0);
std::atomic<size_t>				p1_crtbackidx(0),	p2_crtbackidx(0);
std::atomic<size_t>				p1_crtackidx(0),	p2_crtackidx(0);
std::atomic<size_t>				p1_writecount(0),	p2_writecount(0);

bool							running = true;
Mutex							mtx;
Condition						cnd;
frame::ipc::Service				*pipcpeer1 = nullptr;
frame::ipc::Service				*pipcpeer2 = nullptr;
std::atomic<uint64>				transfered_size(0);
std::atomic<size_t>				transfered_count(0);


size_t real_size(size_t _sz){
	//offset + (align - (offset mod align)) mod align
	return _sz + ((sizeof(uint64) - (_sz % sizeof(uint64))) % sizeof(uint64));
}


struct InitMessage: Dynamic<InitMessage, frame::ipc::Message>{
	std::string		port;
	
	InitMessage(std::string const &_port):port(_port){}
	InitMessage(){}
	
	template <class S>
	void serialize(S &_s, frame::ipc::ConnectionContext &_rctx){
		_s.push(port, "port");
	}
};

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

void peer1_connection_stop(frame::ipc::ConnectionContext &_rctx, ErrorConditionT const&){
	idbg(_rctx.connectionId());
}

void peer1_outgoing_connection_start(frame::ipc::ConnectionContext &_rctx){
	idbg(_rctx.connectionId());
	frame::ipc::MessagePointerT msgptr(new InitMessage("6666"));
	_rctx.service().sendMessage(_rctx.connectionId(), msgptr);
}


void peer1_incoming_connection_start(frame::ipc::ConnectionContext &_rctx){
	idbg(_rctx.connectionId());
	_rctx.service().activateConnection(_rctx.connectionId());
}

void peer2_connection_stop(frame::ipc::ConnectionContext &_rctx, ErrorConditionT const&){
	idbg(_rctx.connectionId());
}

void peer2_outgoing_connection_start(frame::ipc::ConnectionContext &_rctx){
	idbg(_rctx.connectionId());
	_rctx.service().activateConnection(_rctx.connectionId());
}

void peer2_incoming_connection_start(frame::ipc::ConnectionContext &_rctx){
	idbg(_rctx.connectionId());
	frame::ipc::MessagePointerT msgptr(new InitMessage("7777"));
	_rctx.service().sendMessage(_rctx.connectionId(), msgptr);
}

void peer1_receive_init(frame::ipc::ConnectionContext &_rctx, DynamicPointer<InitMessage> &_rmsgptr){
	SocketAddress				localaddr;
	SocketAddress				remoteaddr;
	std::ostringstream			tmposs;
	
	_rctx.device().localAddress(localaddr);
	_rctx.device().remoteAddress(remoteaddr);
	
	
	{
		std::string 				hoststr;
		std::string 				servstr;
		
		synchronous_resolve(
			hoststr, servstr,
			remoteaddr,
			ReverseResolveInfo::NumericHost
		);
		tmposs<<hoststr<<':'<<_rmsgptr->port;
	}
	
	
	idbg("Received init from: "<<tmposs.str());
	
	if(_rmsgptr->port.empty()){
		idbg("The peer did not accept the connection");
	}else if(_rmsgptr->port == "-"){
		idbg("The peer accepted the connection - we activate it too");
		_rctx.service().activateConnection(_rctx.connectionId());
	}else{
		idbg("Init on a incoming connection - try activate it");
		ErrorConditionT				err = _rctx.service().activateConnection(
			_rctx.connectionId(), tmposs.str().c_str(),
			[](ErrorConditionT const &_rerr){
				if(not _rerr){
					return std::pair<frame::ipc::MessagePointerT, uint32>(new InitMessage("-"), 0);
				}else{
					return std::pair<frame::ipc::MessagePointerT, uint32>(new InitMessage(), 0);
				}
			},
			localaddr < remoteaddr
		);
		
		if(err){
			edbg("Activating connection: "<<err.message());
		}
	}
}

void peer1_receive_message(frame::ipc::ConnectionContext &_rctx, DynamicPointer<Message> &_rmsgptr){
	idbg(_rctx.connectionId());
	
	if(not _rmsgptr->check()){
		THROW_EXCEPTION("Message check failed.");
	}
	
	transfered_size += _rmsgptr->str.size();
	++transfered_count;
	
	if(_rmsgptr->isBackOnSender()){
		++p1_crtbackidx;
	
		if(p1_crtbackidx == p1_writecount and p2_crtackidx == p2_writecount){
			Locker<Mutex> lock(mtx);
			running = false;
			cnd.signal();
		}
	}else if(_rmsgptr->isOnPeer()){
		_rctx.service().sendMessage(_rctx.connectionId(), _rmsgptr);
	
		++p2_crtreadidx;
		idbg(p2_crtreadidx);
		if(p2_crtwriteidx < p2_writecount){
			frame::ipc::MessagePointerT	msgptr(new Message(p2_crtwriteidx));
			++p2_crtwriteidx;
			pipcpeer2->sendMessage(
				"localhost:6666", msgptr,
				initarray[p2_crtwriteidx % initarraysize].flags | frame::ipc::Message::WaitResponseFlagE
			);
		}
	}
}

void peer1_complete_init(frame::ipc::ConnectionContext &_rctx, DynamicPointer<InitMessage> &_rmsgptr, ErrorConditionT const &_rerr){
	idbg(_rctx.connectionId());
}

void peer1_complete_message(frame::ipc::ConnectionContext &_rctx, DynamicPointer<Message> &_rmsgptr, ErrorConditionT const &_rerr){
	idbg(_rctx.connectionId());
	if(!_rerr){
		++p1_crtackidx;
	}
}


void peer2_receive_init(frame::ipc::ConnectionContext &_rctx, DynamicPointer<InitMessage> &_rmsgptr){
	SocketAddress				localaddr;
	SocketAddress				remoteaddr;
	std::ostringstream			tmposs;
	
	_rctx.device().localAddress(localaddr);
	_rctx.device().remoteAddress(remoteaddr);
	
	
	{
		std::string 				hoststr;
		std::string 				servstr;
		
		synchronous_resolve(
			hoststr, servstr,
			remoteaddr,
			ReverseResolveInfo::NumericHost
		);
		tmposs<<hoststr<<':'<<_rmsgptr->port;
	}
	
	
	idbg("Received init from: "<<tmposs.str());
	
	if(_rmsgptr->port.empty()){
		idbg("The peer did not accept the connection");
	}else if(_rmsgptr->port == "-"){
		idbg("The peer accepted the connection - we activate it too");
		_rctx.service().activateConnection(_rctx.connectionId());
	}else{
		idbg("Init on a incoming connection - try activate it");
		ErrorConditionT				err = _rctx.service().activateConnection(
			_rctx.connectionId(), tmposs.str().c_str(),
			[](ErrorConditionT const &_rerr){
				if(not _rerr){
					return std::pair<frame::ipc::MessagePointerT, uint32>(new InitMessage("-"), 0);
				}else{
					return std::pair<frame::ipc::MessagePointerT, uint32>(new InitMessage(), 0);
				}
			},
			localaddr < remoteaddr
		);
		
		if(err){
			edbg("Activating connection: "<<err.message());
		}
	}
}

void peer2_receive_message(frame::ipc::ConnectionContext &_rctx, DynamicPointer<Message> &_rmsgptr){
	idbg(_rctx.connectionId()<<" message id on sender "<<_rmsgptr->idOnSender());
	
	if(not _rmsgptr->check()){
		THROW_EXCEPTION("Message check failed.");
	}
	
	transfered_size += _rmsgptr->str.size();
	++transfered_count;
	
	
	if(_rmsgptr->isBackOnSender()){
		++p2_crtbackidx;
	
		if(p1_crtbackidx == p1_writecount and p2_crtackidx == p2_writecount){
			Locker<Mutex> lock(mtx);
			running = false;
			cnd.signal();
		}
	}else if(_rmsgptr->isOnPeer()){
		_rctx.service().sendMessage(_rctx.connectionId(), _rmsgptr);
	
		++p1_crtreadidx;
		idbg(p1_crtreadidx);
		if(p1_crtwriteidx < p1_writecount){
			frame::ipc::MessagePointerT	msgptr(new Message(p1_crtwriteidx));
			++p1_crtwriteidx;
			pipcpeer1->sendMessage(
				"localhost:7777", msgptr,
				initarray[p1_crtwriteidx % initarraysize].flags | frame::ipc::Message::WaitResponseFlagE
			);
		}
	}
}

void peer2_complete_init(frame::ipc::ConnectionContext &_rctx, DynamicPointer<InitMessage> &_rmsgptr, ErrorConditionT const &_rerr){
	idbg(_rctx.connectionId());
}

void peer2_complete_message(frame::ipc::ConnectionContext &_rctx, DynamicPointer<Message> &_rmsgptr, ErrorConditionT const &_rerr){
	idbg(_rctx.connectionId());
	if(!_rerr){
		++p1_crtackidx;
	}
}


}//namespace

int test_peer2peer_basic_single(int argc, char **argv){
	
	Thread::init();
	
#ifdef UDEBUG
	Debug::the().levelMask("view");
	Debug::the().moduleMask("frame_ipc");
	Debug::the().initStdErr(false, nullptr);
#endif
	
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
		AioSchedulerT			sch_peer1;
		AioSchedulerT			sch_peer2;
			
			
		frame::Manager			m;
		frame::ipc::Service		ipcpeer1(m);
		frame::ipc::Service		ipcpeer2(m);
		ErrorConditionT			err;
		
		frame::aio::Resolver	resolver;
		
		err = sch_peer1.start(1);
		
		if(err){
			edbg("starting aio peer1 scheduler: "<<err.message());
			return 1;
		}
		
		err = sch_peer2.start(1);
		
		if(err){
			edbg("starting aio peer2 scheduler: "<<err.message());
			return 1;
		}
		
		err = resolver.start(1);
		
		if(err){
			edbg("starting aio resolver: "<<err.message());
			return 1;
		}
		
		
		{//ipc peer1 initialization
			frame::ipc::Configuration	cfg(sch_peer1);
			
			cfg.protocolCallback(
				[&ipcpeer1](frame::ipc::ServiceProxy& _rsp){
					_rsp.registerType<InitMessage>(
						serialization::basic_factory<InitMessage>,
						peer1_receive_init, peer1_complete_init
					);
					_rsp.registerType<Message>(
						serialization::basic_factory<Message>,
						peer1_receive_message, peer1_complete_message
					);
				}
			);
			
			
			cfg.connection_stop_fnc = peer1_connection_stop;
			cfg.outgoing_connection_start_fnc = peer1_outgoing_connection_start;
			cfg.incoming_connection_start_fnc = peer1_incoming_connection_start;
			
			cfg.max_per_pool_connection_count = 1;
			
			cfg.name_resolve_fnc = frame::ipc::ResolverF(resolver, "7777");
			
			cfg.listen_address_str = "0.0.0.0:7777";
			
			err = ipcpeer1.reconfigure(cfg);
			
			if(err){
				edbg("starting peer1 ipcservice: "<<err.message());
				Thread::waitAll();
				return 1;
			}
		}
		
		{//ipc peer2 initialization
			frame::ipc::Configuration	cfg(sch_peer2);
			
			cfg.protocolCallback(
				[&ipcpeer2](frame::ipc::ServiceProxy& _rsp){
					_rsp.registerType<InitMessage>(
						serialization::basic_factory<InitMessage>,
						peer2_receive_init, peer2_complete_init
					);
					_rsp.registerType<Message>(
						serialization::basic_factory<Message>,
						peer2_receive_message, peer2_complete_message
					);
				}
			);
			
			
			cfg.connection_stop_fnc = peer2_connection_stop;
			cfg.outgoing_connection_start_fnc = peer2_outgoing_connection_start;
			cfg.incoming_connection_start_fnc = peer2_incoming_connection_start;
			
			
			cfg.max_per_pool_connection_count = 1;
			
			cfg.name_resolve_fnc = frame::ipc::ResolverF(resolver, "6666");
			
			cfg.listen_address_str = "0.0.0.0:7777";
			
			err = ipcpeer2.reconfigure(cfg);
			
			if(err){
				edbg("starting peer2 ipcservice: "<<err.message());
				Thread::waitAll();
				return 1;
			}
		}
		
		pipcpeer1  = &ipcpeer1;
		pipcpeer2  = &ipcpeer2;
		
		const size_t					start_count = 1;
		
		p1_writecount = start_count;//10*initarraysize;//
		p2_writecount = start_count;//10*initarraysize;//
		
		for(; p1_crtwriteidx < start_count; ++p1_crtwriteidx, ++p1_crtwriteidx){
			frame::ipc::MessagePointerT	msgptr(new Message(p1_crtwriteidx));
			ipcpeer1.sendMessage(
				"localhost:7777", msgptr,
				initarray[p1_crtwriteidx % initarraysize].flags | frame::ipc::Message::WaitResponseFlagE
			);
			ipcpeer2.sendMessage(
				"localhost:6666", msgptr,
				initarray[p1_crtwriteidx % initarraysize].flags | frame::ipc::Message::WaitResponseFlagE
			);
		}
		
		Locker<Mutex>	lock(mtx);
		
		while(running){
			//cnd.wait(lock);
			TimeSpec	abstime = TimeSpec::createRealTime();
			abstime += (60 * 1000);//ten seconds
			//cnd.wait(lock);
			//break;
			bool b = cnd.wait(lock, abstime);
			if(!b){
				//timeout expired
				THROW_EXCEPTION("Process is taking too long.");
			}
		}
		
		if(p1_crtwriteidx != p1_crtackidx){
			THROW_EXCEPTION("Not all peer1 messages were completed");
		}
		
		if(p2_crtwriteidx != p2_crtackidx){
			THROW_EXCEPTION("Not all peer2 messages were completed");
		}
		
		m.stop();
	}
	
	Thread::waitAll();
	std::cout<<"Transfered size = "<<(transfered_size)/1024<<"KB"<<endl;
	std::cout<<"Transfered count = "<<transfered_count<<endl;
	
	return 0;
}