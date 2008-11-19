/* Implementation file connection.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "clientserver/aio/tcp/connection.hpp"
#include "clientserver/aio/src/aiosocket.hpp"

#include "system/socketdevice.hpp"
#include "system/cassert.hpp"

namespace clientserver{

namespace aio{

namespace tcp{

Connection::Connection(Socket *_psock):
	Object(&stub, 1, &req, &res, &tout), req(-1), res(-1), tout(-1)
{
	stub.psock = _psock;
}
Connection::Connection(const SocketDevice &_rsd):
	Object(&stub, 1, &req, &res, &tout), req(-1), res(-1), tout(-1)
{
	if(_rsd.ok()){
		stub.psock = new Socket(Socket::CHANNEL, _rsd);
	}
}

bool Connection::socketOk()const{
	cassert(stub.psock);
	return stub.psock->ok();
}
int Connection::socketConnect(const AddrInfoIterator& _rai){
	cassert(stub.psock);
	return stub.psock->connect(_rai);
}
bool Connection::socketIsSecure()const{
	//TODO:
	return false;
}
int Connection::socketSend(const char* _pb, uint32 _bl, uint32 _flags){
	//ensure that we dont have double request
	cassert(stub.request <= SocketStub::Response);
	cassert(stub.psock);
	int rv = stub.psock->send(_pb, _bl);
	if(rv == NOK){
		pushRequest(0, SocketStub::IORequest);
	}
	return rv;
}
int Connection::socketRecv(char *_pb, uint32 _bl, uint32 _flags){
	//ensure that we dont have double request
	cassert(stub.request <= SocketStub::Response);
	cassert(stub.psock);
	int rv = stub.psock->recv(_pb, _bl);
	if(rv == NOK){
		//stub.timepos.set(0xffffffff, 0xffffffff);
		pushRequest(0, SocketStub::IORequest);
	}
	return rv;
}
uint32 Connection::socketRecvSize()const{
	return stub.psock->recvSize();
}
const uint64& Connection::socketSendCount()const{
	return stub.psock->sendCount();
}
const uint64& Connection::socketRecvCount()const{
	return stub.psock->recvCount();
}
bool Connection::socketHasPendingSend()const{
	return stub.psock->isSendPending();
}
bool Connection::socketHasPendingRecv()const{
	return stub.psock->isRecvPending();
}
int Connection::socketLocalAddress(SocketAddress &_rsa)const{
	return stub.psock->localAddress(_rsa);
}
int Connection::socketRemoteAddress(SocketAddress &_rsa)const{
	return stub.psock->remoteAddress(_rsa);
}
void Connection::socketTimeout(const TimeSpec &_crttime, ulong _addsec, ulong _addnsec){
	stub.timepos = _crttime;
	stub.timepos.add(_addsec, _addnsec);
	if(stub.toutpos < 0){
		stub.toutpos = 0;
		*toutpos = 0;
		++toutpos;
	}
	if(*ptimepos > stub.timepos){
		*ptimepos = stub.timepos;
	}	
}
uint32 Connection::socketEvents()const{
	return stub.chnevents;
}
void Connection::socketErase(){
	delete stub.psock;
	stub.psock = NULL;
}
uint Connection::socketSet(Socket *_psock){
	cassert(!stub.psock);
	stub.psock = _psock;
	return 0;
}
uint Connection::socketSet(const SocketDevice &_rsd){
	cassert(!stub.psock);
	if(_rsd.ok()){
		stub.psock = new Socket(Socket::CHANNEL, _rsd);
	}
	return 0;
}
void Connection::socketRequestRegister(){
	//ensure that we dont have double request
	cassert(stub.request <= SocketStub::Response);
	stub.request = SocketStub::RegisterRequest;
	*reqpos = 0;
	++reqpos;
}
void Connection::socketRequestUnregister(){
	//ensure that we dont have double request
	cassert(stub.request <= SocketStub::Response);
	stub.request = SocketStub::UnregisterRequest;
	*reqpos = 0;
	++reqpos;
}

int Connection::state()const{
	return stub.state;
}
void Connection::state(int _st){
	stub.state = _st;
}

}//namespace tcp

}//namespace aio

}//namespace clientserver
