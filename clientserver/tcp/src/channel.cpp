/* Implementation file channel.cpp
	
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

#include <unistd.h>
#include <cerrno>
#include <fcntl.h>

#include "system/debug.hpp"
#include "system/socketaddress.hpp"

#include "core/common.hpp"

#include "tcp/channel.hpp"
#include "tcp/connectionselector.hpp"

#include "channeldata.hpp"

namespace clientserver{
namespace tcp{

#ifndef UINLINES
#include "channel.ipp"
#endif

Channel* Channel::create(const AddrInfoIterator &_rai){
	cassert(false);
	return NULL;
}

Channel::Channel(int _sd):sd(_sd), rcvcnt(0), sndcnt(0), pcd(NULL), psch(NULL){
	if(ok() && fcntl(sd, F_SETFL, O_NONBLOCK) < 0){
		close(sd);
		sd = -1;
	}
}

Channel::~Channel(){
	if(ok()){
		shutdown(sd, SHUT_RDWR);
		close(sd);
	}
}

uint32 Channel::recvSize()const {
	return pcd->rcvsz;
}

void Channel::prepare(){
	pcd = ChannelData::pop();
	pcd->clear();
}

void Channel::unprepare(){
	ChannelData::push(pcd);
}

ulong Channel::ioRequest()const{
	return pcd->flags & IO_TOUT_FLAGS;
}

int Channel::arePendingSends(){
	return pcd->arePendingSends();
}

int Channel::localAddress(SocketAddress &_rsa){
	_rsa.clear();
	_rsa.size() = SocketAddress::MaxSockAddrSz;
	int rv = getsockname(sd, _rsa.addr(), &_rsa.size());
	if(rv){
		idbg("error getting local socket address: "<<strerror(errno));
		return BAD;
	}
	return OK;
}

int Channel::remoteAddress(SocketAddress &_rsa){
	_rsa.clear();
	_rsa.size() = SocketAddress::MaxSockAddrSz;
	int rv = getpeername(sd, _rsa.addr(), &_rsa.size());
	if(rv){
		idbg("error getting local socket address: "<<strerror(errno));
		return BAD;
	}
	return OK;
}


//TODO: try to only do connect here
// and create the socket, bind it to a certain interface in 'create'
int Channel::connect(const AddrInfoIterator &_it){
	//in order to make the selector wait for data out, just add an empty buffer to snddq.
	if(ok()){	close(sd);}
	
	sd = socket(_it.family(), _it.type(), _it.protocol());
	if(!ok()) return BAD;
	int flg = fcntl(sd, F_GETFL);
	if(flg == -1){
		idbg("Error fcntl getfl: "<<strerror(errno));
		close(sd);
		sd = -1;
		return BAD;
	}
	if(fcntl(sd, F_SETFL, flg | O_NONBLOCK) < 0){
		idbg("Error fcntl setfl: "<<strerror(errno));
		close(sd);
		sd = -1;
		return BAD;
	}
	int rv = ::connect(sd, _it.addr(), _it.size());
	if(rv < 0){
		if(errno != EINPROGRESS) return BAD;
		rv = NOK;
		pcd->pushSend((const char*) NULL, 0, RAISE_ON_END);
		pcd->flags |= OUTTOUT;
	}else rv = OK;
	return rv;
}

int Channel::doSendPlain(const char* _pb, uint32 _bl, uint32 _flags){
	if(!_bl) return OK;
	int rv = 0;
	if(!pcd->arePendingSends()){
		//try to send it
		rv = write(sd, _pb, _bl);
		if(rv == (int)_bl) return OK;
		if(!rv) return BAD;
		if(rv < 0){
			if(errno != EAGAIN) return BAD;
			rv = 0;
		}
		pcd->flags |= OUTTOUT;
	}
	cassert(!pcd->arePendingSends());
	pcd->pushSend(_pb + rv, _bl - rv, _flags);
	return NOK;
}
int Channel::doSendSecure(const char* _pb, uint32 _bl, uint32 _flags){
	//TODO:
	return BAD;
}
int Channel::doRecvPlain(char* _pb, uint32 _bl, uint32 _flags){
	if(!_bl) return OK;
	if(!(_flags & PREPARE_ONLY)){
		int rv = read(sd, _pb, _bl);
		if(rv > 0){
			pcd->rcvsz = rv;
			return OK;
		}
		if(!rv) return BAD;
		if(rv < 0 && errno != EAGAIN) return BAD;
	}
	if(!(_flags & TRY_ONLY)){
		pcd->setRecv(_pb, _bl, _flags);
		pcd->rcvsz = 0;
		pcd->flags |= INTOUT;
	}else{
		pcd->flags &= ~INTOUT;
	}
	
	return NOK;
}
int Channel::doRecvSecure(char* _pbuf, uint32 _bl, uint32 _flags){
	//TODO:
	return BAD;
}

//--- Interface used by ConnectionSelector
/*
NOTE:
	* Here's the situation when the signal must be ignored:
	S: tout on read
	C: send 2k
	C: send 2k
	S: read 2k with 4k buf
	S: process data
	S: yield (exit with OK from connection::execute)
	S: epoll, signals ready for reading
	S: try do a recv, channel not ready for receiving i.e. no buffer is set
	
	* There should be no problem ignoring the signal from epoll, because
	the connection doesnt know there was actually a timeout, and will
	surely do a new non blocking read which will succeed.
*/
int Channel::doRecvPlain(){
	if(!pcd->rdn.b.pb) return 0;//Ignore this signal
	pcd->flags &= ~INTOUT;
	int rv;
	//we've got a buffer
	rv = read(sd, pcd->rdn.b.pb, pcd->rdn.bl);
	if(rv <= 0) return ERRDONE;
	pcd->rcvsz = rv;
	pcd->rdn.b.pb = NULL; //rcvd.flags = 0;
	return INDONE;
}
int Channel::doRecvSecure(){
	//TODO:
	return BAD;
}
int Channel::doSendPlain(){
	int rv = 0;
	int retv = 0;
	if(!pcd->arePendingSends()) return 0;//Ignore this signal
	pcd->flags &= ~OUTTOUT;
	while(pcd->arePendingSends()){
		//idbg("another pending send");
		DataNode	&rdn = *pcd->psdnfirst;
		
		rv = write(sd, rdn.b.pcb, rdn.bl);
		if(rv == (int)rdn.bl){
			if(rdn.flags & RAISE_ON_END) retv |= OUTDONE;
			pcd->popSendData();
			continue;
		}
		if(!rv) return ERRDONE;
		if(rv > 0){
			rdn.b.pcb += rv; rdn.bl -= rv;
		}else{
			if(errno != EAGAIN) return ERRDONE;
			pcd->flags |= OUTTOUT;
		}
		return retv;
	}
	return OUTDONE;
}
int Channel::doSendSecure(){
	//TODO:
	return BAD;
}


}//namespace tcp
}//namespace clientserver
