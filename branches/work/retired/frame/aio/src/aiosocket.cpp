// frame/aio/src/aiosocket.cpp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "frame/common.hpp"
#include "frame/aio/src/aiosocket.hpp"
#include "frame/aio/aiosecuresocket.hpp"
#include "system/socketaddress.hpp"
#include "system/specific.hpp"
#include "system/cassert.hpp"
#include "system/debug.hpp"
#include <cerrno>
#include <cstring>


#ifdef HAS_EPOLL

#include <sys/epoll.h>

enum{
	FLAG_POLL_IN  = EPOLLIN,
	FLAG_POLL_OUT = EPOLLOUT 
};

#endif

#ifdef HAS_KQUEUE

#include <sys/event.h>
#include <sys/time.h>
#include <fcntl.h>

enum{
	FLAG_POLL_IN  = 1,
	FLAG_POLL_OUT = 2 
};

#endif


namespace solid{
namespace frame{
namespace aio{

#ifdef NINLINES
#include "aiosocket.ipp"
#endif


struct Socket::StationData{
	StationData()/*:rcvaddrpair(rcvaddr)*/{}
	static unsigned specificCount(){return 0xffffff;}
	void specificRelease(){
		sndaddrpair.clear();
	}
	SocketAddress		rcvaddr;
	//SocketAddressStub	rcvaddrpair;
	SocketAddressStub	sndaddrpair;
};

struct Socket::AcceptorData{
	AcceptorData():psd(NULL){}
	static unsigned specificCount(){return 0xffffff;}
	void specificRelease(){psd = NULL;}
	SocketDevice	*psd;
};


Socket::Socket(Type _type, SecureSocket *_pss):
	pss(NULL),
	type(_type), want(0), rcvcnt(0), sndcnt(0),
	rcvbuf(NULL), sndbuf(NULL), rcvlen(0), sndlen(0), ioreq(0)
{
	d.psd = NULL;
}

Socket::Socket(Type _type, const SocketDevice &_rsd, SecureSocket *_pss):
	sd(_rsd),
	pss(NULL),
	type(_type), want(0), rcvcnt(0), sndcnt(0),
	rcvbuf(NULL), sndbuf(NULL), rcvlen(0), sndlen(0), ioreq(0)
{	
	sd.makeNonBlocking();
	secureSocket(_pss);
	d.psd = NULL;
}

Socket::~Socket(){
	delete pss;
}


bool Socket::create(const ResolveIterator& _rai){
	return sd.create(_rai);
}
AsyncE Socket::connect(const SocketAddressStub& _rsas){
	cassert(!isSendPending());
	cassert(type == CHANNEL);
	const solid::AsyncE rv = sd.connectNonBlocking(_rsas);
	if(rv == solid::AsyncWait){
		sndbuf = reinterpret_cast<char*>(1);
		sndlen = 0;
		ioreq |= FLAG_POLL_OUT;
	}
	return static_cast<AsyncE>(rv);
}

AsyncE Socket::accept(SocketDevice &_rsd){
	cassert(type == ACCEPTOR);
	const solid::AsyncE rv = sd.acceptNonBlocking(_rsd);
	if(rv == solid::AsyncWait){
		d.pad->psd = &_rsd;
		rcvbuf = reinterpret_cast<char*>(1);
		rcvlen = 0;
		ioreq |= FLAG_POLL_IN;
	}
	return static_cast<AsyncE>(rv);
}

AsyncE Socket::accept(Socket &_rs){
	cassert(type == ACCEPTOR);
	const solid::AsyncE rv = sd.acceptNonBlocking(_rs.sd);
	if(rv == solid::AsyncWait){
		d.pad->psd = &_rs.sd;
		rcvbuf = reinterpret_cast<char*>(1);
		rcvlen = 0;
		ioreq |= FLAG_POLL_IN;
	}
	return static_cast<AsyncE>(rv);
}

AsyncE Socket::doSendPlain(const char* _pb, uint32 _bl, uint32 _flags){
	cassert(!isSendPending());
	cassert(type == CHANNEL);
	if(!_bl) return AsyncSuccess;
	int rv = sd.send(_pb, _bl);
	if(rv == (int)_bl){
		sndlen = 0;
		sndcnt += rv;
		return AsyncSuccess;
	}
	if(!rv) return AsyncError;
	if(rv < 0){
		if(errno != EAGAIN) return AsyncError;
		rv = 0;
	}
	sndbuf = _pb + rv;
	sndlen = _bl - rv;
	ioreq |= FLAG_POLL_OUT;
	return AsyncWait;
}

AsyncE Socket::doRecvPlain(char *_pb, uint32 _bl, uint32 _flags){
	cassert(!isRecvPending());
	cassert(type == CHANNEL);
	if(!_bl) return AsyncSuccess;
	int rv = sd.recv(_pb, _bl);
	if(rv > 0){
		rcvlen = rv;
		rcvcnt += rv;
		return AsyncSuccess;
	}
	if(!rv) return AsyncError;
	if(errno != EAGAIN) return AsyncError;
	rcvbuf = _pb;
	rcvlen = _bl;
	ioreq |= FLAG_POLL_IN;
	return AsyncWait;
}

uint32 Socket::recvSize()const{
	return rcvlen;
}

const uint64& Socket::sendCount()const{
	return sndcnt;
}

const uint64& Socket::recvCount()const{
	return rcvcnt;
}

bool Socket::isSendPending()const{
	return sndbuf != NULL;
}

bool Socket::isRecvPending()const{
	return rcvbuf != NULL;
}

bool Socket::localAddress(SocketAddress &_rsa)const{
	return sd.localAddress(_rsa);
}

bool Socket::remoteAddress(SocketAddress &_rsa)const{
	return sd.remoteAddress(_rsa);
}

AsyncE Socket::recvFrom(char *_pb, uint32 _bl, uint32 _flags){
	if(!_bl) return AsyncSuccess;
	cassert(!isRecvPending());
	int rv = sd.recv(_pb, _bl, d.psd->rcvaddr);
	if(rv > 0){
		rcvlen = rv;
		rcvcnt += rv;
		//d.psd->rcvaddrpair.size(d.psd->rcvaddr.size());
		//d.pad->rcvaddrpair.addr = rcvsa.addr();
		//d.pad->rcvaddrpair.size = rcvsa.size();
		return AsyncSuccess;
	}
	if(rv == 0) return AsyncError;
	if(errno != EAGAIN) return AsyncError;
	rcvbuf = _pb;
	rcvlen = _bl;
	ioreq |= FLAG_POLL_IN;
	return AsyncWait;
}

AsyncE Socket::sendTo(const char *_pb, uint32 _bl, const SocketAddressStub &_sap, uint32 _flags){	
	cassert(!isSendPending());
	int rv = sd.send(_pb, _bl, _sap);
	if(rv == (ssize_t)_bl){
		sndlen = 0;
		sndcnt += rv;
		return AsyncSuccess;
	}
	if(rv >= 0) return AsyncError;
	if(errno != EAGAIN) return AsyncError;
	sndbuf = _pb;
	sndlen = _bl;
	
	ioreq |= FLAG_POLL_OUT;
	d.psd->sndaddrpair = _sap;
	return AsyncWait;
}

const SocketAddress &Socket::recvAddr()const{
	return d.psd->rcvaddr;
}

void Socket::doPrepare(){
	switch(type){
		case ACCEPTOR:
			d.pad = Specific::uncache<Socket::AcceptorData>();
			break;
		case CHANNEL:
			break;
		case STATION:
			d.psd = Specific::uncache<Socket::StationData>();
			break;
	}
}

void Socket::doUnprepare(){
	switch(type){
		case ACCEPTOR:
			Specific::cache(d.pad);
			break;
		case CHANNEL:
			break;
		case STATION:
			Specific::cache(d.psd);
			break;
	}
}
/*NOTE: for both doSend and doRecv
	We must also check for sndbuf/rcvbuf on send or recv
	because we allow to have events on sockets not waiting for events
	(this is a limitation of the aio selector). Here's a situation
	1) cona issue recv == NOK for 4K
	2) selector wait recv on cona
	3) cona->dorecv 2K
	4) cona->execute, return ok
	5) selector puts cona in execq (selector still waits for recv on cona)
	6) selector does epollselect and finds cona ready for read
	
	The situation is far more difficult to avoid on multiconnections/multitalkers.
*/
ulong Socket::doSendPlain(){
	switch(type){
		case CHANNEL://tcp
			if(sndlen && sndbuf){//NOTE: see the above note
				const int rv = sd.send(sndbuf, sndlen);
				vdbgx(Debug::aio, "send rv = "<<rv);
				if(rv <= 0) return EventDoneError;
				sndbuf += rv;
				sndlen -= rv;
				sndcnt += rv;
				if(sndlen) return EventNone;//not yet done
			}
			sndbuf = NULL;
			ioreq &= ~FLAG_POLL_OUT;
			return EventDoneSend;
		case STATION://udp
			if(sndlen && sndbuf){//NOTE: see the above note
				const int rv = sd.send(sndbuf, sndlen, d.psd->sndaddrpair);
				if(rv != (int)sndlen) return EventDoneError;
				sndcnt += rv;
				sndlen = 0;
			}
			sndbuf = NULL;
			ioreq &= ~FLAG_POLL_OUT;
			return EventDoneSend;
	}
	cassert(false);
	return EventDoneError;
}

ulong Socket::doRecvPlain(){
	switch(type){
		case CHANNEL://tcp
			if(rcvlen && rcvbuf){//NOTE: see the above note
				const int rv = sd.recv(rcvbuf, rcvlen);
				vdbgx(Debug::aio, "recv rv = "<<rv<<" err = "<<strerror(errno)<<" rcvbuf = "<<(void*)rcvbuf<<" rcvlen = "<<rcvlen);
				if(rv <= 0) return EventDoneError;
				rcvcnt += rv;
				rcvlen = rv;
			}
			rcvbuf = NULL;
			ioreq &= ~FLAG_POLL_IN;
			return EventDoneRecv;
		case STATION://udp
			if(rcvlen && rcvbuf){//NOTE: see the above note
				const int rv = sd.recv(rcvbuf, rcvlen, d.psd->rcvaddr);
				if(rv <= 0) return EventDoneError;
				rcvcnt += rv;
				rcvlen = rv;
				//d.psd->rcvaddrpair.size(d.psd->rcvaddr.size());
			}
			rcvbuf = NULL;
			ioreq &= ~FLAG_POLL_IN;
			return EventDoneRecv;
		case ACCEPTOR:{
			const bool rv = sd.accept(*d.pad->psd);
			sndbuf = NULL;
			ioreq &= ~FLAG_POLL_IN;
			if(rv) return EventDoneSend;
			}
			return EventDoneError;
	};
	cassert(false);
	return EventDoneError;
}
void  Socket::doClear(){
	rcvbuf = NULL;
	sndbuf = NULL;
	ioreq = 0;
}

inline void Socket::doWantAccept(int _w){
	if(_w & (SecureSocket::WANT_READ)){
		ioreq |= FLAG_POLL_IN;
		want = SecureSocket::WANT_READ_ON_ACCEPT;
	}
	if(_w & (SecureSocket::WANT_WRITE)){
		ioreq |= FLAG_POLL_OUT;
		want |= SecureSocket::WANT_WRITE_ON_ACCEPT;
	}
}
inline void Socket::doWantConnect(int _w){
	if(_w & (SecureSocket::WANT_READ)){
		ioreq |= FLAG_POLL_IN;
		want = SecureSocket::WANT_READ_ON_CONNECT;
	}
	if(_w & (SecureSocket::WANT_WRITE)){
		ioreq |= FLAG_POLL_OUT;
		want |= SecureSocket::WANT_WRITE_ON_CONNECT;
	}
}
inline void Socket::doWantRead(int _w){
	if(_w & (SecureSocket::WANT_READ)){
		ioreq |= FLAG_POLL_IN;
		want |= SecureSocket::WANT_READ_ON_READ;
	}
	if(_w & (SecureSocket::WANT_WRITE)){
		ioreq |= FLAG_POLL_OUT;
		want |= SecureSocket::WANT_WRITE_ON_READ;
	}
}
inline void Socket::doWantWrite(int _w){
	if(_w & (SecureSocket::WANT_READ)){
		ioreq |= FLAG_POLL_IN;
		want |= SecureSocket::WANT_READ_ON_WRITE;
	}
	if(_w & (SecureSocket::WANT_WRITE)){
		ioreq |= FLAG_POLL_OUT;
		want |= SecureSocket::WANT_WRITE_ON_WRITE;
	}
}

AsyncE Socket::doSendSecure(const char* _pb, uint32 _bl, uint32 _flags){
	cassert(!isSendPending());
	cassert(type == CHANNEL);
	if(!_bl) return AsyncSuccess;
	int rv = pss->send(_pb, _bl);
	if(rv == (int)_bl){
		sndlen = 0;
		sndcnt += rv;
		return AsyncSuccess;
	}
	if(!rv) return AsyncError;
	if(rv < 0){
		int w = pss->wantEvents();
		if(!w) return AsyncError;
		doWantWrite(w);
		rv = 0;
	}
	sndbuf = _pb + rv;
	sndlen = _bl - rv;
	return AsyncWait;
}

AsyncE Socket::doRecvSecure(char *_pb, uint32 _bl, uint32 _flags){
	cassert(!isRecvPending());
	cassert(type == CHANNEL);
	if(!_bl) return AsyncSuccess;
	const int rv = pss->recv(_pb, _bl);
	if(rv > 0){
		rcvlen = rv;
		rcvcnt += rv;
		return AsyncSuccess;
	}
	if(!rv) return AsyncError;
	int w = pss->wantEvents();
	if(!w) return AsyncError;
	doWantRead(w);
	rcvbuf = _pb;
	rcvlen = _bl;
	ioreq |= FLAG_POLL_IN;
	return AsyncWait;
}

ulong Socket::doSecureAccept(){
	const AsyncE rv = pss->secureAccept();
	vdbgx(Debug::aio, " secureaccept "<<rv);
	ioreq = 0;
	want = 0;
	if(rv == AsyncSuccess){
		rcvbuf = NULL;
		return EventDoneSend;
	}
	if(rv == AsyncError){
		rcvbuf = NULL;
		return EventDoneError;
	}
	doWantAccept(pss->wantEvents());
	return EventNone;
}

ulong Socket::doSecureConnect(){
	const AsyncE rv = pss->secureConnect();
	ioreq = 0;
	want = 0;
	if(rv == AsyncSuccess){
		sndbuf = NULL;
		return EventDoneSend;
	}
	if(rv == AsyncError){
		sndbuf = NULL;
		return EventDoneSend;
	}
	doWantAccept(pss->wantEvents());
	return EventNone;
}

ulong Socket::doSecureReadWrite(int _w){
	ulong retval = 0;
	int w = _w & (SecureSocket::WANT_WRITE_ON_WRITE | SecureSocket::WANT_READ_ON_WRITE);
	if(w){
		want &= (~w);
		if(sndlen && sndbuf){//NOTE: see the above note
			int rv = pss->send(sndbuf, sndlen);
			vdbgx(Debug::aio, "send rv = "<<rv);
			if(rv == 0) return EventDoneError;
			if(rv < 0){
				return EventNone;
			}else{
				sndbuf += rv;
				sndcnt += rv;
				sndlen -= rv;
				if(sndlen){
					
				}else{
					sndbuf = NULL;
					retval |= EventDoneSend;
				}
			}
		}else{
			sndbuf = NULL;
			retval |= EventDoneSend;
		}
	}
	w = _w & (SecureSocket::WANT_WRITE_ON_READ | SecureSocket::WANT_READ_ON_READ);
	if(w){
		want &= (~w);
		if(rcvlen && rcvbuf){//NOTE: see the above note
			int rv = pss->recv(rcvbuf, rcvlen);
			vdbgx(Debug::aio, "recv rv = "<<rv);
			if(rv == 0) return EventDoneError;
			if(rv < 0){
				int sw = pss->wantEvents();
				if(!sw) return EventDoneError;
				
				return 0;
			}else{
				rcvcnt += rv;
				rcvlen = rv;
				rcvbuf = NULL;
				retval |= EventDoneRecv;
			}
		}else{
			sndbuf = NULL;
			retval |= EventDoneRecv;
		}
	}
	return retval;
}

ulong Socket::doSendSecure(){
	ioreq &= ~FLAG_POLL_OUT;
	{
		int w = want & (SecureSocket::WANT_WRITE_ON_WRITE | SecureSocket::WANT_WRITE_ON_READ);
		if(w){
			return doSecureReadWrite(w);
		}
	}
	if(want == SecureSocket::WANT_WRITE_ON_ACCEPT){
		return doSecureAccept();
	}
	if(want == SecureSocket::WANT_WRITE_ON_CONNECT){
		return doSecureConnect();
	}
	cassert(false);
	return EventDoneError;
}

ulong Socket::doRecvSecure(){
	ioreq &= ~FLAG_POLL_IN;
	{
		int w = want & (SecureSocket::WANT_READ_ON_WRITE | SecureSocket::WANT_READ_ON_READ);
		if(w){
			return doSecureReadWrite(w);
		}
	}
	if(want == SecureSocket::WANT_READ_ON_ACCEPT){
		return doSecureAccept();
	}
	if(want == SecureSocket::WANT_READ_ON_CONNECT){
		return doSecureConnect();
	}
	cassert(false);
	return EventDoneError;
}

AsyncE Socket::secureAccept(){
	cassert(!isRecvPending());
	cassert(isSecure());
	cassert(type == CHANNEL);
	const AsyncE rv = pss->secureAccept();
	if(rv != AsyncWait) return rv;
	rcvbuf = reinterpret_cast<char*>(1);
	rcvlen = 0;
	ioreq = 0;
	doWantAccept(pss->wantEvents());
	return AsyncWait;
}

AsyncE Socket::secureConnect(){
	cassert(!isRecvPending());
	cassert(isSecure());
	cassert(type == CHANNEL);
	const AsyncE rv = pss->secureConnect();
	if(rv != AsyncWait) return rv;
	
	sndbuf = "";
	sndlen = 0;
	ioreq = 0;
	doWantConnect(pss->wantEvents());
	return AsyncWait;
}

void Socket::secureSocket(SecureSocket *_pss){
	delete pss;
	pss = _pss;
	if(pss)
		pss->descriptor(sd);
}

}//namespace aio
}//namespace frame
}//namespace solid

