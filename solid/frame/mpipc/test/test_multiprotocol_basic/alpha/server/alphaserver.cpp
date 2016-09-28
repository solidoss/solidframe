#include "../alphamessages.hpp"
#include "alphaserver.hpp"
#include "solid/serialization/typeidmap.hpp"
#include "solid/system/debug.hpp"
#include "solid/frame/mpipc/mpipcservice.hpp"

using namespace solid;
using namespace std;

namespace alpha_server{

template <class M>
void complete_message(
	frame::mpipc::ConnectionContext &_rctx,
	std::shared_ptr<M> &_rsent_msg_ptr,
	std::shared_ptr<M> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
);

template <>
void complete_message<alpha_protocol::FirstMessage>(
	frame::mpipc::ConnectionContext &_rctx,
	std::shared_ptr<alpha_protocol::FirstMessage> &_rsent_msg_ptr,
	std::shared_ptr<alpha_protocol::FirstMessage> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	idbg("");
	SOLID_CHECK(not _rerror);
	if(_rrecv_msg_ptr){
		SOLID_CHECK(not _rsent_msg_ptr);
		ErrorConditionT err = _rctx.service().sendMessage(_rctx.recipientId(), std::move(_rrecv_msg_ptr));
		
		if(err){
			SOLID_THROW_EX("Connection id should not be invalid!", err.message());
		}
	}
	if(_rsent_msg_ptr){
		SOLID_CHECK(not _rrecv_msg_ptr);
	}
}

template <>
void complete_message<alpha_protocol::SecondMessage>(
	frame::mpipc::ConnectionContext &_rctx,
	std::shared_ptr<alpha_protocol::SecondMessage> &_rsent_msg_ptr,
	std::shared_ptr<alpha_protocol::SecondMessage> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	idbg("");
	SOLID_CHECK(not _rerror);
	if(_rrecv_msg_ptr){
		SOLID_CHECK(not _rsent_msg_ptr);
		ErrorConditionT err = _rctx.service().sendMessage(_rctx.recipientId(), std::move(_rrecv_msg_ptr));
		
		if(err){
			SOLID_THROW_EX("Connection id should not be invalid!", err.message());
		}
	}
	if(_rsent_msg_ptr){
		SOLID_CHECK(not _rrecv_msg_ptr);
	}
}

template <>
void complete_message<alpha_protocol::ThirdMessage>(
	frame::mpipc::ConnectionContext &_rctx,
	std::shared_ptr<alpha_protocol::ThirdMessage> &_rsent_msg_ptr,
	std::shared_ptr<alpha_protocol::ThirdMessage> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	idbg("");
	SOLID_CHECK(not _rerror);
	if(_rrecv_msg_ptr){
		SOLID_CHECK(not _rsent_msg_ptr);
		ErrorConditionT err = _rctx.service().sendMessage(_rctx.recipientId(), std::move(_rrecv_msg_ptr));
		
		if(err){
			SOLID_THROW_EX("Connection id should not be invalid!", err.message());
		}
	}
	if(_rsent_msg_ptr){
		SOLID_CHECK(not _rrecv_msg_ptr);
	}
}

template <typename T>
struct MessageSetup{
	std::string str;
	
	MessageSetup(std::string &&_rstr):str(_rstr){}
	MessageSetup(){}
	
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<T>(complete_message<T>, _protocol_idx, _message_idx);
	}
};


void register_messages(frame::mpipc::serialization_v1::Protocol &_rprotocol){
	alpha_protocol::ProtoSpecT::setup<MessageSetup>(_rprotocol);
	//alpha_protocol::ProtoSpecT::setup<MessageSetup>(_rprotocol, 0, std::string("ceva"));
	
// 	_rprotocol.registerType<alpha_protocol::FirstMessage>(
// 		complete_message<alpha_protocol::FirstMessage>
// 	);
// 	_rprotocol.registerType<alpha_protocol::SecondMessage>(
// 		complete_message<alpha_protocol::SecondMessage>
// 	);
// 	_rprotocol.registerType<alpha_protocol::ThirdMessage>(
// 		complete_message<alpha_protocol::ThirdMessage>
// 	);
}

}//namespace