/* Implementation file betaclientcommands.cpp
	
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

#include "betaclientcommands.hpp"
#include "system/debug.hpp"
#include "betaclientconnection.hpp"
#include "betarequests.hpp"
#include "betaresponses.hpp"

namespace concept{
namespace beta{
namespace client{
namespace command{
//--------------------------------------------------------------------------
/*virtual*/ Noop::~Noop(){
	
}
/*virtual*/ uint32 Noop::dynamicType() const{
	return request::Noop::Identifier;
}

/*virtual*/ void Noop::prepareSerialization(SerializerT &_rser){
	idbg("noop");
	_rser.push(request, "noop_request");
}
/*virtual*/ void Noop::prepareDeserialization(DeserializerT &_rdes){
	idbg("noop");
	_rdes.push(response, "noop_response");
}
/*virtual*/ int Noop::executeRecv(uint32 _cmdidx){
	idbg("noop response "<<_cmdidx<<": "<<response.error);
	
	return OK;
}
//--------------------------------------------------------------------------
Login::Login(
	const std::string &_user,
	const std::string &_pass
){
	request.user = _user;
	request.password = _pass;
	idbg(""<<(void*)this);
}
/*virtual*/ Login::~Login(){
	idbg(""<<(void*)this);
}
/*virtual*/ uint32 Login::dynamicType() const{
	return request::Login::Identifier;
}

/*virtual*/ void Login::prepareSerialization(SerializerT &_rser){
	idbg("login");
	_rser.push(request, "login_request");
}
/*virtual*/ void Login::prepareDeserialization(DeserializerT &_rdes){
	idbg("login");
	_rdes.push(response, "login_response");
}
/*virtual*/ int Login::executeRecv(uint32 _cmdidx){
	idbg("login response "<<_cmdidx<<": "<<response.error);
	signal->rsw.oss<<"login response "<<_cmdidx<<": "<<response.error;
	LoginSignal *psig = signal.release();
	psig->rsw.signal();
	return OK;
}
//--------------------------------------------------------------------------
Cancel::Cancel(uint32	_tag, uint32 _uid):uid(_uid){
	request.tag = _tag;
}
/*virtual*/ Cancel::~Cancel(){
	
}
/*virtual*/ uint32 Cancel::dynamicType() const{
	return request::Cancel::Identifier;
}
/*virtual*/ void Cancel::prepareSerialization(SerializerT &_rser){
	idbg("cancel");
	_rser.push(request, "cancel_request");
}
/*virtual*/ void Cancel::prepareDeserialization(DeserializerT &_rdes){
	idbg("cancel");
	_rdes.push(response, "cancel_response");
}
/*virtual*/ int Cancel::executeSend(uint32 _cmdidx){
	if(Connection::the().commandUid(request.tag) == uid){
		//the command did not complete
		idbg("cancel");
		return CONTINUE;
	}else{
		idbg("cancel cancel");
		return OK;
	}
}
/*virtual*/ int Cancel::executeRecv(uint32 _cmdidx){
	idbg("cancel response "<<_cmdidx<<": "<<response.error);
	return OK;
}
//--------------------------------------------------------------------------
Test::Test(
	uint32 _count,
	uint32 _timeout,
	const std::string &_token
){
	request.count = _count;
	request.timeout = _timeout;
	request.token = _token;
}
/*virtual*/ Test::~Test(){
	
}
/*virtual*/ uint32 Test::dynamicType() const{
	return request::Test::Identifier;
}
/*virtual*/ void Test::prepareSerialization(SerializerT &_rser){
	idbg("test");
	_rser.push(request, "test_request");
}
/*virtual*/ void Test::prepareDeserialization(DeserializerT &_rdes){
	_rdes.push(response, "test_response");
}
/*virtual*/ int Test::executeSend(uint32 _cmdidx){
	idbg("test response: "<<response.count<<' '<<response.token);
	response.token.clear();
	Connection::the().pushCommand(new Cancel(_cmdidx, Connection::the().commandUid(_cmdidx)));
	return CONTINUE;
}
/*virtual*/ int Test::executeRecv(uint32 _cmdidx){
	idbg("test response "<<_cmdidx<<": "<<response.count<<' '<<response.token);
	response.token.clear();
	return response.count <= 0 ? OK : NOK;
}

//--------------------------------------------------------------------------


}//namespace command

//==========================================================================
Command::Command(){
}
Command::~Command(){
	
}
/*virtual*/ int Command::executeSend(uint32 _cmdidx){
	return CONTINUE;
}
/*virtual*/ int Command::executeRecv(uint32 _cmdidx){
	return OK;
}

}//namespace client
}//namespace beta
}//namespace concept

