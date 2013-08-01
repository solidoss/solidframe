// betaservercommands.hpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef BETASERVERCOMMANDS_HPP
#define BETASERVERCOMMANDS_HPP

#include "betaservercommand.hpp"
#include "betarequests.hpp"
#include "betaresponses.hpp"

namespace concept{

namespace beta{

namespace server{

namespace command{

struct Noop: Command{
	/*virtual*/ ~Noop();

	/*virtual*/ uint32 dynamicType() const;

	/*virtual*/ void prepareSerialization(SerializerT &_rser);
	/*virtual*/ void prepareDeserialization(DeserializerT &_rdes);
	
	/*virtual*/ int executeRecv(uint32 _cmdidx);
//	/*virtual*/ int executeSend(uint32 _cmdidx);
	
	request::Noop	request;
	response::Basic	response;
};

struct Login: Command{
	/*virtual*/ ~Login();

	/*virtual*/ uint32 dynamicType() const;

	
	/*virtual*/ void prepareSerialization(SerializerT &_rser);
	/*virtual*/ void prepareDeserialization(DeserializerT &_rdes);

	/*virtual*/ int executeRecv(uint32 _cmdidx);
//	/*virtual*/ int executeSend(uint32 _cmdidx);
	
	request::Login	request;
	response::Basic	response;
};

struct Cancel: Command{
	/*virtual*/ ~Cancel();

	/*virtual*/ uint32 dynamicType() const;

	/*virtual*/ void prepareSerialization(SerializerT &_rser);
	/*virtual*/ void prepareDeserialization(DeserializerT &_rdes);
	
	/*virtual*/ int executeRecv(uint32 _cmdidx);
//	/*virtual*/ int executeSend(uint32 _cmdidx);

	request::Cancel	request;
	response::Basic	response;
};

struct Test: Command{
	/*virtual*/ ~Test();

	/*virtual*/ uint32 dynamicType() const;
	
	/*virtual*/ void prepareSerialization(SerializerT &_rser);
	/*virtual*/ void prepareDeserialization(DeserializerT &_rdes);
	
	/*virtual*/ int executeRecv(uint32 _cmdidx);
	/*virtual*/ int executeSend(uint32 _cmdidx);
	
	request::Test	request;
	response::Test	response;
};


}//namespace command

}//namespace server

}//namespace beta

}//namespace concept

#endif
