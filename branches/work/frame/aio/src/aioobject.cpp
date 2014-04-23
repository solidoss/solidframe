// frame/aio/src/aioobject.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "frame/aio/aioobject.hpp"
#include "frame/aio/aiosingleobject.hpp"
#include "frame/aio/aiomultiobject.hpp"
#include "frame/aio/src/aiosocket.hpp"
#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "system/thread.hpp"

#include <memory>
#include <cstring>

namespace solid{
namespace frame{
namespace aio{

//======================== aio::SocketPointer ===========================
void SocketPointer::clear(Socket *_ps)const{
	delete ps;
	ps = _ps;
}

//======================== aio::Object ==================================

Object::SocketStub::~SocketStub(){
	delete psock;
}

/*virtual*/ Object::~Object(){
}

void Object::setSocketPointer(const SocketPointer &_rsp, Socket *_ps){
	_rsp.clear(_ps);
}

Socket* Object::getSocketPointer(const SocketPointer &_rsp){
	return _rsp.release();
}

size_t Object::doOnTimeoutRecv(const TimeSpec &_timepos){
	//add all timeouted stubs to responses
	const size_t	*pend(itoutpos);
	uint 			rv(0);
	
	*pitimepos = TimeSpec::maximum;
	
	for(size_t *pit(itoutbeg); pit != pend;){
		SocketStub &rss(pstubs[*pit]);
		cassert(rss.psock);
		
		vdbgx(Debug::aio, "compare time for pos "<<*pit<<" tout "<<rss.itimepos.seconds()<<" with crttime "<<_timepos.seconds());
		
		if(rss.itimepos <= _timepos){
			rv = EventTimeoutRecv;
			socketPostEvents(*pit, EventTimeoutRecv);
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

size_t Object::doOnTimeoutSend(const TimeSpec &_timepos){
	//add all timeouted stubs to responses
	const size_t	*pend(otoutpos);
	size_t 			rv(0);
	
	*potimepos = TimeSpec::maximum;
	
	for(size_t *pit(otoutbeg); pit != pend;){
		SocketStub &rss(pstubs[*pit]);
		cassert(rss.psock);
		
		vdbgx(Debug::aio, "compare time for pos "<<*pit<<" tout "<<rss.otimepos.seconds()<<" with crttime "<<_timepos.seconds());
		
		if(rss.otimepos <= _timepos){
			rv = EventTimeoutSend;
			socketPostEvents(*pit, EventTimeoutSend);
			--pend;
			*pit = *pend;
			//TODO: add some checking
			//pstubs[*pit].toutpos = pit - toutbeg;
		}else{
			//pitimepos will hold the minimum itimepos for sockets
			if(rss.otimepos < *potimepos){
				*potimepos = rss.otimepos;
			}
			++pit;
		}
	}
	return rv;
}
	
inline void Object::doPopTimeoutRecv(const size_t _pos){
	cassert(itoutpos != itoutbeg);
	vdbgx(Debug::aio, "pop itimeout pos "<<_pos<<" tout "<<pstubs[_pos].itimepos.seconds());
	const size_t tpos = pstubs[_pos].itoutpos;
	--itoutpos;
	itoutbeg[tpos] = *itoutpos;
	pstubs[*itoutpos].itoutpos = tpos;
	pstubs[_pos].itoutpos = -1;
}

inline void Object::doPopTimeoutSend(const size_t _pos){
	cassert(otoutpos != otoutbeg);
	vdbgx(Debug::aio, "pop otimeout pos "<<_pos<<" tout "<<pstubs[_pos].otimepos.seconds());
	const size_t tpos = pstubs[_pos].otoutpos;
	--otoutpos;
	otoutbeg[tpos] = *otoutpos;
	pstubs[*otoutpos].otoutpos = tpos;
	pstubs[_pos].otoutpos = -1;
}
	
void Object::doPushTimeoutRecv(const size_t _pos, const TimeSpec &_crttime, const ulong _addsec, const ulong _addnsec){
	SocketStub &rss(pstubs[_pos]);
	rss.itimepos = _crttime;
	rss.itimepos.add(_addsec, _addnsec);
	vdbgx(Debug::aio, "pos = "<<_pos<<"itimepos = "<<rss.itimepos.seconds()<<" crttime = "<<_crttime.seconds());
	if(rss.itoutpos == static_cast<size_t>(-1)){
		rss.itoutpos = itoutpos - itoutbeg;
		*itoutpos = _pos;
		++itoutpos;
	}
	if(rss.itimepos < *pitimepos){
		*pitimepos = rss.itimepos;
	}
}

void Object::doPushTimeoutSend(const size_t _pos, const TimeSpec &_crttime, const ulong _addsec, const ulong _addnsec){
	SocketStub &rss(pstubs[_pos]);
	rss.otimepos = _crttime;
	rss.otimepos.add(_addsec, _addnsec);
	vdbgx(Debug::aio, "pos = "<<_pos<<"otimepos = "<<rss.otimepos.seconds()<<" crttime = "<<_crttime.seconds());
	if(rss.otoutpos == static_cast<size_t>(-1)){
		rss.otoutpos = otoutpos - otoutbeg;
		*otoutpos = _pos;
		++otoutpos;
	}
	if(rss.otimepos < *potimepos){
		*potimepos = rss.otimepos;
	}
}

void Object::socketPushRequest(const size_t _pos, const uint8 _req){
	//NOTE: only a single request at a time is allowed
	//cassert(!pstubs[_pos].requesttype);
	//you can have 2 io requests on the same time - one for recv and one for send
	cassert(!pstubs[_pos].requesttype || (pstubs[_pos].requesttype == SocketStub::IORequest && _req == SocketStub::IORequest));
	if(pstubs[_pos].requesttype) return;
	pstubs[_pos].requesttype = _req;
	*reqpos = _pos;
	++reqpos;
}

void Object::socketPostEvents(const size_t _pos, const uint32 _evs){
	SocketStub &rss(pstubs[_pos]);
	rss.chnevents |= _evs;
	if(!rss.hasresponse){
		rss.hasresponse = true;
		resbeg[respos] = _pos;
		respos = (respos + 1) % (stubcp);
		++ressize;
	}
	if((_evs & EventDoneRecv) && rss.itoutpos != static_cast<size_t>(-1)){
		doPopTimeoutRecv(_pos);
	}
	if((_evs & EventDoneSend) && rss.otoutpos != static_cast<size_t>(-1)){
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

/*virtual*/void Object::doStop(){
	doUnprepare();
}

void Object::doClearRequests(){
	reqpos = reqbeg;
}

//========================== aio::SingleObject =============================

SingleObject::SingleObject(const SocketPointer& _rsp):
	BaseT(&stub, 1, &req, &res, &itout, &otout), req(-1), res(-1), itout(-1), otout(-1)
{
	//stub.psock = this->getSocketPointer(_psock);
	socketInsert(_rsp);
}

SingleObject::SingleObject(const SocketDevice &_rsd, const bool _isacceptor):
	BaseT(&stub, 1, &req, &res, &itout, &otout), req(-1), res(-1), itout(-1), otout(-1)
{
	if(_rsd.ok()){
		socketInsert(_rsd, _isacceptor);
	}
}

SingleObject::~SingleObject(){
}

bool SingleObject::socketOk()const{
	cassert(stub.psock);
	return stub.psock->ok();
}

AsyncE SingleObject::socketAccept(SocketDevice &_rsd){
	const AsyncE rv = stub.psock->accept(_rsd);
	if(rv == AsyncWait){
		socketPushRequest(0, SocketStub::IORequest);
	}
	return rv;
}

AsyncE SingleObject::socketConnect(const SocketAddressStub& _rsas){
	cassert(stub.psock);
	const AsyncE rv = stub.psock->connect(_rsas);
	if(rv == AsyncWait){
		socketPushRequest(0, SocketStub::IORequest);
	}
	return rv;
}


AsyncE SingleObject::socketSend(const char* _pb, uint32 _bl, uint32 _flags){
	//ensure that we dont have double request
	//cassert(stub.request <= SocketStub::Response);
	cassert(stub.psock);
	const AsyncE rv = stub.psock->send(_pb, _bl);
	if(rv == AsyncWait){
		socketPushRequest(0, SocketStub::IORequest);
	}
	return rv;
}

AsyncE SingleObject::socketSendTo(const char* _pb, uint32 _bl, const SocketAddressStub &_sap, uint32 _flags){
	//ensure that we dont have double request
	//cassert(stub.request <= SocketStub::Response);
	cassert(stub.psock);
	const AsyncE rv = stub.psock->sendTo(_pb, _bl, _sap);
	if(rv == AsyncWait){
		socketPushRequest(0, SocketStub::IORequest);
	}
	return rv;
}

AsyncE SingleObject::socketRecv(char *_pb, uint32 _bl, uint32 _flags){
	//ensure that we dont have double request
	//cassert(stub.request <= SocketStub::Response);
	cassert(stub.psock);
	const AsyncE rv = stub.psock->recv(_pb, _bl);
	if(rv == AsyncWait){
		//stub.timepos.set(0xffffffff, 0xffffffff);
		socketPushRequest(0, SocketStub::IORequest);
	}
	return rv;
}

AsyncE SingleObject::socketRecvFrom(char *_pb, uint32 _bl, uint32 _flags){
	//ensure that we dont have double request
	//cassert(stub.request <= SocketStub::Response);
	cassert(stub.psock);
	const AsyncE rv = stub.psock->recvFrom(_pb, _bl);
	if(rv == AsyncWait){
		//stub.timepos.set(0xffffffff, 0xffffffff);
		socketPushRequest(0, SocketStub::IORequest);
	}
	return rv;
}

uint32 SingleObject::socketRecvSize()const{
	return stub.psock->recvSize();
}

const SocketAddress &SingleObject::socketRecvAddr() const{
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

bool SingleObject::socketLocalAddress(SocketAddress &_rsa)const{
	return stub.psock->localAddress(_rsa);
}

bool SingleObject::socketRemoteAddress(SocketAddress &_rsa)const{
	return stub.psock->remoteAddress(_rsa);
}

void SingleObject::socketTimeoutRecv(const TimeSpec &_crttime, const ulong _addsec, const ulong _addnsec){
	this->doPushTimeoutRecv(0, _crttime, _addsec, _addnsec);
}
void SingleObject::socketTimeoutSend(const TimeSpec &_crttime, const ulong _addsec, const ulong _addnsec){
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

bool SingleObject::socketInsert(const SocketPointer &_rsp){
	cassert(!stub.psock);
	stub.psock = this->getSocketPointer(_rsp);
	return true;
}

bool SingleObject::socketInsert(const SocketDevice &_rsd, const bool _isacceptor){
	cassert(!stub.psock);
	if(_rsd.ok()){
		Socket::Type tp;
		const std::pair<bool, int> socktp = _rsd.type();
		if(!socktp.first) return false;
		
		if(socktp.second == SocketInfo::Datagram){
			tp = Socket::STATION;
		}else if(socktp.second == SocketInfo::Stream){
			if(_isacceptor){
				tp = Socket::ACCEPTOR;
			}else{
				tp = Socket::CHANNEL;
			}
			
		}else return false;
		
		stub.psock = new Socket(tp, _rsd);
		return true;
	}
	return false;
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

AsyncE SingleObject::socketSecureAccept(){
	const AsyncE rv = stub.psock->secureAccept();
	if(rv == AsyncWait){
		//stub.timepos.set(0xffffffff, 0xffffffff);
		socketPushRequest(0, SocketStub::IORequest);
	}
	return rv;
}

AsyncE SingleObject::socketSecureConnect(){
	const AsyncE rv = stub.psock->secureConnect();
	if(rv == AsyncWait){
		//stub.timepos.set(0xffffffff, 0xffffffff);
		socketPushRequest(0, SocketStub::IORequest);
	}
	return rv;
}

//=========================== aio::MultiObject =============================

MultiObject::MultiObject(const SocketPointer& _rps):respoppos(0){
	if(_rps){
		socketInsert(_rps);
	}
}

MultiObject::MultiObject(const SocketDevice &_rsd, const bool _isacceptor):respoppos(0){
	socketInsert(_rsd, _isacceptor);
}

MultiObject::~MultiObject(){
	for(size_t i = 0; i < stubcp; ++i){
		delete pstubs[i].psock;
		pstubs[i].psock = NULL;
	}
	char *oldbuf = reinterpret_cast<char*>(pstubs);
	delete []oldbuf;
}

size_t MultiObject::signaledSize()const{
	return ressize;
}

size_t MultiObject::signaledFront()const{
	return resbeg[respoppos];
}

void MultiObject::signaledPop(){
	cassert(signaledSize());
	const size_t v = resbeg[respoppos];
	//first we clear the events from current "front"
	pstubs[v].chnevents = 0;
	pstubs[v].hasresponse = false;
	//the new front position
	respoppos = (respoppos + 1) % stubcp;
	--ressize;
}

bool MultiObject::socketOk(const size_t _pos)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->ok();
}

AsyncE MultiObject::socketAccept(const size_t _pos, SocketDevice &_rsd){
	cassert(_pos < stubcp);
	const AsyncE rv = pstubs[_pos].psock->accept(_rsd);
	if(rv == AsyncWait){
		socketPushRequest(_pos, SocketStub::IORequest);
	}
	return rv;
}

AsyncE MultiObject::socketConnect(const size_t _pos, const SocketAddressStub& _rsas){
	cassert(_pos < stubcp);
	const AsyncE rv = pstubs[_pos].psock->connect(_rsas);
	if(rv == AsyncWait){
		socketPushRequest(_pos, SocketStub::IORequest);
	}
	return rv;
}

const SocketAddress &MultiObject::socketRecvAddr(const size_t _pos) const{
	return pstubs[_pos].psock->recvAddr();
}

AsyncE MultiObject::socketSend(
	const size_t _pos,
	const char* _pb,
	uint32 _bl,
	uint32 _flags
){
	cassert(_pos < stubcp);
	const AsyncE rv = pstubs[_pos].psock->send(_pb, _bl, _flags);
	if(rv == AsyncWait){
		socketPushRequest(_pos, SocketStub::IORequest);
	}
	return rv;
}

AsyncE MultiObject::socketSendTo(
	const size_t _pos,
	const char* _pb,
	uint32 _bl,
	const SocketAddressStub &_sap,
	uint32 _flags
){
	cassert(_pos < stubcp);
	const AsyncE rv = pstubs[_pos].psock->sendTo(_pb, _bl, _sap, _flags);
	if(rv == AsyncWait){
		socketPushRequest(_pos, SocketStub::IORequest);
	}
	return rv;
}

AsyncE MultiObject::socketRecv(
	const size_t _pos,
	char *_pb,
	uint32 _bl,
	uint32 _flags
){
	cassert(_pos < stubcp);
	const AsyncE rv = pstubs[_pos].psock->recv(_pb, _bl, _flags);
	if(rv == AsyncWait){
		socketPushRequest(_pos, SocketStub::IORequest);
	}
	return rv;
}

AsyncE MultiObject::socketRecvFrom(
	const size_t _pos,
	char *_pb,
	uint32 _bl,
	uint32 _flags
){
	cassert(_pos < stubcp);
	const AsyncE rv = pstubs[_pos].psock->recvFrom(_pb, _bl, _flags);
	if(rv == AsyncWait){
		socketPushRequest(_pos, SocketStub::IORequest);
	}
	return rv;
}

uint32 MultiObject::socketRecvSize(const size_t _pos)const{	
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->recvSize();
}

const uint64& MultiObject::socketSendCount(const size_t _pos)const{	
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->sendCount();
}

const uint64& MultiObject::socketRecvCount(const size_t _pos)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->recvCount();
}

bool MultiObject::socketHasPendingSend(const size_t _pos)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->isSendPending();
}

bool MultiObject::socketHasPendingRecv(const size_t _pos)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->isRecvPending();
}

bool MultiObject::socketLocalAddress(const size_t _pos, SocketAddress &_rsa)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->localAddress(_rsa);
}

bool MultiObject::socketRemoteAddress(const size_t _pos, SocketAddress &_rsa)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].psock->remoteAddress(_rsa);
}

void MultiObject::socketTimeoutRecv(
	const size_t _pos,
	const TimeSpec &_crttime,
	const ulong _addsec,
	const ulong _addnsec
){
	this->doPushTimeoutRecv(_pos, _crttime, _addsec, _addnsec);
}

void MultiObject::socketTimeoutSend(
	const size_t _pos,
	const TimeSpec &_crttime,
	const ulong _addsec,
	const ulong _addnsec
){
	this->doPushTimeoutSend(_pos, _crttime, _addsec, _addnsec);
}

uint32 MultiObject::socketEvents(const size_t _pos)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].chnevents;
}

void MultiObject::socketErase(const size_t _pos){
	cassert(_pos < stubcp);
	delete pstubs[_pos].psock;
	pstubs[_pos].reset();
	posstk.push(_pos);
}

void MultiObject::socketGrab(const size_t _pos, SocketPointer &_rsp){
	cassert(_pos < stubcp);
	this->setSocketPointer(_rsp, pstubs[_pos].psock);
	pstubs[_pos].reset();
	posstk.push(_pos);
}

size_t MultiObject::socketInsert(const SocketPointer &_rsp){
	cassert(_rsp);
	const size_t pos = newStub();
	pstubs[pos].psock = this->getSocketPointer(_rsp);
	return pos;
}

size_t MultiObject::socketInsert(const SocketDevice &_rsd, const bool _isacceptor){
	if(_rsd.ok()){
		const size_t 				pos = newStub();
		Socket::Type				tp;
		const std::pair<bool, int>	socktp = _rsd.type();
		
		if(!socktp.first) return -1;
		
		if(socktp.second == SocketInfo::Datagram){
			tp = Socket::STATION;
		}else if(socktp.second == SocketInfo::Stream){
			if(_isacceptor){
				tp = Socket::ACCEPTOR;
			}else{
				tp = Socket::CHANNEL;
			}
			
		}else return -1;
		
		pstubs[pos].psock = new Socket(tp, _rsd);
		return pos;
	}
	return -1;
}

void MultiObject::socketRequestRegister(const size_t _pos){
	cassert(_pos < stubcp);
	cassert(!pstubs[_pos].requesttype);
	pstubs[_pos].requesttype = SocketStub::RegisterRequest;
	*reqpos = _pos;
	++reqpos;
}

void MultiObject::socketRequestUnregister(const size_t _pos){
	cassert(_pos < stubcp);
	cassert(!pstubs[_pos].requesttype);
	pstubs[_pos].requesttype = SocketStub::UnregisterRequest;
	*reqpos = _pos;
	++reqpos;
}

int MultiObject::socketState(const size_t _pos)const{
	cassert(_pos < stubcp);
	return pstubs[_pos].state;
}

void MultiObject::socketState(const size_t _pos, int _st){
	cassert(_pos < stubcp);
	pstubs[_pos].state = _st;
}

inline size_t MultiObject::dataSize(const size_t _cp){
	return _cp * sizeof(SocketStub)//the stubs
			+ _cp * sizeof(size_t)//the requests
			+ _cp * sizeof(size_t)//the reqponses
			+ _cp * sizeof(size_t)//the itimeouts
			+ _cp * sizeof(size_t)//the otimeouts
			;
}

void MultiObject::reserve(const size_t _cp){
	cassert(_cp > stubcp);
	//first we allocate the space
	char* buf = new char[dataSize(_cp)];
	
	SocketStub	*pnewstubs   (reinterpret_cast<SocketStub*>(buf));
	size_t		*pnewreqbeg  (reinterpret_cast<size_t*>(buf + sizeof(SocketStub) * _cp));
	size_t		*pnewresbeg  (reinterpret_cast<size_t*>(buf + sizeof(SocketStub) * _cp + 1 * _cp * sizeof(size_t)));
	size_t		*pnewitoutbeg(reinterpret_cast<size_t*>(buf + sizeof(SocketStub) * _cp + 2 * _cp * sizeof(size_t)));
	size_t		*pnewotoutbeg(reinterpret_cast<size_t*>(buf + sizeof(SocketStub) * _cp + 3 * _cp * sizeof(size_t)));
	
	if(pstubs){
		//then copy all the existing stubs:
		memcpy(pnewstubs, (void*)pstubs, sizeof(SocketStub) * stubcp);
		//then the requests
		memcpy(pnewreqbeg, reqbeg, sizeof(size_t) * stubcp);
		//then the responses
		memcpy(pnewresbeg, resbeg, sizeof(size_t) * stubcp);
		//then the itimeouts
		memcpy(pnewitoutbeg, itoutbeg, sizeof(size_t) * stubcp);
		//then the otimeouts
		memcpy(pnewotoutbeg, otoutbeg, sizeof(size_t) * stubcp);
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
	
	{
		size_t i = _cp;
		do{
			--i;
			pnewstubs[i].reset();
			posstk.push(i);
		}while(i > stubcp);
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

size_t MultiObject::newStub(){
	if(posstk.size()){
		const size_t pos = posstk.top();
		posstk.pop();
		return pos;
	}
	reserve(stubcp + 4);
	const size_t pos = posstk.top();
	posstk.pop();
	return pos;
}

bool MultiObject::socketIsSecure(const size_t _pos)const{
	return pstubs[_pos].psock->isSecure();
}

SecureSocket* MultiObject::socketSecureSocket(const size_t _pos){
	return pstubs[_pos].psock->secureSocket();
}

void MultiObject::socketSecureSocket(const size_t _pos, SecureSocket *_pss){
	pstubs[_pos].psock->secureSocket(_pss);
}

AsyncE MultiObject::socketSecureAccept(const size_t _pos){
	const AsyncE rv = pstubs[_pos].psock->secureAccept();
	if(rv == AsyncWait){
		socketPushRequest(_pos, SocketStub::IORequest);
	}
	return rv;
}

AsyncE MultiObject::socketSecureConnect(const size_t _pos){
	const AsyncE rv = pstubs[_pos].psock->secureConnect();
	if(rv == AsyncWait){
		socketPushRequest(_pos, SocketStub::IORequest);
	}
	return rv;
}

}//namespace aio;
}//namespace frame
}//namespace solid
