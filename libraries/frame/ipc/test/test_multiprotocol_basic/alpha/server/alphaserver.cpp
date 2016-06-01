#include "../alphamessages.hpp"
#include "alphaserver.hpp"
#include "utility/dynamicpointer.hpp"
#include "serialization/typeidmap.hpp"

using namespace solid;
using namespace std;

namespace alpha_server{

template <class M>
void complete_message(
	frame::ipc::ConnectionContext &_rctx,
	DynamicPointer<M> &_rsent_msg_ptr,
	DynamicPointer<M> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
);

template <>
void complete_message<alpha_protocol::FirstMessage>(
	frame::ipc::ConnectionContext &_rctx,
	DynamicPointer<alpha_protocol::FirstMessage> &_rsent_msg_ptr,
	DynamicPointer<alpha_protocol::FirstMessage> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	
}

template <>
void complete_message<alpha_protocol::SecondMessage>(
	frame::ipc::ConnectionContext &_rctx,
	DynamicPointer<alpha_protocol::SecondMessage> &_rsent_msg_ptr,
	DynamicPointer<alpha_protocol::SecondMessage> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	
}

template <>
void complete_message<alpha_protocol::ThirdMessage>(
	frame::ipc::ConnectionContext &_rctx,
	DynamicPointer<alpha_protocol::ThirdMessage> &_rsent_msg_ptr,
	DynamicPointer<alpha_protocol::ThirdMessage> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	
}


void register_messages(solid::frame::ipc::serialization_v1::Protocol &_rprotocol){
	_rprotocol.registerType<alpha_protocol::FirstMessage>(
		serialization::basic_factory<alpha_protocol::FirstMessage>,
		complete_message<alpha_protocol::FirstMessage>
	);
	_rprotocol.registerType<alpha_protocol::SecondMessage>(
		serialization::basic_factory<alpha_protocol::SecondMessage>,
		complete_message<alpha_protocol::SecondMessage>
	);
	_rprotocol.registerType<alpha_protocol::ThirdMessage>(
		serialization::basic_factory<alpha_protocol::ThirdMessage>,
		complete_message<alpha_protocol::ThirdMessage>
	);
}

}