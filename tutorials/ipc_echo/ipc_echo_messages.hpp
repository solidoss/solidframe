#ifndef TUTORIAL_IPC_ECHO_MESSAGES_HPP
#define TUTORIAL_IPC_ECHO_MESSAGES_HPP

#include "solid/frame/ipc/ipcmessage.hpp"
#include "solid/frame/ipc/ipccontext.hpp"
#include "solid/frame/ipc/ipcprotocol_serialization_v1.hpp"

namespace ipc_echo{

struct Message: solid::frame::ipc::Message{
	std::string			str;
	
	Message(){}
	
	Message(std::string && _ustr): str(std::move(_ustr)){}
	
	template <class S>
	void serialize(S &_s, solid::frame::ipc::ConnectionContext &_rctx){
		_s.push(str, "str");
	}	
};

using ProtoSpecT = solid::frame::ipc::serialization_v1::ProtoSpec<0, Message>;

}//namespace ipc_echo

#endif
