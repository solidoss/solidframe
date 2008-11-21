/* Implementation file talker.cpp
	
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

#include "clientserver/aio/udp/talker.hpp"
#include "clientserver/aio/udp/multitalker.hpp"
#include "clientserver/aio/src/aiosocket.hpp"

#include "system/socketdevice.hpp"
#include "system/cassert.hpp"

#include <memory>
#include <cstring>

namespace clientserver{

namespace aio{

namespace udp{

//================== Talker ===================================

Talker::Talker(Socket *_psock):
	Object(&stub, 1, &req, &res, &tout), req(-1), res(-1), tout(-1)
{
	stub.psock = _psock;
}
Talker::Talker(const SocketDevice &_rsd):
	Object(&stub, 1, &req, &res, &tout), req(-1), res(-1), tout(-1)
{
	if(_rsd.ok()){
		stub.psock = new Socket(Socket::STATION, _rsd);
	}
}
Talker::~Talker(){
}
bool Talker::socketOk()const{
	cassert(stub.psock);
	return stub.psock->ok();
}
int Talker::socketSend(const char* _pb, uint32 _bl, const SockAddrPair &_sap, uint32 _flags){
	//ensure that we dont have double request
	//cassert(stub.request <= SocketStub::Response);
	cassert(stub.psock);
	int rv = stub.psock->sendTo(_pb, _bl, _sap);
	if(rv == NOK){
		pushRequest(0, SocketStub::IORequest);
	}
	return rv;
}
int Talker::socketRecv(char *_pb, uint32 _bl, uint32 _flags){
	//ensure that we dont have double request
	//cassert(stub.request <= SocketStub::Response);
	cassert(stub.psock);
	int rv = stub.psock->recvFrom(_pb, _bl);
	if(rv == NOK){
		//stub.timepos.set(0xffffffff, 0xffffffff);
		pushRequest(0, SocketStub::IORequest);
	}
	return rv;
}
uint32 Talker::socketRecvSize()const{
	return stub.psock->recvSize();
}
const SockAddrPair &Talker::socketRecvAddr() const{
	return stub.psock->recvAddr();
}
const uint64& Talker::socketSendCount()const{
	return stub.psock->sendCount();
}
const uint64& Talker::socketRecvCount()const{
	return stub.psock->recvCount();
}
bool Talker::socketHasPendingSend()const{
	return stub.psock->isSendPending();
}
bool Talker::socketHasPendingRecv()const{
	return stub.psock->isRecvPending();
}
int Talker::socketLocalAddress(SocketAddress &_rsa)const{
	return stub.psock->localAddress(_rsa);
}
int Talker::socketRemoteAddress(SocketAddress &_rsa)const{
	return stub.psock->remoteAddress(_rsa);
}
void Talker::socketTimeout(const TimeSpec &_crttime, ulong _addsec, ulong _addnsec){
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
uint32 Talker::socketEvents()const{
	return stub.chnevents;
}
void Talker::socketErase(){
	delete stub.psock;
	stub.psock = NULL;
}
int Talker::socketSet(Socket *_psock){
	cassert(!stub.psock);
	stub.psock = _psock;
	return 0;
}
int Talker::socketSet(const SocketDevice &_rsd){
	cassert(!stub.psock);
	if(_rsd.ok()){
		stub.psock = new Socket(Socket::STATION, _rsd);
		return 0;
	}
	return -1;
}
int Talker::socketCreate4(const SockAddrPair &_sap){
	cassert(!stub.psock);
	SocketDevice sd;
	sd.create(AddrInfo::Inet4, AddrInfo::Datagram, 0);
	sd.bind(_sap);
	if(sd.ok()){
		stub.psock = new Socket(Socket::STATION, sd);
		return 0;
	}
	return -1;
}
int Talker::socketCreate6(const SockAddrPair &_sap){
	cassert(!stub.psock);
	SocketDevice sd;
	sd.create(AddrInfo::Inet6, AddrInfo::Datagram, 0);
	sd.bind(_sap);
	if(sd.ok()){
		stub.psock = new Socket(Socket::STATION, sd);
		return 0;
	}
	return -1;
}
int Talker::socketCreate(const AddrInfoIterator&_rai){
	cassert(!stub.psock);
	SocketDevice sd;
	sd.create(_rai);
	sd.bind(_rai);
	if(sd.ok()){
		stub.psock = new Socket(Socket::STATION, sd);
		return 0;
	}
	return -1;
}
void Talker::socketRequestRegister(){
	//ensure that we dont have double request
	cassert(stub.request <= SocketStub::Response);
	stub.request = SocketStub::RegisterRequest;
	*reqpos = 0;
	++reqpos;
}
void Talker::socketRequestUnregister(){
	//ensure that we dont have double request
	cassert(stub.request <= SocketStub::Response);
	stub.request = SocketStub::UnregisterRequest;
	*reqpos = 0;
	++reqpos;
}

int Talker::state()const{
	return stub.state;
}
void Talker::state(int _st){
	stub.state = _st;
}

//================== MultiTalker =============================

MultiTalker::MultiTalker(Socket *_psock):Object(NULL, 0, NULL, NULL, NULL){
	if(_psock)
		socketInsert(_psock);
}
MultiTalker::MultiTalker(const SocketDevice &_rsd):Object(NULL, 0, NULL, NULL, NULL){
	socketInsert(_rsd);
}
MultiTalker::~MultiTalker(){
	for(uint i = 0; i < stubcp; ++i){
		delete pstubs[i].psock;
		pstubs[i].psock = NULL;
	}
	char *oldbuf = reinterpret_cast<char*>(pstubs);
	delete []oldbuf;
}
bool MultiTalker::socketOk(uint _pos)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->ok();
}
const SockAddrPair &MultiTalker::socketRecvAddr(uint _pos) const{
	return pstubs[_pos].psock->recvAddr();
}
int MultiTalker::socketSend(
	uint _pos,
	const char* _pb,
	uint32 _bl,
	const SockAddrPair &_sap,
	uint32 _flags
){
	cassert(_pos < stubcp);
	int rv = pstubs[_pos].psock->sendTo(_pb, _bl, _sap, _flags);
	if(rv == NOK){
		pushRequest(_pos, SocketStub::IORequest);
	}
	return rv;
}
int MultiTalker::socketRecv(
	uint _pos,
	char *_pb,
	uint32 _bl,
	uint32 _flags
){
	cassert(_pos < stubcp);
	int rv = pstubs[_pos].psock->recvFrom(_pb, _bl, _flags);
	if(rv == NOK){
		pushRequest(_pos, SocketStub::IORequest);
	}
	return rv;
}
uint32 MultiTalker::socketRecvSize(uint _pos)const{	
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->recvSize();
}
const uint64& MultiTalker::socketSendCount(uint _pos)const{	
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->sendCount();
}
const uint64& MultiTalker::socketRecvCount(uint _pos)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->recvCount();
}
bool MultiTalker::socketHasPendingSend(uint _pos)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->isSendPending();
}
bool MultiTalker::socketHasPendingRecv(uint _pos)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->isRecvPending();
}
int MultiTalker::socketLocalAddress(uint _pos, SocketAddress &_rsa)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->localAddress(_rsa);
}
int MultiTalker::socketRemoteAddress(uint _pos, SocketAddress &_rsa)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->remoteAddress(_rsa);
}
void MultiTalker::socketTimeout(
	uint _pos,
	const TimeSpec &_crttime,
	ulong _addsec,
	ulong _addnsec
){
	pstubs[_pos].timepos = _crttime;
	pstubs[_pos].timepos.add(_addsec, _addnsec);
	if(pstubs[_pos].toutpos < 0){
		pstubs[_pos].toutpos = toutpos - toutbeg;
		*toutpos = _pos;
		++toutpos;
	}
	if(*ptimepos > pstubs[_pos].timepos){
		*ptimepos = pstubs[_pos].timepos;
	}
}
uint32 MultiTalker::socketEvents(uint _pos)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].chnevents;
}
void MultiTalker::socketErase(uint _pos){
	cassert(_pos < stubcp);
	delete pstubs[_pos].psock;
	pstubs[_pos].reset();
}
int MultiTalker::socketInsert(Socket *_psock){
	cassert(_psock);
	uint pos = newStub();
	pstubs[pos].psock = _psock;
	return pos;
}
int MultiTalker::socketInsert(const SocketDevice &_rsd){
	if(_rsd.ok()){
		uint pos = newStub();
		pstubs[pos].psock = new Socket(Socket::STATION, _rsd);
		return pos;
	}
	return -1;
}
int MultiTalker::socketCreate4(const SockAddrPair &_sap){
	uint pos = newStub();
	SocketDevice sd;
	sd.create(AddrInfo::Inet4, AddrInfo::Datagram, 0);
	sd.bind(_sap);
	if(sd.ok()){
		pstubs[pos].psock = new Socket(Socket::STATION, sd);
		return pos;
	}
	posstk.push(pos);
	return -1;
}
int MultiTalker::socketCreate6(const SockAddrPair &_sap){
	uint pos = newStub();
	SocketDevice sd;
	sd.create(AddrInfo::Inet6, AddrInfo::Datagram, 0);
	sd.bind(_sap);
	if(sd.ok()){
		pstubs[pos].psock = new Socket(Socket::STATION, sd);
		return pos;
	}
	posstk.push(pos);
	return -1;
}
int MultiTalker::socketCreate(const AddrInfoIterator& _rai){
	uint pos = newStub();
	SocketDevice sd;
	sd.create(_rai);
	sd.bind(_rai);
	if(sd.ok()){
		pstubs[pos].psock = new Socket(Socket::STATION, sd);
		return pos;
	}
	posstk.push(pos);
	return -1;
}
void MultiTalker::socketRequestRegister(uint _pos){
	cassert(_pos < stubcp);
	cassert(pstubs[_pos].request <= SocketStub::Response);
	pstubs[_pos].request = SocketStub::RegisterRequest;
	*reqpos = _pos;
	++reqpos;
}
void MultiTalker::socketRequestUnregister(uint _pos){
	cassert(_pos < stubcp);
	cassert(pstubs[_pos].request <= SocketStub::Response);
	pstubs[_pos].request = SocketStub::UnregisterRequest;
	*reqpos = _pos;
	++reqpos;
}
int MultiTalker::socketState(unsigned _pos)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].state;
}
void MultiTalker::socketState(unsigned _pos, int _st){
	cassert(_pos < stubcp);
	pstubs[_pos].state = _st;
}
inline uint MultiTalker::dataSize(uint _cp){
	return _cp * sizeof(SocketStub)//the stubs
			+ _cp * sizeof(int32)//the requests
			+ _cp * sizeof(int32)//the reqponses
			+ _cp * sizeof(int32);//the timeouts
}
void MultiTalker::reserve(uint _cp){
	cassert(_cp > stubcp);
	//first we allocate the space
	char* buf = new char[dataSize(_cp)];
	SocketStub	*pnewstubs(reinterpret_cast<SocketStub*>(buf));
	int32		*pnewreqbeg(reinterpret_cast<int32*>(buf + sizeof(SocketStub) * _cp));
	int32		*pnewresbeg(reinterpret_cast<int32*>(buf + sizeof(SocketStub) * _cp + _cp * sizeof(int32)));
	int32		*pnewtoutbeg(reinterpret_cast<int32*>(buf + sizeof(SocketStub) * _cp + 2 * _cp * sizeof(int32)));
	if(pstubs){
		//then copy all the existing stubs:
		memcpy(pnewstubs, (void*)pstubs, sizeof(SocketStub) * stubcp);
		//then the requests
		memcpy(pnewreqbeg, reqbeg, sizeof(int32) * stubcp);
		//then the responses
		memcpy(pnewresbeg, resbeg, sizeof(int32) * stubcp);
		//then the timeouts
		memcpy(pnewtoutbeg, resbeg, sizeof(int32) * stubcp);
		char *oldbuf = reinterpret_cast<char*>(pstubs);
		delete []oldbuf;
		pstubs = pnewstubs;
	}else{
		reqpos = reqbeg = NULL;
		respos = resbeg = NULL;
		toutpos = toutbeg = NULL;
	}
	for(int i = _cp - 1; i >= (int)stubcp; --i){
		pnewstubs[i].reset();
		posstk.push(i);
	}
	pstubs = pnewstubs;
	stubcp = _cp;
	reqpos = pnewreqbeg + (reqpos - reqbeg);
	reqbeg = pnewreqbeg;
	respos = pnewresbeg + (respos - resbeg);
	resbeg = pnewresbeg;
	toutpos = pnewtoutbeg + (toutpos - toutbeg);
	toutbeg = pnewtoutbeg;
}
uint MultiTalker::newStub(){
	if(posstk.size()){
		uint pos = posstk.top();
		posstk.pop();
		return pos;
	}
	reserve(stubcp + 4);
	uint pos = posstk.top();
	posstk.pop();
	return pos;
}

}//namespace udp

}//namespace aio

}//namespace clientserver
