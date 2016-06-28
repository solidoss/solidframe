#ifndef TEST_MULTIPROTOCOL_ALPHA_PROTOCOL_HPP
#define TEST_MULTIPROTOCOL_ALPHA_PROTOCOL_HPP

#include "solid/system/common.hpp"
#include "solid/frame/ipc/ipcmessage.hpp"
#include "solid/frame/ipc/ipccontext.hpp"
#include "solid/frame/ipc/ipcprotocol_serialization_v1.hpp"

namespace alpha_protocol{

struct FirstMessage: solid::frame::ipc::Message{
	uint32_t			v;
	std::string			str;
	
	FirstMessage(){}
	FirstMessage(uint32_t _v, std::string &&_str):v(_v), str(std::move(_str)){}
	
	template <class S>
	void serialize(S &_s, solid::frame::ipc::ConnectionContext &_rctx){
		_s.push(str, "str");
		_s.push(v, "v");
	}	
};

struct SecondMessage: solid::frame::ipc::Message{
	uint64_t			v;
	std::string			str;
	
	SecondMessage(){}
	SecondMessage(uint64_t _v, std::string &&_str):v(_v), str(std::move(_str)){}

	template <class S>
	void serialize(S &_s, solid::frame::ipc::ConnectionContext &_rctx){
		_s.push(str, "str");
		_s.push(v, "v");
	}	
};


struct ThirdMessage: solid::frame::ipc::Message{
	uint16_t			v;
	std::string			str;

	ThirdMessage(){}
	ThirdMessage(uint16_t _v, std::string &&_str):v(_v), str(std::move(_str)){}
	
	template <class S>
	void serialize(S &_s, solid::frame::ipc::ConnectionContext &_rctx){
		_s.push(str, "str");
		_s.push(v, "v");
	}	
};

using ProtoSpecT = solid::frame::ipc::serialization_v1::ProtoSpec<0, FirstMessage, SecondMessage, ThirdMessage>;

}//namespace

#endif
