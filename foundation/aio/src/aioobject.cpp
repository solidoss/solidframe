/* Implementation file aioobject.cpp
	
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

#include "foundation/aio/aioobject.hpp"
#include "foundation/aio/aiosingleobject.hpp"
#include "foundation/aio/aiomultiobject.hpp"
#include "foundation/aio/src/aiosocket.hpp"
#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "system/thread.hpp"

#include <memory>
#include <cstring>

namespace foundation{

namespace aio{

namespace {
static const uint crtTimeThreadSpec(){
	static const uint id(Thread::specificId());
	return id;
}
}

//======================== aio::SocketPointer ===========================
void SocketPointer::clear(Socket *_ps)const{
	delete ps;
	ps = _ps;
}

//======================== aio::Object ==================================

/*static*/ const TimeSpec& Object::currentTime(){
	return *reinterpret_cast<const TimeSpec*>(Thread::specific(crtTimeThreadSpec()));
}
/*static*/ void Object::doSetCurrentTime(const TimeSpec *_pcrtts){
	Thread::specific(crtTimeThreadSpec(), const_cast<TimeSpec *>(_pcrtts));
}

Object::SocketStub::~SocketStub(){
	delete psock;
}

/*virtual*/ Object::~Object(){
}

/*virtual*/ int Object::accept(foundation::Visitor &_roi){
	return BAD;
}
void Object::setSocketPointer(const SocketPointer &_rsp, Socket *_ps){
	_rsp.clear(_ps);
}
Socket* Object::getSocketPointer(const SocketPointer &_rsp){
	return _rsp.release();
}
uint Object::doOnTimeoutRecv(const TimeSpec &_timepos){
	//add all timeouted stubs to responses
	const int32	*pend(itoutpos);
	uint 		rv(0);
	
	*pitimepos = TimeSpec::max;
	
	for(int32 *pit(itoutbeg); pit != pend;){
		SocketStub &rss(pstubs[*pit]);
		cassert(rss.psock);
		
		vdbgx(Dbg::aio, "compare time for pos "<<*pit<<" tout "<<rss.itimepos.seconds()<<" with crttime "<<_timepos.seconds());
		
		if(rss.itimepos <= _timepos){
			rv = TIMEOUT_RECV;
			socketPostEvents(*pit, TIMEOUT_RECV);
			--pend;
			*pit = *pend;
			//TODO: add some checking
			//pstubs[*pit].toutpos = pit - toutbeg;
		}else{
			//pitimepos will hold the minimum itimepos for sockets
			if(rss.itimepos < *pitimepos){
				*pitimepos = rss.itimepos;
			}
			++pit;
		}
	}
	return rv;
}
uint Object::doOnTimeoutSend(const TimeSpec &_timepos){
	//add all timeouted stubs to responses
	const int32	*pend(otoutpos);
	uint 		rv(0);
	
	*potimepos = TimeSpec::max;
	
	for(int32 *pit(otoutbeg); pit != pend;){
		SocketStub &rss(pstubs[*pit]);
		cassert(rss.psock);
		
		vdbgx(Dbg::aio, "compare time for pos "<<*pit<<" tout "<<rss.otimepos.seconds()<<" with crttime "<<_timepos.seconds());
		
		if(rss.otimepos <= _timepos){
			rv = TIMEOUT_SEND;
			socketPostEvents(*pit, TIMEOUT_SEND);
			--pend;
			*pit = *pend;
			//TODO: add some checking
			//pstubs[*pit].toutpos = pit - toutbeg;
		}else{
			//pitimepos will hold the minimum itimepos for sockets
			if(rss.otimepos < *potimepos){
				*potimepos = rss.itimepos;
			}
			++pit;
		}
	}
	return rv;
}
	
inline void Object::doPopTimeoutRecv(uint32 _pos){
	cassert(itoutpos != itoutbeg);
	vdbgx(Dbg::aio, "pop itimeout pos "<<_pos<<" tout "<<pstubs[_pos].itimepos.seconds());
	uint tpos = pstubs[_pos].itoutpos;
	--itoutpos;
	itoutbeg[tpos] = *itoutpos;
	pstubs[*itoutpos].itoutpos = tpos;
	pstubs[_pos].itoutpos = -1;
}
inline void Object::doPopTimeoutSend(uint32 _pos){
	cassert(otoutpos != otoutbeg);
	vdbgx(Dbg::aio, "pop otimeout pos "<<_pos<<" tout "<<pstubs[_pos].otimepos.seconds());
	uint tpos = pstubs[_pos].otoutpos;
	--otoutpos;
	otoutbeg[tpos] = *otoutpos;
	pstubs[*otoutpos].otoutpos = tpos;
	pstubs[_pos].otoutpos = -1;
}
	
void Object::doPushTimeoutRecv(uint32 _pos, const TimeSpec &_crttime, ulong _addsec, ulong _addnsec){
	SocketStub &rss(pstubs[_pos]);
	rss.itimepos = _crttime;
	rss.itimepos.add(_addsec, _addnsec);
	vdbgx(Dbg::aio, "pos = "<<_pos<<"itimepos = "<<rss.itimepos.seconds()<<" crttime = "<<_crttime.seconds());
	if(rss.itoutpos < 0){
		rss.itoutpos = itoutpos - itoutbeg;
		*itoutpos = _pos;
		++itoutpos;
	}
	if(rss.itimepos < *pitimepos){
		*pitimepos = rss.itimepos;
	}
}
void Object::doPushTimeoutSend(uint32 _pos, const TimeSpec &_crttime, ulong _addsec, ulong _addnsec){
	SocketStub &rss(pstubs[_pos]);
	rss.otimepos = _crttime;
	rss.otimepos.add(_addsec, _addnsec);
	vdbgx(Dbg::aio, "pos = "<<_pos<<"otimepos = "<<rss.otimepos.seconds()<<" crttime = "<<_crttime.seconds());
	if(rss.otoutpos < 0){
		rss.otoutpos = otoutpos - otoutbeg;
		*otoutpos = _pos;
		++otoutpos;
	}
	if(rss.otimepos < *potimepos){
		*potimepos = rss.otimepos;
	}
}

void Object::socketPushRequest(const uint _pos, const uint8 _req){
	//NOTE: only a single request at a time is allowed
	cassert(!pstubs[_pos].requesttype);
	if(pstubs[_pos].requesttype) return;
	pstubs[_pos].requesttype = _req;
	*reqpos = _pos;
	++reqpos;
}

void Object::socketPostEvents(const uint _pos, const uint32 _evs){
	SocketStub &rss(pstubs[_pos]);
	rss.chnevents |= _evs;
	if(!rss.hasresponse){
		rss.hasresponse = true;
		resbeg[respos] = _pos;
		respos = (respos + 1) % (stubcp);
		++ressize;
	}
	if((_evs & INDONE) && rss.itoutpos >= 0){
		doPopTimeoutRecv(_pos);
	}
	if((_evs & OUTDONE) && rss.otoutpos >= 0){
		doPopTimeoutSend(_pos);
	}
}

void Object::doPrepare(TimeSpec *_pitimepos, TimeSpec *_potimepos){
	pitimepos = _pitimepos;
	potimepos = _potimepos;
}
void Object::doUnprepare(){
	pitimepos = NULL;
	potimepos = NULL;
}

void Object::doClearRequests(){
	reqpos = reqbeg;
}

//========================== aio::SingleObject =============================

SingleObject::SingleObject(const SocketPointer& _rsp):
	Object(&stub, 1, &req, &res, &itout, &otout), req(-1), res(-1), itout(-1), otout(-1)
{
	//stub.psock = this->getSocketPointer(_psock);
	socketInsert(_rsp);
}
SingleObject::SingleObject(const SocketDevice &_rsd):
	Object(&stub, 1, &req, &res, &itout, &otout), req(-1), res(-1), itout(-1), otout(-1)
{
	if(_rsd.ok()){
		socketInsert(_rsd);
	}
}
SingleObject::~SingleObject(){
}
bool SingleObject::socketOk()const{
	cassert(stub.psock);
	return stub.psock->ok();
}

int SingleObject::socketAccept(SocketDevice &_rsd){
	int rv = stub.psock->accept(_rsd);
	if(rv == NOK){
		socketPushRequest(0, SocketStub::IORequest);
	}
	return rv;
}

int SingleObject::socketConnect(const AddrInfoIterator& _rai){
	cassert(stub.psock);
	int rv = stub.psock->connect(_rai);
	if(rv == NOK){
		socketPushRequest(0, SocketStub::IORequest);
	}
	return rv;
}


int SingleObject::socketSend(const char* _pb, uint32 _bl, uint32 _flags){
	//ensure that we dont have double request
	//cassert(stub.request <= SocketStub::Response);
	cassert(stub.psock);
	int rv = stub.psock->send(_pb, _bl);
	if(rv == NOK){
		socketPushRequest(0, SocketStub::IORequest);
	}
	return rv;
}

int SingleObject::socketSendTo(const char* _pb, uint32 _bl, const SockAddrPair &_sap, uint32 _flags){
	//ensure that we dont have double request
	//cassert(stub.request <= SocketStub::Response);
	cassert(stub.psock);
	int rv = stub.psock->sendTo(_pb, _bl, _sap);
	if(rv == NOK){
		socketPushRequest(0, SocketStub::IORequest);
	}
	return rv;
}

int SingleObject::socketRecv(char *_pb, uint32 _bl, uint32 _flags){
	//ensure that we dont have double request
	//cassert(stub.request <= SocketStub::Response);
	cassert(stub.psock);
	int rv = stub.psock->recv(_pb, _bl);
	if(rv == NOK){
		//stub.timepos.set(0xffffffff, 0xffffffff);
		socketPushRequest(0, SocketStub::IORequest);
	}
	return rv;
}

int SingleObject::socketRecvFrom(char *_pb, uint32 _bl, uint32 _flags){
	//ensure that we dont have double request
	//cassert(stub.request <= SocketStub::Response);
	cassert(stub.psock);
	int rv = stub.psock->recvFrom(_pb, _bl);
	if(rv == NOK){
		//stub.timepos.set(0xffffffff, 0xffffffff);
		socketPushRequest(0, SocketStub::IORequest);
	}
	return rv;
}
uint32 SingleObject::socketRecvSize()const{
	return stub.psock->recvSize();
}
const SockAddrPair &SingleObject::socketRecvAddr() const{
	return stub.psock->recvAddr();
}
const uint64& SingleObject::socketSendCount()const{
	return stub.psock->sendCount();
}
const uint64& SingleObject::socketRecvCount()const{
	return stub.psock->recvCount();
}
bool SingleObject::socketHasPendingSend()const{
	return stub.psock->isSendPending();
}
bool SingleObject::socketHasPendingRecv()const{
	return stub.psock->isRecvPending();
}
int SingleObject::socketLocalAddress(SocketAddress &_rsa)const{
	return stub.psock->localAddress(_rsa);
}
int SingleObject::socketRemoteAddress(SocketAddress &_rsa)const{
	return stub.psock->remoteAddress(_rsa);
}

void SingleObject::socketTimeoutRecv(const TimeSpec &_crttime, ulong _addsec, ulong _addnsec){
	this->doPushTimeoutRecv(0, _crttime, _addsec, _addnsec);
}
void SingleObject::socketTimeoutSend(const TimeSpec &_crttime, ulong _addsec, ulong _addnsec){
	this->doPushTimeoutSend(0, _crttime, _addsec, _addnsec);
}

// uint32 SingleObject::socketEvents()const{
// 	return stub.chnevents;
// }

uint32 SingleObject::socketEventsGrab(){
	uint32 evs = stub.chnevents;
	stub.chnevents = 0;
	stub.hasresponse = false;
	return evs;
}

void SingleObject::socketErase(){
	delete stub.psock;
	stub.psock = NULL;
}

void SingleObject::socketGrab(SocketPointer &_rsp){
	this->setSocketPointer(_rsp, stub.psock);
	stub.reset();
}

int SingleObject::socketInsert(const SocketPointer &_rsp){
	cassert(!stub.psock);
	stub.psock = this->getSocketPointer(_rsp);
	return 0;
}
int SingleObject::socketInsert(const SocketDevice &_rsd){
	cassert(!stub.psock);
	if(_rsd.ok()){
		Socket::Type tp;
		if(_rsd.isListening()){
			tp = Socket::ACCEPTOR;
		}else if(_rsd.type() == AddrInfo::Stream){
			tp = Socket::CHANNEL;
		}else if(_rsd.type() == AddrInfo::Datagram){
			tp = Socket::STATION;
		}else return -1;
		stub.psock = new Socket(tp, _rsd);
		return 0;
	}
	return -1;
}

void SingleObject::socketRequestRegister(){
	//ensure that we dont have double request
	cassert(!stub.requesttype);
	stub.requesttype = SocketStub::RegisterRequest;
	*reqpos = 0;
	++reqpos;
}
void SingleObject::socketRequestUnregister(){
	//ensure that we dont have double request
	cassert(!stub.requesttype);
	stub.requesttype = SocketStub::UnregisterRequest;
	*reqpos = 0;
	++reqpos;
}

int SingleObject::socketState()const{
	return stub.state;
}
void SingleObject::socketState(int _st){
	stub.state = _st;
}

bool SingleObject::socketIsSecure()const{
	return stub.psock->isSecure();
}

SecureSocket* SingleObject::socketSecureSocket(){
	return stub.psock->secureSocket();
}
void SingleObject::socketSecureSocket(SecureSocket *_pss){
	stub.psock->secureSocket(_pss);
}

int SingleObject::socketSecureAccept(){
	int rv = stub.psock->secureAccept();
	if(rv == NOK){
		//stub.timepos.set(0xffffffff, 0xffffffff);
		socketPushRequest(0, SocketStub::IORequest);
	}
	return rv;
}
int SingleObject::socketSecureConnect(){
	int rv = stub.psock->secureConnect();
	if(rv == NOK){
		//stub.timepos.set(0xffffffff, 0xffffffff);
		socketPushRequest(0, SocketStub::IORequest);
	}
	return rv;
}

//=========================== aio::MultiObject =============================

MultiObject::MultiObject(const SocketPointer& _rps):Object(NULL, 0, NULL, NULL, NULL, NULL), respoppos(0){
	if(_rps)
		socketInsert(_rps);
}
MultiObject::MultiObject(const SocketDevice &_rsd):Object(NULL, 0, NULL, NULL, NULL, NULL), respoppos(0){
	socketInsert(_rsd);
}
MultiObject::~MultiObject(){
	for(uint i = 0; i < stubcp; ++i){
		delete pstubs[i].psock;
		pstubs[i].psock = NULL;
	}
	char *oldbuf = reinterpret_cast<char*>(pstubs);
	delete []oldbuf;
}

uint MultiObject::signaledSize()const{
	return ressize;
}

uint MultiObject::signaledFront()const{
	return resbeg[respoppos];
}

void MultiObject::signaledPop(){
	cassert(signaledSize());
	uint v = resbeg[respoppos];
	//first we clear the events from current "front"
	pstubs[v].chnevents = 0;
	pstubs[v].hasresponse = false;
	//the new front position
	respoppos = (respoppos + 1) % stubcp;
	--ressize;
}

bool MultiObject::socketOk(const uint _pos)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->ok();
}

int MultiObject::socketAccept(const uint _pos, SocketDevice &_rsd){
	cassert(_pos < stubcp);
	int rv = pstubs[_pos].psock->accept(_rsd);
	if(rv == NOK){
		socketPushRequest(0, SocketStub::IORequest);
	}
	return rv;
}

int MultiObject::socketConnect(const uint _pos, const AddrInfoIterator& _rai){
	cassert(_pos < stubcp);
	int rv = pstubs[_pos].psock->connect(_rai);
	if(rv == NOK){
		socketPushRequest(_pos, SocketStub::IORequest);
	}
	return rv;
}

const SockAddrPair &MultiObject::socketRecvAddr(uint _pos) const{
	return pstubs[_pos].psock->recvAddr();
}

int MultiObject::socketSend(
	const uint _pos,
	const char* _pb,
	uint32 _bl,
	uint32 _flags
){
	cassert(_pos < stubcp);
	int rv = pstubs[_pos].psock->send(_pb, _bl, _flags);
	if(rv == NOK){
		socketPushRequest(_pos, SocketStub::IORequest);
	}
	return rv;
}

int MultiObject::socketSendTo(
	const uint _pos,
	const char* _pb,
	uint32 _bl,
	const SockAddrPair &_sap,
	uint32 _flags
){
	cassert(_pos < stubcp);
	int rv = pstubs[_pos].psock->sendTo(_pb, _bl, _sap, _flags);
	if(rv == NOK){
		socketPushRequest(_pos, SocketStub::IORequest);
	}
	return rv;
}

int MultiObject::socketRecv(
	const uint _pos,
	char *_pb,
	uint32 _bl,
	uint32 _flags
){
	cassert(_pos < stubcp);
	int rv = pstubs[_pos].psock->recv(_pb, _bl, _flags);
	if(rv == NOK){
		socketPushRequest(_pos, SocketStub::IORequest);
	}
	return rv;
}
int MultiObject::socketRecvFrom(
	const uint _pos,
	char *_pb,
	uint32 _bl,
	uint32 _flags
){
	cassert(_pos < stubcp);
	int rv = pstubs[_pos].psock->recvFrom(_pb, _bl, _flags);
	if(rv == NOK){
		socketPushRequest(_pos, SocketStub::IORequest);
	}
	return rv;
}
uint32 MultiObject::socketRecvSize(const uint _pos)const{	
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->recvSize();
}
const uint64& MultiObject::socketSendCount(const uint _pos)const{	
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->sendCount();
}
const uint64& MultiObject::socketRecvCount(const uint _pos)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->recvCount();
}
bool MultiObject::socketHasPendingSend(const uint _pos)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->isSendPending();
}
bool MultiObject::socketHasPendingRecv(const uint _pos)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->isRecvPending();
}
int MultiObject::socketLocalAddress(const uint _pos, SocketAddress &_rsa)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->localAddress(_rsa);
}
int MultiObject::socketRemoteAddress(const uint _pos, SocketAddress &_rsa)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->remoteAddress(_rsa);
}

void MultiObject::socketTimeoutRecv(
	const uint _pos,
	const TimeSpec &_crttime,
	ulong _addsec,
	ulong _addnsec
){
	this->doPushTimeoutRecv(_pos, _crttime, _addsec, _addnsec);
}

void MultiObject::socketTimeoutSend(
	const uint _pos,
	const TimeSpec &_crttime,
	ulong _addsec,
	ulong _addnsec
){
	this->doPushTimeoutSend(_pos, _crttime, _addsec, _addnsec);
}
uint32 MultiObject::socketEvents(const uint _pos)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].chnevents;
}
void MultiObject::socketErase(const uint _pos){
	cassert(_pos < stubcp);
	delete pstubs[_pos].psock;
	pstubs[_pos].reset();
	posstk.push(_pos);
}
void MultiObject::socketGrab(const uint _pos, SocketPointer &_rsp){
	cassert(_pos < stubcp);
	this->setSocketPointer(_rsp, pstubs[_pos].psock);
	pstubs[_pos].reset();
	posstk.push(_pos);
}
int MultiObject::socketInsert(const SocketPointer &_rsp){
	cassert(_rsp);
	uint pos = newStub();
	pstubs[pos].psock = this->getSocketPointer(_rsp);
	return pos;
}
int MultiObject::socketInsert(const SocketDevice &_rsd){
	if(_rsd.ok()){
		uint pos = newStub();
		Socket::Type tp;
		if(_rsd.isListening()){
			tp = Socket::ACCEPTOR;
		}else if(_rsd.type() == AddrInfo::Stream){
			tp = Socket::CHANNEL;
		}else if(_rsd.type() == AddrInfo::Datagram){
			tp = Socket::STATION;
		}else return -1;
		
		pstubs[pos].psock = new Socket(tp, _rsd);
		return pos;
	}
	return -1;
}
void MultiObject::socketRequestRegister(const uint _pos){
	cassert(_pos < stubcp);
	cassert(!pstubs[_pos].requesttype);
	pstubs[_pos].requesttype = SocketStub::RegisterRequest;
	*reqpos = _pos;
	++reqpos;
}
void MultiObject::socketRequestUnregister(const uint _pos){
	cassert(_pos < stubcp);
	cassert(!pstubs[_pos].requesttype);
	pstubs[_pos].requesttype = SocketStub::UnregisterRequest;
	*reqpos = _pos;
	++reqpos;
}
int MultiObject::socketState(const uint _pos)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].state;
}
void MultiObject::socketState(const uint _pos, int _st){
	cassert(_pos < stubcp);
	pstubs[_pos].state = _st;
}
inline uint MultiObject::dataSize(const uint _cp){
	return _cp * sizeof(SocketStub)//the stubs
			+ _cp * sizeof(int32)//the requests
			+ _cp * sizeof(int32)//the reqponses
			+ _cp * sizeof(int32)//the itimeouts
			+ _cp * sizeof(int32)//the otimeouts
			;
}
void MultiObject::reserve(const uint _cp){
	cassert(_cp > stubcp);
	//first we allocate the space
	char* buf = new char[dataSize(_cp)];
	
	SocketStub	*pnewstubs   (reinterpret_cast<SocketStub*>(buf));
	int32		*pnewreqbeg  (reinterpret_cast<int32*>(buf + sizeof(SocketStub) * _cp));
	int32		*pnewresbeg  (reinterpret_cast<int32*>(buf + sizeof(SocketStub) * _cp + 1 * _cp * sizeof(int32)));
	int32		*pnewitoutbeg(reinterpret_cast<int32*>(buf + sizeof(SocketStub) * _cp + 2 * _cp * sizeof(int32)));
	int32		*pnewotoutbeg(reinterpret_cast<int32*>(buf + sizeof(SocketStub) * _cp + 3 * _cp * sizeof(int32)));
	if(pstubs){
		//then copy all the existing stubs:
		memcpy(pnewstubs, (void*)pstubs, sizeof(SocketStub) * stubcp);
		//then the requests
		memcpy(pnewreqbeg, reqbeg, sizeof(int32) * stubcp);
		//then the responses
		memcpy(pnewresbeg, resbeg, sizeof(int32) * stubcp);
		//then the itimeouts
		memcpy(pnewitoutbeg, itoutbeg, sizeof(int32) * stubcp);
		//then the otimeouts
		memcpy(pnewotoutbeg, otoutbeg, sizeof(int32) * stubcp);
		char *oldbuf = reinterpret_cast<char*>(pstubs);
		delete []oldbuf;
		pstubs = pnewstubs;
	}else{
		reqpos = reqbeg = NULL;
		resbeg = NULL;
		respos = 0;
		ressize = 0;
		itoutpos = itoutbeg = NULL;
		otoutpos = otoutbeg = NULL;
	}
	
	for(int i = _cp - 1; i >= (int)stubcp; --i){
		pnewstubs[i].reset();
		posstk.push(i);
	}
	
	pstubs = pnewstubs;
	stubcp = _cp;
	reqpos = pnewreqbeg + (reqpos - reqbeg);
	reqbeg = pnewreqbeg;
	//respos = pnewresbeg + (respos - resbeg);
	resbeg = pnewresbeg;
	itoutpos = pnewitoutbeg + (itoutpos - itoutbeg);
	itoutbeg = pnewitoutbeg;
	otoutpos = pnewotoutbeg + (otoutpos - otoutbeg);
	otoutbeg = pnewotoutbeg;
}
uint MultiObject::newStub(){
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

bool MultiObject::socketIsSecure(const uint _pos)const{
	return pstubs[_pos].psock->isSecure();
}

SecureSocket* MultiObject::socketSecureSocket(const uint _pos){
	return pstubs[_pos].psock->secureSocket();
}
void MultiObject::socketSecureSocket(const uint _pos, SecureSocket *_pss){
	pstubs[_pos].psock->secureSocket(_pss);
}

int MultiObject::socketSecureAccept(const uint _pos){
	int rv = pstubs[_pos].psock->secureAccept();
	if(rv == NOK){
		socketPushRequest(_pos, SocketStub::IORequest);
	}
	return rv;
}
int MultiObject::socketSecureConnect(const uint _pos){
	int rv = pstubs[_pos].psock->secureConnect();
	if(rv == NOK){
		socketPushRequest(_pos, SocketStub::IORequest);
	}
	return rv;
}

}//namespace aio;
}//namespace foundation
