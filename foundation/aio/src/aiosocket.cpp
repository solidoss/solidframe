#include "foundation/common.hpp"
#include "foundation/aio/src/aiosocket.hpp"
#include "foundation/aio/aiosecuresocket.hpp"
#include "system/socketaddress.hpp"
#include "system/specific.hpp"
#include "system/cassert.hpp"
#include "system/debug.hpp"
#include <cerrno>
#include <cstring>
#include <sys/epoll.h>

namespace foundation{
namespace aio{

#ifndef UINLINES
#include "aiosocket.ipp"
#endif


struct Socket::StationData{
	StationData():rcvaddrpair(rcvaddr){}
	static unsigned specificCount(){return 0xffffff;}
	void specificRelease(){
		sndaddrpair.addr = NULL;
	}
	SocketAddress	rcvaddr;
	SockAddrPair	rcvaddrpair;
	SockAddrPair	sndaddrpair;
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


int Socket::create(const AddrInfoIterator& _rai){
	return sd.create(_rai);
}
int Socket::connect(const AddrInfoIterator& _rai){
	cassert(!isSendPending());
	cassert(type == CHANNEL);
	int rv = sd.connect(_rai);
	if(rv == NOK){
		sndbuf = "";
		sndlen = 0;
		ioreq |= EPOLLOUT;
	}
	return rv;
}
int Socket::accept(SocketDevice &_rsd){
	cassert(type == ACCEPTOR);
	int rv = sd.accept(_rsd);
	if(rv == NOK){
		d.pad->psd = &_rsd;
		rcvbuf = "";
		rcvlen = 0;
		ioreq |= EPOLLIN;
	}
	return rv;
}
int Socket::accept(Socket &_rs){
	cassert(type == ACCEPTOR);
	int rv = sd.accept(_rs.sd);
	if(rv == NOK){
		d.pad->psd = &_rs.sd;
		rcvbuf = "";
		rcvlen = 0;
		ioreq |= EPOLLIN;
	}
	return rv;
}
int Socket::doSendPlain(const char* _pb, uint32 _bl, uint32 _flags){
	cassert(!isSendPending());
	cassert(type == CHANNEL);
	if(!_bl) return OK;
	int rv = sd.send(_pb, _bl);
	if(rv == (int)_bl){
		sndlen = 0;
		sndcnt += rv;
		return OK;
	}
	if(!rv) return BAD;
	if(rv < 0){
		if(errno != EAGAIN) return BAD;
		rv = 0;
	}
	sndbuf = _pb + rv;
	sndlen = _bl - rv;
	ioreq |= EPOLLOUT;
	return NOK;
}
int Socket::doRecvPlain(char *_pb, uint32 _bl, uint32 _flags){
	cassert(!isRecvPending());
	cassert(type == CHANNEL);
	if(!_bl) return OK;
	int rv = sd.recv(_pb, _bl);
	if(rv > 0){
		rcvlen = rv;
		rcvcnt += rv;
		return OK;
	}
	if(!rv) return BAD;
	if(errno != EAGAIN) return BAD;
	rcvbuf = _pb;
	rcvlen = _bl;
	ioreq |= EPOLLIN;
	return NOK;
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
int Socket::localAddress(SocketAddress &_rsa)const{
	return sd.localAddress(_rsa);
}
int Socket::remoteAddress(SocketAddress &_rsa)const{
	return sd.remoteAddress(_rsa);
}

int Socket::recvFrom(char *_pb, uint32 _bl, uint32 _flags){
	if(!_bl) return OK;
	cassert(!isRecvPending());
	d.psd->rcvaddr.size() = SocketAddress::MaxSockAddrSz;
	int rv = sd.recv(_pb, _bl, d.psd->rcvaddr);
	if(rv > 0){
		rcvlen = rv;
		rcvcnt += rv;
		d.psd->rcvaddrpair.size = d.psd->rcvaddr.size();
		//d.pad->rcvaddrpair.addr = rcvsa.addr();
		//d.pad->rcvaddrpair.size = rcvsa.size();
		return OK;
	}
	if(rv == 0) return BAD;
	if(errno != EAGAIN) return BAD;
	rcvbuf = _pb;
	rcvlen = _bl;
	ioreq |= EPOLLIN;
	return NOK;
}
int Socket::sendTo(const char *_pb, uint32 _bl, const SockAddrPair &_sap, uint32 _flags){	
	cassert(!isSendPending());
	int rv = sd.send(_pb, _bl, _sap);
	if(rv == (ssize_t)_bl){
		sndlen = 0;
		sndcnt += rv;
		return OK;
	}
	if(rv >= 0) return BAD;
	if(errno != EAGAIN) return BAD;
	sndbuf = _pb;
	sndlen = _bl;
	
	ioreq |= EPOLLOUT;
	d.psd->sndaddrpair = _sap;
	return NOK;
}
const SockAddrPair &Socket::recvAddr()const{
	return d.psd->rcvaddrpair;
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
int Socket::doSendPlain(){
	switch(type){
		case CHANNEL://tcp
			if(sndlen && sndbuf){//NOTE: see the above note
				int rv = sd.send(sndbuf, sndlen);
				vdbgx(Dbg::aio, "send rv = "<<rv);
				if(rv <= 0) return ERRDONE;
				sndbuf += rv;
				sndlen -= rv;
				sndcnt += rv;
				if(sndlen) return 0;//not yet done
			}
			sndbuf = NULL;
			ioreq &= ~EPOLLOUT;
			return OUTDONE;
		case STATION://udp
			if(sndlen && sndbuf){//NOTE: see the above note
				int rv = sd.send(sndbuf, sndlen, d.psd->sndaddrpair);
				if(rv != (int)sndlen) return ERRDONE;
				sndcnt += rv;
				sndlen = 0;
			}
			sndbuf = NULL;
			ioreq &= ~EPOLLOUT;
			return OUTDONE;
	}
	cassert(false);
	return ERRDONE;
}
int Socket::doRecvPlain(){
	switch(type){
		case ACCEPTOR:{
			int rv = sd.accept(*d.pad->psd);
			sndbuf = NULL;
			ioreq &= ~EPOLLIN;
			if(rv == OK) return OUTDONE;
			}return ERRDONE;
		case CHANNEL://tcp
			if(rcvlen && rcvbuf){//NOTE: see the above note
				int rv = sd.recv(rcvbuf, rcvlen);
				vdbgx(Dbg::aio, "recv rv = "<<rv<<" err = "<<strerror(errno)<<" rcvbuf = "<<(void*)rcvbuf<<" rcvlen = "<<rcvlen);
				if(rv <= 0) return ERRDONE;
				rcvcnt += rv;
				rcvlen = rv;
			}
			rcvbuf = NULL;
			ioreq &= ~EPOLLIN;
			return INDONE;
		case STATION://udp
			if(rcvlen && rcvbuf){//NOTE: see the above note
				int rv = sd.recv(rcvbuf, rcvlen, d.psd->rcvaddr);
				if(rv <= 0) return ERRDONE;
				rcvcnt += rv;
				rcvlen = rv;
				d.psd->rcvaddrpair.size = d.psd->rcvaddr.size();
			}
			rcvbuf = NULL;
			ioreq &= ~EPOLLIN;
			return INDONE;
	};
	cassert(false);
	return ERRDONE;
}
void  Socket::doClear(){
	rcvbuf = NULL;
	sndbuf = NULL;
	ioreq = 0;
}

inline void Socket::doWantAccept(int _w){
	if(_w & (SecureSocket::WANT_READ)){
		ioreq |= EPOLLIN;
		want = SecureSocket::WANT_READ_ON_ACCEPT;
	}
	if(_w & (SecureSocket::WANT_WRITE)){
		ioreq |= EPOLLOUT;
		want |= SecureSocket::WANT_WRITE_ON_ACCEPT;
	}
}
inline void Socket::doWantConnect(int _w){
	if(_w & (SecureSocket::WANT_READ)){
		ioreq |= EPOLLIN;
		want = SecureSocket::WANT_READ_ON_CONNECT;
	}
	if(_w & (SecureSocket::WANT_WRITE)){
		ioreq |= EPOLLOUT;
		want |= SecureSocket::WANT_WRITE_ON_CONNECT;
	}
}
inline void Socket::doWantRead(int _w){
	if(_w & (SecureSocket::WANT_READ)){
		ioreq |= EPOLLIN;
		want |= SecureSocket::WANT_READ_ON_READ;
	}
	if(_w & (SecureSocket::WANT_WRITE)){
		ioreq |= EPOLLOUT;
		want |= SecureSocket::WANT_WRITE_ON_READ;
	}
}
inline void Socket::doWantWrite(int _w){
	if(_w & (SecureSocket::WANT_READ)){
		ioreq |= EPOLLIN;
		want |= SecureSocket::WANT_READ_ON_WRITE;
	}
	if(_w & (SecureSocket::WANT_WRITE)){
		ioreq |= EPOLLOUT;
		want |= SecureSocket::WANT_WRITE_ON_WRITE;
	}
}

int Socket::doSendSecure(const char* _pb, uint32 _bl, uint32 _flags){
	cassert(!isSendPending());
	cassert(type == CHANNEL);
	if(!_bl) return OK;
	int rv = pss->send(_pb, _bl);
	if(rv == (int)_bl){
		sndlen = 0;
		sndcnt += rv;
		return OK;
	}
	if(!rv) return BAD;
	if(rv < 0){
		int w = pss->wantEvents();
		if(!w) return BAD;
		doWantWrite(w);
		rv = 0;
	}
	sndbuf = _pb + rv;
	sndlen = _bl - rv;
	return NOK;
}
int Socket::doRecvSecure(char *_pb, uint32 _bl, uint32 _flags){
	cassert(!isRecvPending());
	cassert(type == CHANNEL);
	if(!_bl) return OK;
	int rv = pss->recv(_pb, _bl);
	if(rv > 0){
		rcvlen = rv;
		rcvcnt += rv;
		return OK;
	}
	if(!rv) return BAD;
	int w = pss->wantEvents();
	if(!w) return BAD;
	doWantRead(w);
	rcvbuf = _pb;
	rcvlen = _bl;
	ioreq |= EPOLLIN;
	return NOK;
}

int Socket::doSecureAccept(){
	int rv = pss->secureAccept();
	vdbgx(Dbg::aio, " secureaccept "<<rv);
	ioreq = 0;
	want = 0;
	if(rv == OK){
		rcvbuf = NULL;
		return OUTDONE;
	}
	if(rv == BAD){
		rcvbuf = NULL;
		return ERRDONE;
	}
	doWantAccept(pss->wantEvents());
	return 0;
}
int Socket::doSecureConnect(){
	int rv = pss->secureConnect();
	ioreq = 0;
	want = 0;
	if(rv == OK){
		sndbuf = NULL;
		return OUTDONE;
	}
	if(rv == BAD){
		sndbuf = NULL;
		return ERRDONE;
	}
	doWantAccept(pss->wantEvents());
	return 0;
}

int Socket::doSecureReadWrite(int _w){
	int retval = 0;
	int w = _w & (SecureSocket::WANT_WRITE_ON_WRITE | SecureSocket::WANT_READ_ON_WRITE);
	if(w){
		want &= (~w);
		if(sndlen && sndbuf){//NOTE: see the above note
			int rv = pss->send(sndbuf, sndlen);
			vdbgx(Dbg::aio, "send rv = "<<rv);
			if(rv == 0) return ERRDONE;
			if(rv < 0){
				
				return 0;
			}else{
				sndbuf += rv;
				sndcnt += rv;
				sndlen -= rv;
				if(sndlen){
					
				}else{
					sndbuf = NULL;
					retval |= OUTDONE;
				}
			}
		}else{
			sndbuf = NULL;
			retval |= OUTDONE;
		}
	}
	w = _w & (SecureSocket::WANT_WRITE_ON_READ | SecureSocket::WANT_READ_ON_READ);
	if(w){
		want &= (~w);
		if(rcvlen && rcvbuf){//NOTE: see the above note
			int rv = pss->recv(rcvbuf, rcvlen);
			vdbgx(Dbg::aio, "recv rv = "<<rv);
			if(rv == 0) return ERRDONE;
			if(rv < 0){
				int sw = pss->wantEvents();
				if(!sw) return ERRDONE;
				
				return 0;
			}else{
				rcvcnt += rv;
				rcvlen = rv;
				rcvbuf = NULL;
				retval |= INDONE;
			}
		}else{
			sndbuf = NULL;
			retval |= INDONE;
		}
	}
	return retval;
}

int Socket::doSendSecure(){
	ioreq &= ~EPOLLOUT;
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
	return ERRDONE;
}
int Socket::doRecvSecure(){
	ioreq &= ~EPOLLIN;
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
	return ERRDONE;
}

int Socket::secureAccept(){
	cassert(!isRecvPending());
	cassert(isSecure());
	cassert(type == CHANNEL);
	int rv = pss->secureAccept();
	if(rv == OK) return OK;
	if(rv == BAD) return BAD;
	rcvbuf = "";
	rcvlen = 0;
	ioreq = 0;
	doWantAccept(pss->wantEvents());
	return NOK;
}
int Socket::secureConnect(){
	cassert(!isRecvPending());
	cassert(isSecure());
	cassert(type == CHANNEL);
	int rv = pss->secureConnect();
	if(rv == OK) return OK;
	if(rv == BAD) return BAD;
	sndbuf = "";
	sndlen = 0;
	ioreq = 0;
	doWantConnect(pss->wantEvents());
	return NOK;
}

void Socket::secureSocket(SecureSocket *_pss){
	delete pss;
	pss = _pss;
	if(pss)
		pss->descriptor(sd);
}

}//namespace aio
}//namespace foundation

