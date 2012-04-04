/* Declarations file betaservercommands.hpp
	
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
