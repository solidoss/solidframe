#include "../alphamessages.hpp"
#include "alphaclient.hpp"
#include <system/condition.hpp>

using namespace std;
using namespace solid;

namespace alpha_client {

using IpcServicePointerT = shared_ptr<frame::ipc::Service>;
IpcServicePointerT		ipcclient_ptr;

Context					*pctx;

void client_connection_stop(frame::ipc::ConnectionContext &_rctx, ErrorConditionT const&){
	idbg(_rctx.recipientId());
}

void client_connection_start(frame::ipc::ConnectionContext &_rctx){
	idbg(_rctx.recipientId());
}


template <class M>
void complete_message(
	frame::ipc::ConnectionContext &_rctx,
	std::shared_ptr<M> &_rsent_msg_ptr,
	std::shared_ptr<M> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
);


template <>
void complete_message<alpha_protocol::FirstMessage>(
	frame::ipc::ConnectionContext &_rctx,
	std::shared_ptr<alpha_protocol::FirstMessage> &_rsent_msg_ptr,
	std::shared_ptr<alpha_protocol::FirstMessage> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	idbg("");
	SOLID_CHECK(not _rerror);
	SOLID_CHECK(_rsent_msg_ptr and _rrecv_msg_ptr);
	
	SOLID_CHECK(_rsent_msg_ptr->v == _rrecv_msg_ptr->v);
	SOLID_CHECK(_rsent_msg_ptr->str == _rrecv_msg_ptr->str);
	{
		Locker<Mutex>	lock(pctx->rmtx);
		--pctx->rwait_count;
		if(pctx->rwait_count == 0){
			pctx->rcnd.signal();
		}
	}
}

template <>
void complete_message<alpha_protocol::SecondMessage>(
	frame::ipc::ConnectionContext &_rctx,
	std::shared_ptr<alpha_protocol::SecondMessage> &_rsent_msg_ptr,
	std::shared_ptr<alpha_protocol::SecondMessage> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	idbg("");
	SOLID_CHECK(not _rerror);
	SOLID_CHECK(_rsent_msg_ptr and _rrecv_msg_ptr);
	
	SOLID_CHECK(_rsent_msg_ptr->v == _rrecv_msg_ptr->v);
	SOLID_CHECK(_rsent_msg_ptr->str == _rrecv_msg_ptr->str);
	{
		Locker<Mutex>	lock(pctx->rmtx);
		--pctx->rwait_count;
		if(pctx->rwait_count == 0){
			pctx->rcnd.signal();
		}
	}
}

template <>
void complete_message<alpha_protocol::ThirdMessage>(
	frame::ipc::ConnectionContext &_rctx,
	std::shared_ptr<alpha_protocol::ThirdMessage> &_rsent_msg_ptr,
	std::shared_ptr<alpha_protocol::ThirdMessage> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	idbg("");
	SOLID_CHECK(not _rerror);
	SOLID_CHECK(_rsent_msg_ptr and _rrecv_msg_ptr);
	
	SOLID_CHECK(_rsent_msg_ptr->v == _rrecv_msg_ptr->v);
	SOLID_CHECK(_rsent_msg_ptr->str == _rrecv_msg_ptr->str);
	{
		Locker<Mutex>	lock(pctx->rmtx);
		--pctx->rwait_count;
		if(pctx->rwait_count == 0){
			pctx->rcnd.signal();
		}
	}
}

template <typename T>
struct MessageSetup{
	std::string str;
	
	MessageSetup(std::string &&_rstr):str(_rstr){}
	MessageSetup(){}
	
	void operator()(frame::ipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<T>(complete_message<T>, _protocol_idx, _message_idx);
	}
};

std::string make_string(const size_t _sz){
	std::string				str;
	static const char 		pattern[] = "`1234567890qwertyuiop[]\\asdfghjkl;'zxcvbnm,./~!@#$%%^*()_+QWERTYUIOP{}|ASDFGHJKL:\"ZXCVBNM<>?";
	static const size_t		pattern_size = sizeof(pattern) - 1;
	
	str.reserve(_sz);
	for(size_t i = 0; i < _sz; ++i){
		str += pattern[i % pattern_size];
	}
	return str;
}

ErrorConditionT start(
	Context &_rctx
){
	ErrorConditionT	err;
	
	pctx = &_rctx;
	
	if(not ipcclient_ptr){//ipc client initialization
		frame::ipc::serialization_v1::Protocol	*proto = new frame::ipc::serialization_v1::Protocol;
		frame::ipc::Configuration				cfg(_rctx.rsched, proto);
		
		alpha_protocol::ProtoSpecT::setup<MessageSetup>(*proto);
		
		cfg.connection_stop_fnc = client_connection_stop;
		cfg.connection_start_outgoing_fnc = client_connection_start;
		
		cfg.connection_start_state = frame::ipc::ConnectionState::Active;
		
		cfg.pool_max_active_connection_count = _rctx.max_per_pool_connection_count;
		
		cfg.name_resolve_fnc = frame::ipc::InternetResolverF(_rctx.rresolver, _rctx.rserver_port.c_str()/*, SocketInfo::Inet4*/);
		
		ipcclient_ptr = std::make_shared<frame::ipc::Service>(_rctx.rm);
		err = ipcclient_ptr->reconfigure(std::move(cfg));
		
		if(err){return err;}
		
		_rctx.rwait_count += 3;
		
		err = ipcclient_ptr->sendMessage(
			"localhost", std::make_shared<alpha_protocol::FirstMessage>(100000, make_string(100000)),
			frame::ipc::Message::WaitResponseFlagE
		);
		if(err){return err;}
		err = ipcclient_ptr->sendMessage(
			"localhost", std::make_shared<alpha_protocol::SecondMessage>(200000, make_string(200000)),
			frame::ipc::Message::WaitResponseFlagE
		);
		if(err){return err;}
		err = ipcclient_ptr->sendMessage(
			"localhost", std::make_shared<alpha_protocol::ThirdMessage>(30000, make_string(30000)),
			frame::ipc::Message::WaitResponseFlagE
		);
		if(err){return err;}
	}
	
	return err;
}


}//namespace