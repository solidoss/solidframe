#ifndef EXAMPLE_BINARYPROTOCOL_CORE_MESSAGES_HPP
#define EXAMPLE_BINARYPROTOCOL_CORE_MESSAGES_HPP

#include <string>
#include "system/common.hpp"
#include "frame/message.hpp"

struct ConnectionContext;

struct LoginRequest: solid::Dynamic<LoginRequest, solid::frame::Message>{
	LoginRequest(){}
	LoginRequest(
		const std::string &_user,
		const std::string &_pass
	):user(_user), pass(_pass){}
	std::string		user;
	std::string		pass;
	template <class S>
	void serialize(S &_s, ConnectionContext &_rctx){
		_s.push(pass, "pass").push(user, "user");
	}
};

struct BasicMessage: solid::Dynamic<BasicMessage, solid::frame::Message>{
	enum ErrorCodes{
		ErrorNo = 0,
		ErrorAuth,
		ErrorState
	};
	
	BasicMessage(solid::uint32 _v = 0):v(_v){}
	
	solid::uint32		v;
	
	const char *errorText()const;
	
	template <class S>
	void serialize(S &_s, ConnectionContext &_rctx){
		_s.push(v, "value");
	}
};

struct TextMessageBase{
	TextMessageBase(const std::string &_txt):text(_txt){}
	TextMessageBase(){}
	
	std::string		text;
	std::string		user;
	template <class S>
	void serialize(S &_s, ConnectionContext &_rctx){
		_s.push(text, "text").push(user, "user");
	}
};

struct NoopMessage: solid::Dynamic<NoopMessage, solid::DynamicShared<solid::frame::Message> >{
	NoopMessage(){}
	template <class S>
	void serialize(S &_s, ConnectionContext &_rctx){
	}
};


#endif
