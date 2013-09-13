#ifndef EXAMPLE_BINARYPROTOCOL_CORE_MESSAGES_HPP
#define EXAMPLE_BINARYPROTOCOL_CORE_MESSAGES_HPP

#include <string>
#include "system/common.hpp"
#include "frame/message.hpp"

struct ConnectionContext;

struct FirstRequest: solid::Dynamic<FirstRequest, solid::frame::Message>{
	FirstRequest(solid::uint32 _v = 0):v(_v){}
	solid::uint32	v;
	template <class S>
	void serialize(S &_s, ConnectionContext &_rctx){
		_s.push(v, "value");
	}
};

struct SecondRequest: solid::Dynamic<SecondRequest, solid::frame::Message>{
	SecondRequest(const std::string &_v = ""):v(_v){}
	std::string	v;
	template <class S>
	void serialize(S &_s, ConnectionContext &_rctx){
		_s.push(v, "value");
	}
};

struct FirstResponse: solid::Dynamic<FirstResponse, solid::frame::Message>{
	FirstResponse(solid::uint32 _v = 0):v(_v){}
	solid::uint32	v;
	template <class S>
	void serialize(S &_s, ConnectionContext &_rctx){
		_s.push(v, "value");
	}
};

struct SecondResponse: solid::Dynamic<SecondResponse, solid::frame::Message>{
	SecondResponse(const std::string &_v = ""):v(_v){}
	std::string	v;
	template <class S>
	void serialize(S &_s, ConnectionContext &_rctx){
		_s.push(v, "value");
	}
};


#endif
