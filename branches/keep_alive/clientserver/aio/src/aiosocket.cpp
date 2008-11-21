#include "clientserver/core/common.hpp"
#include "clientserver/aio/src/aiosocket.hpp"
#include "system/socketaddress.hpp"
#include "system/specific.hpp"
#include "system/cassert.hpp"
#include "system/debug.hpp"
#include <cerrno>
#include <sys/epoll.h>

namespace clientserver{
namespace aio{

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


Socket::Socket(Type _type):
	type(_type), rcvcnt(0), sndcnt(0),
	rcvbuf(NULL), sndbuf(NULL), rcvlen(0), sndlen(0)
{
	d.psd = NULL;
}

Socket::Socket(Type _type, const SocketDevice &_rsd):
	sd(_rsd),
	type(_type), rcvcnt(0), sndcnt(0),
	rcvbuf(NULL), sndbuf(NULL), rcvlen(0), sndlen(0)
{	
	sd.makeNonBlocking();
	d.psd = NULL;
}


bool Socket::ok()const{
	return sd.ok();
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
	}
	return rv;
}
int Socket::send(const char* _pb, uint32 _bl, uint32 _flags){
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
	return NOK;
}
int Socket::recv(char *_pb, uint32 _bl, uint32 _flags){
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
		d.psd->rcvaddrpair.size = d.psd->rcvaddr.size();
		//d.pad->rcvaddrpair.addr = rcvsa.addr();
		//d.pad->rcvaddrpair.size = rcvsa.size();
		return OK;
	}
	if(rv == 0) return BAD;
	if(errno != EAGAIN) return BAD;
	rcvbuf = _pb;
	rcvlen = _bl;
	return NOK;
}
int Socket::sendTo(const char *_pb, uint32 _bl, const SockAddrPair &_sap, uint32 _flags){	
	cassert(!isSendPending());
	int rv = sd.send(_pb, _bl, _sap);
	if(rv == (ssize_t)_bl) return OK;
	if(rv >= 0) return BAD;
	if(errno != EAGAIN) return BAD;
	sndbuf = _pb;
	sndlen = _bl;
	d.psd->sndaddrpair = _sap;
	return NOK;
}
const SockAddrPair &Socket::recvAddr()const{
	return d.psd->rcvaddrpair;
}

uint32 Socket::ioRequest()const{
	uint32 rv = 0;
	if(sndbuf) rv |= EPOLLOUT;
	if(rcvbuf) rv |= EPOLLIN;
	return rv;
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
int Socket::doSend(){
	switch(type){
		case CHANNEL://tcp
			if(sndlen && sndbuf){//NOTE: see the above note
				int rv = sd.send(sndbuf, sndlen);
				idbgx(Dbg::aio, "send rv = "<<rv);
				if(rv <= 0) return ERRDONE;
				sndbuf += rv;
				sndlen -= rv;
				if(sndlen) return 0;//not yet done
			}
			sndbuf = NULL;
			return OUTDONE;
		case STATION://udp
			if(sndlen && sndbuf){//NOTE: see the above note
				int rv = sd.send(sndbuf, sndlen, d.psd->sndaddrpair);
				if(rv != sndlen) return ERRDONE;
				sndlen = 0;
			}
			sndbuf = NULL;
			return OUTDONE;
	}
	cassert(false);
	return ERRDONE;
}
int Socket::doRecv(){
	switch(type){
		case ACCEPTOR:{
			int rv = sd.accept(*d.pad->psd);
			sndbuf = NULL;
			if(rv == OK) return OUTDONE;
			}return ERRDONE;
		case CHANNEL://tcp
			if(rcvlen && rcvbuf){//NOTE: see the above note
				int rv = sd.recv(rcvbuf, rcvlen);
				idbgx(Dbg::aio, "recv rv = "<<rv<<" err = "<<strerror(errno)<<" rcvbuf = "<<(void*)rcvbuf<<" rcvlen = "<<rcvlen);
				if(rv <= 0) return ERRDONE;
				rcvcnt += rv;
				rcvlen = rv;
			}
			rcvbuf = NULL;
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
			return INDONE;
	};
	cassert(false);
	return ERRDONE;
}
void  Socket::doClear(){
	rcvbuf = NULL;
	sndbuf = NULL;
}

}//namespace aio
}//namespace clientserver

