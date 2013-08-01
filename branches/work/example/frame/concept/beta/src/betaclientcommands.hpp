// betaclientcommands.hpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef BETACLIENTCOMMANDS_HPP
#define BETACLIENTCOMMANDS_HPP

#include "beta/betaservice.hpp"
#include "beta/betamessages.hpp"

#include "betaclientcommand.hpp"
#include "betarequests.hpp"
#include "betaresponses.hpp"


namespace concept{

namespace beta{

namespace client{

namespace command{

struct Noop: Command{
	/*virtual*/ ~Noop();
	
	/*virtual*/ uint32 dynamicType() const;
	
	/*virtual*/ void prepareSerialization(SerializerT &_rser);
	/*virtual*/ void prepareDeserialization(DeserializerT &_rdes);
	
	/*virtual*/ int executeRecv(uint32 _cmdidx);
	
	request::Noop		request;
	response::Basic		response;
};

struct Login: Command{
	Login(const std::string &_user, const std::string &_pass);
	/*virtual*/ ~Login();
	
	/*virtual*/ uint32 dynamicType() const;
	
	/*virtual*/ void prepareSerialization(SerializerT &_rser);
	/*virtual*/ void prepareDeserialization(DeserializerT &_rdes);
	
	/*virtual*/ int executeRecv(uint32 _cmdidx);
	
	request::Login						request;
	response::Basic						response;
	solid::DynamicPointer<LoginMessage>	msgptr;
};

struct Cancel: Command{
	Cancel(uint32	tag, uint32 _uid);
	/*virtual*/ ~Cancel();
	
	/*virtual*/ uint32 dynamicType() const;
	
	/*virtual*/ void prepareSerialization(SerializerT &_rser);
	/*virtual*/ void prepareDeserialization(DeserializerT &_rdes);
	
	/*virtual*/ int executeSend(uint32 _cmdidx);
	/*virtual*/ int executeRecv(uint32 _cmdidx);
	
	request::Cancel							request;
	response::Basic							response;
	uint32									uid;
	solid::DynamicPointer<CancelMessage>	msgptr;
};

struct Test: Command{
	Test(uint32 _count, uint32 _timeout, const std::string &_token);
	/*virtual*/ ~Test();
	
	/*virtual*/ uint32 dynamicType() const;
	
	/*virtual*/ void prepareSerialization(SerializerT &_rser);
	/*virtual*/ void prepareDeserialization(DeserializerT &_rdes);
	
	/*virtual*/ int executeSend(uint32 _cmdidx);
	/*virtual*/ int executeRecv(uint32 _cmdidx);
	
	request::Test	request;
	response::Test	response;
};

}//namespace command

}//namespace client

}//namespace beta

}//namespace concept

#endif
