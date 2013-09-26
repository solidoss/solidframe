#ifndef EXAMPLE_BINARYPROTOCOL_CORE_MESSAGES_HPP
#define EXAMPLE_BINARYPROTOCOL_CORE_MESSAGES_HPP

#include <string>
#include "system/common.hpp"
#include "frame/message.hpp"

struct ConnectionContext;

struct FirstRequest: solid::Dynamic<FirstRequest, solid::frame::Message>{
	FirstRequest(solid::uint32 _idx = 0, solid::uint32 _v = 0):idx(_idx), v(_v){}
	solid::uint32	idx;
	solid::uint64	v;
	template <class S>
	void serialize(S &_s, ConnectionContext &_rctx){
		_s.push(idx, "index").push(v, "value");
	}
};

struct SecondMessage: solid::Dynamic<SecondMessage, solid::frame::Message>{
	SecondMessage(solid::uint32 _idx = 0, const std::string &_v = ""):idx(_idx), v(_v){}
	solid::uint32		idx;
	std::string			v;
	template <class S>
	void serialize(S &_s, ConnectionContext &_rctx){
		_s.push(idx, "index").push(v, "value");
	}
};

struct FirstResponse: solid::Dynamic<FirstResponse, solid::frame::Message>{
	FirstResponse(solid::uint32 _idx = 0, solid::uint32 _v = 0):idx(_idx), v(_v){}
	solid::uint32	idx;
	solid::uint64	v;
	template <class S>
	void serialize(S &_s, ConnectionContext &_rctx){
		_s.push(idx, "index").push(v, "value");
	}
};

struct NoopMessage: solid::Dynamic<NoopMessage, solid::DynamicShared<solid::frame::Message> >{
	NoopMessage(){}
	template <class S>
	void serialize(S &_s, ConnectionContext &_rctx){
	}
};


#endif
