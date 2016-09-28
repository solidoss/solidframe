#ifndef TEST_MULTIPROTOCOL_BETA_PROTOCOL_HPP
#define TEST_MULTIPROTOCOL_BETA_PROTOCOL_HPP

#include "solid/system/common.hpp"
#include "solid/frame/mpipc/mpipcmessage.hpp"
#include "solid/frame/mpipc/mpipccontext.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v1.hpp"

namespace beta_protocol{

struct ThirdMessage: solid::frame::mpipc::Message{
	uint16_t			v;
	std::string			str;
	
	ThirdMessage(){}
	ThirdMessage(uint16_t _v, std::string &&_str):v(_v), str(std::move(_str)){}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(str, "str");
		_s.push(v, "v");
	}	
};


struct FirstMessage: solid::frame::mpipc::Message{
	uint32_t			v;
	std::string			str;
	
	FirstMessage(){}
		
	FirstMessage(ThirdMessage &&_rmsg):solid::frame::mpipc::Message(_rmsg), v(_rmsg.v), str(std::move(_rmsg.str)){}
	
	FirstMessage(uint32_t _v, std::string &&_str):v(_v), str(std::move(_str)){}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(str, "str");
		_s.push(v, "v");
	}	
};

struct SecondMessage: solid::frame::mpipc::Message{
	uint64_t			v;
	std::string			str;
	
	SecondMessage(){}
	SecondMessage(FirstMessage &&_rmsg):solid::frame::mpipc::Message(_rmsg), v(_rmsg.v), str(std::move(_rmsg.str)){}
	SecondMessage(uint64_t _v, std::string &&_str):v(_v), str(std::move(_str)){}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(str, "str");
		_s.push(v, "v");
	}	
};


using ProtoSpecT = solid::frame::mpipc::serialization_v1::ProtoSpec<1, FirstMessage, SecondMessage, ThirdMessage>;

}//namespace

#endif
