/* Implementation file betaservercommands.cpp
	
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
#include "betaservercommands.hpp"
#include "system/debug.hpp"

namespace concept{
namespace beta{
namespace server{
namespace command{

//--------------------------------------------------------------------------
/*virtual*/ Noop::~Noop(){
	
}
/*virtual*/ uint32 Noop::dynamicType() const{
	return request::Noop::Identifier;
}

/*virtual*/ void Noop::prepareSerialization(SerializerT &_rser){
	idbg("noop");
	_rser.push(response, "noop_response");
}
/*virtual*/ void Noop::prepareDeserialization(DeserializerT &_rdes){
	idbg("noop");
	_rdes.push(request, "noop_request");
}

/*virtual*/ int Noop::executeRecv(uint32 _cmdidx){
	idbg("noop "<<_cmdidx);
	return CONTINUE;
}
//--------------------------------------------------------------------------
/*virtual*/ Login::~Login(){
	
}
/*virtual*/ uint32 Login::dynamicType() const{
	return request::Login::Identifier;
}

/*virtual*/ void Login::prepareSerialization(SerializerT &_rser){
	idbg("login");
	_rser.push(response, "login_response");
}
/*virtual*/ void Login::prepareDeserialization(DeserializerT &_rdes){
	idbg("login");
	_rdes.push(request, "login_request");
}
/*virtual*/ int Login::executeRecv(uint32 _cmdidx){
	idbg("login "<<_cmdidx<<": "<<request.user<<' '<<request.password);
	return CONTINUE;
}
//--------------------------------------------------------------------------
/*virtual*/ Cancel::~Cancel(){
	
}
/*virtual*/ uint32 Cancel::dynamicType() const{
	return request::Cancel::Identifier;
}
/*virtual*/ void Cancel::prepareSerialization(SerializerT &_rser){
	idbg("cancel");
	_rser.push(response, "cancel_response");
}
/*virtual*/ void Cancel::prepareDeserialization(DeserializerT &_rdes){
	idbg("cancel");
	_rdes.push(request, "cancel_request");
}
/*virtual*/ int Cancel::executeRecv(uint32 _cmdidx){
	idbg("cancel "<<_cmdidx<<": "<<request.tag);
	return CONTINUE;
}
//--------------------------------------------------------------------------
/*virtual*/ Test::~Test(){
	
}
/*virtual*/ uint32 Test::dynamicType() const{
	return request::Test::Identifier;
}
/*virtual*/ void Test::prepareSerialization(SerializerT &_rser){
	idbg("test");
	_rser.push(response, "test_response");
}
/*virtual*/ void Test::prepareDeserialization(DeserializerT &_rdes){
	idbg("test");
	_rdes.push(request, "test_request");
}
/*virtual*/ int Test::executeRecv(uint32 _cmdidx){
	idbg("test "<<_cmdidx<<": "<<request.count<<' '<<request.timeout<<' '<<request.token);
	
	response.token = request.token;
	response.count = request.count;
	return CONTINUE;
}
/*virtual*/ int Test::executeSend(uint32 _cmdidx){
	idbg("test "<<_cmdidx<<": "<<request.count<<' '<<request.timeout<<' '<<request.token);
	if(response.count >= 0){
		--response.count;
		return CONTINUE;
	}else{
		return OK;
	}
}
//--------------------------------------------------------------------------


}//namespace command

//==========================================================================
Command::Command(){
}

Command::~Command(){
	
}

/*virtual*/ int Command::executeRecv(uint32){
	return CONTINUE;
}

/*virtual*/ int Command::executeSend(uint32){
	return OK;
}


}//namespace server
}//namespace beta
}//namespace concept
