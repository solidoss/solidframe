/* Declarations file betaclientcommands.hpp
	
	Copyright 2012 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef BETACLIENTCOMMANDS_HPP
#define BETACLIENTCOMMANDS_HPP

#include "beta/betaservice.hpp"
#include "beta/betasignals.hpp"

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
	
	request::Login					request;
	response::Basic					response;
	DynamicPointer<LoginSignal>		signal;
};

struct Cancel: Command{
	Cancel(uint32	tag, uint32 _uid);
	/*virtual*/ ~Cancel();
	
	/*virtual*/ uint32 dynamicType() const;
	
	/*virtual*/ void prepareSerialization(SerializerT &_rser);
	/*virtual*/ void prepareDeserialization(DeserializerT &_rdes);
	
	/*virtual*/ int executeSend(uint32 _cmdidx);
	/*virtual*/ int executeRecv(uint32 _cmdidx);
	
	request::Cancel					request;
	response::Basic					response;
	uint32							uid;
	DynamicPointer<CancelSignal>	signal;
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
