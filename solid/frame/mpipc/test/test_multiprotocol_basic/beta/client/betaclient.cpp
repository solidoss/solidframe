#include "../betamessages.hpp"
#include "betaclient.hpp"

using namespace std;
using namespace solid;

namespace beta_client {

using IpcServicePointerT = shared_ptr<frame::mpipc::ServiceT>;
IpcServicePointerT		mpipcclient_ptr;

Context					*pctx;

void client_connection_stop(frame::mpipc::ConnectionContext &_rctx){
	idbg(_rctx.recipientId()<<" error: "<<_rctx.error().message());
}

void client_connection_start(frame::mpipc::ConnectionContext &_rctx){
	idbg(_rctx.recipientId());
}


void complete_message_first(
	frame::mpipc::ConnectionContext &_rctx,
	std::shared_ptr<beta_protocol::FirstMessage> &_rsent_msg_ptr,
	std::shared_ptr<beta_protocol::SecondMessage> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	idbg("");
	SOLID_CHECK(not _rerror);
	SOLID_CHECK(_rsent_msg_ptr and _rrecv_msg_ptr);
	
	SOLID_CHECK(_rsent_msg_ptr->v == _rrecv_msg_ptr->v);
	SOLID_CHECK(_rsent_msg_ptr->str == _rrecv_msg_ptr->str);
	{
		unique_lock<mutex>	lock(pctx->rmtx);
		--pctx->rwait_count;
		if(pctx->rwait_count == 0){
			pctx->rcnd.notify_one();
		}
	}
}

void complete_message_second(
	frame::mpipc::ConnectionContext &_rctx,
	std::shared_ptr<beta_protocol::SecondMessage> &_rsent_msg_ptr,
	std::shared_ptr<beta_protocol::SecondMessage> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	idbg("");
	SOLID_CHECK(not _rerror);
	SOLID_CHECK(_rsent_msg_ptr and _rrecv_msg_ptr);
	
	SOLID_CHECK(_rsent_msg_ptr->v == _rrecv_msg_ptr->v);
	SOLID_CHECK(_rsent_msg_ptr->str == _rrecv_msg_ptr->str);
	{
		unique_lock<mutex>	lock(pctx->rmtx);
		--pctx->rwait_count;
		if(pctx->rwait_count == 0){
			pctx->rcnd.notify_one();
		}
	}
}

void complete_message_third(
	frame::mpipc::ConnectionContext &_rctx,
	std::shared_ptr<beta_protocol::ThirdMessage> &_rsent_msg_ptr,
	std::shared_ptr<beta_protocol::FirstMessage> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	idbg("");
	SOLID_CHECK(not _rerror);
	SOLID_CHECK(_rsent_msg_ptr and _rrecv_msg_ptr);
	
	SOLID_CHECK(_rsent_msg_ptr->v == _rrecv_msg_ptr->v);
	SOLID_CHECK(_rsent_msg_ptr->str == _rrecv_msg_ptr->str);
	{
		unique_lock<mutex>	lock(pctx->rmtx);
		--pctx->rwait_count;
		if(pctx->rwait_count == 0){
			pctx->rcnd.notify_one();
		}
	}
}

template <typename T>
struct MessageSetup;


template <>
struct MessageSetup<beta_protocol::FirstMessage>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<beta_protocol::FirstMessage>(complete_message_first, _protocol_idx, _message_idx);
	}
};

template <>
struct MessageSetup<beta_protocol::SecondMessage>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<beta_protocol::SecondMessage>(complete_message_second, _protocol_idx, _message_idx);
	}
};

template <>
struct MessageSetup<beta_protocol::ThirdMessage>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<beta_protocol::ThirdMessage>(complete_message_third, _protocol_idx, _message_idx);
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
	
	if(not mpipcclient_ptr){//mpipc client initialization
		auto						proto = frame::mpipc::serialization_v1::Protocol::create();
		frame::mpipc::Configuration	cfg(_rctx.rsched, proto);
		
		beta_protocol::ProtoSpecT::setup<MessageSetup>(*proto);
		
		cfg.connection_stop_fnc = client_connection_stop;
		cfg.connection_start_outgoing_fnc = client_connection_start;
		
		cfg.connection_start_state = frame::mpipc::ConnectionState::Active;
		
		cfg.pool_max_active_connection_count = _rctx.max_per_pool_connection_count;
		
		cfg.name_resolve_fnc = frame::mpipc::InternetResolverF(_rctx.rresolver, _rctx.rserver_port.c_str()/*, SocketInfo::Inet4*/);
		
		mpipcclient_ptr = std::make_shared<frame::mpipc::ServiceT>(_rctx.rm);
		err = mpipcclient_ptr->reconfigure(std::move(cfg));
		
		if(err){return err;}
		
		_rctx.rwait_count += 3;
		
		err = mpipcclient_ptr->sendMessage(
			"localhost", std::make_shared<beta_protocol::FirstMessage>(100000, make_string(100000)),
			0|frame::mpipc::MessageFlags::WaitResponse
		);
		if(err){return err;}
		err = mpipcclient_ptr->sendMessage(
			"localhost", std::make_shared<beta_protocol::SecondMessage>(200000, make_string(200000)),
			0|frame::mpipc::MessageFlags::WaitResponse
		);
		if(err){return err;}
		err = mpipcclient_ptr->sendMessage(
			"localhost", std::make_shared<beta_protocol::ThirdMessage>(30000, make_string(30000)),
			0|frame::mpipc::MessageFlags::WaitResponse
		);
		if(err){return err;}
	}
	
	return err;
}

void stop(){
	mpipcclient_ptr.reset();
}

}//namespace
