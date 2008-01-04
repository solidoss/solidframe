/* Implementation file channel.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "system/debug.h"
#include "system/socketaddress.h"

#include "core/common.h"

#include "tcp/channel.h"
#include "tcp/connectionselector.h"

#include "channeldata.h"

namespace clientserver{
namespace tcp{

Channel* Channel::create(const AddrInfoIterator &_rai){
	assert(false);
	return NULL;
}

Channel::Channel(int _sd):sd(_sd), rcvcnt(0), sndcnt(0), pcd(NULL), psch(NULL){
	if(isOk() && fcntl(sd, F_SETFL, O_NONBLOCK) < 0){
		close(sd);
		sd = -1;
	}
}

// Channel::Channel():sd(-1), rcvcnt(0), sndcnt(0), pcd(NULL), psch(NULL){
// }

Channel::~Channel(){
	if(isOk()) close(sd);
	//delete psch;
}

uint32 Channel::recvSize()const {return pcd->rcvsz;}

void Channel::prepare(){
	pcd = ChannelData::pop();
	pcd->clear();
}

void Channel::unprepare(){
	ChannelData::push(pcd);
}

ulong Channel::ioRequest()const{
	//TODO: it can be optimezed
	ulong rv = 0;
	if(pcd->rdn.b.pcb) rv = INTOUT;
	if(pcd->arePendingSends()) return rv | OUTTOUT;
	return rv;
}

int Channel::arePendingSends(){
	return pcd->arePendingSends();
}
//TODO: try to only do connect here
// and create the socket, bind it to a certain interface in 'create'
int Channel::connect(const AddrInfoIterator &_it){
	//in order to make the selector wait for data out, just add an empty buffer to snddq.
	if(isOk()){	close(sd);}
	
	sd = socket(_it.family(), _it.type(), _it.protocol());
	if(!isOk()) return BAD;
	if(fcntl(sd, F_SETFL, O_NONBLOCK) < 0){
		close(sd);
		sd = -1;
		return BAD;
	}
	int rv = ::connect(sd, _it.addr(), _it.size());
	if(rv < 0){
		if(errno != EINPROGRESS) return BAD;
		rv = NOK;
		//snddq.push(Data((const char*)NULL, 0, RAISE_ON_END | IS_BUFFER));
		pcd->pushSend((const char*) NULL, 0, RAISE_ON_END | IS_BUFFER);
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
	}
	//snddq.push(Data(_pb + rv, _bl - rv, _flags | IS_BUFFER));
	pcd->pushSend(_pb + rv, _bl - rv, _flags | IS_BUFFER);
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
		//rcvd.reinit(_pb, _bl, _flags | IS_BUFFER);
		pcd->setRecv(_pb, _bl, _flags | IS_BUFFER);
		pcd->rcvsz = 0;
	}
	return NOK;
}
int Channel::doRecvSecure(char* _pbuf, uint32 _bl, uint32 _flags){
	//TODO:
	return BAD;
}
/*
The function must be used with great care, it will report a failed read from stream
as an unrecoverable error and will return BAD.
Also one cannot schedule the sending of different sections of the same stream.
TODO: evaluate the need of an StreamIterator
*/
int Channel::doSendPlain(IStreamIterator &_rsi, uint64 _sl, char* _pb, uint32 _bl, uint32 _flags){
	if(!_sl) return OK;
	if(!pcd->arePendingSends()){
		idbg("send nopending stream "<<_sl);
		_rsi.start();
		_flags &= ~MUST_START;
		int rv = 0;
		uint toread = _bl;
		if(toread > _sl) toread = _sl;
		while(_sl){
			rv = _rsi->read(_pb, toread);
			if(rv != (int)toread){ 
				idbg("error reading from stream rv = "<<rv<<" toread = "<<toread<<" streamlen = "<<_sl); 
				return BAD;
			}
			_sl -= rv;
			rv = write(sd, _pb, toread);
			if(!rv){idbg("error writing to socket"); return BAD;}
			if(rv == (int)toread){
				if(_sl < toread) toread = _sl;
			}else{
				if(rv < 0){
					if(errno != EAGAIN){idbg("some other socket error"); return BAD;}
					rv = 0;
				}
				idbg("pending stream send "<<_sl);
				//snddq.push(Data(_pb, _bl, _flags & (~IS_BUFFER)));
				//sndsq.push(IStreamPairTp(_rsi, _sl - rv));
				pcd->pushSend(_pb, _sl ? _bl : toread, _flags & (~IS_BUFFER));
				pcd->pushSend(_rsi,_sl);
				pcd->wcnt = rv;
				return NOK;
			}
		}
	}else{
		idbg("send pending stream "<<_sl);
		//snddq.push(Data(_pb, _bl, _flags & (~IS_BUFFER)));
		//sndsq.push(IStreamPairTp(_rsi, _sl));
		pcd->pushSend(_pb, _bl, (_flags & (~IS_BUFFER)) | MUST_START);
		pcd->pushSend(_rsi, _sl);
		return NOK;
	}
	
	return OK;
}
int Channel::doSendSecure(IStreamIterator &_rsi, uint64 _sl, char* _pb, uint32 _bl, uint32 _flags){
	//TODO:
	return BAD;
}
/*
The function must be also used with great care as it will not check for out stream errors.
It will return when _sl octets were read from the socket.
*/
int Channel::doRecvPlain(OStreamIterator &_rsi, uint64 _sl, char* _pb, uint32 _bl, uint32 _flags){
	if(!_sl) return OK;
	assert(!pcd->rdn.b.pb);
	int rv;
	uint toread = _bl;
	pcd->rcvsz = 0;
	if(!(_flags & PREPARE_ONLY)){
		if(toread > _sl) toread = _sl;
		while(_sl){
			rv = read(sd, _pb, toread);
			if(!rv) return BAD;
			if(rv > 0){
				_rsi->write(_pb, rv);
				_sl -= rv;
				if(toread > _sl) toread = _sl;
			}else break;
		}
		if(errno != EAGAIN) return BAD;
	}
	if(!(_flags & TRY_ONLY)){
		//rcvd.reinit(_pb, _bl, _flags & (~IS_BUFFER));
		//rcvs.first = _rsi;
		//rcvs.second = _sl;
		pcd->setRecv(_pb, _bl, _flags & (~IS_BUFFER));
		pcd->setRecv(_rsi, _sl);
	}
	return NOK;
}
int Channel::doRecvSecure(OStreamIterator &_rsi, uint64 _sl, char* _pb, uint32 _bl, uint32 _flags){
	//TODO:
	return BAD;
}

//--- Interface used by ConnectionChannel

int Channel::doRecvPlain(){
	assert(pcd->rdn.b.pb);
	int rv;
	if(pcd->rdn.flags & IS_BUFFER){
		//we've got a buffer
		rv = read(sd, pcd->rdn.b.pb, pcd->rdn.bl);
		if(rv <= 0) return ERRDONE;
		pcd->rcvsz = rv;
	}else{
		pcd->rcvsz = 0;
		//we've got a stream
		uint toread = pcd->rdn.bl;
		if(toread > pcd->rsn.sz) toread = pcd->rsn.sz;
		while(pcd->rsn.sz){
			rv = read(sd, pcd->rdn.b.pb, toread);
			if(!rv) return ERRDONE;
			if(rv > 0){
				pcd->rsn.sit->write(pcd->rdn.b.pb, rv);
				pcd->rsn.sz -= rv;
				if(toread > pcd->rsn.sz) toread = pcd->rsn.sz;
			}else{
				if(errno != EAGAIN) return ERRDONE;
				return 0;
			}
		}
	}
	pcd->rdn.b.pb = NULL; //rcvd.flags = 0;
	return INDONE;
}
int Channel::doRecvSecure(){
	//TODO:
	return BAD;
}
int Channel::doSendPlain(){
	int rv;
	int retv = 0;
	while(pcd->arePendingSends()){
		//idbg("another pending send");
		DataNode	&rdn = *pcd->psdnfirst; //snddq.front();
		if(rdn.flags & IS_BUFFER){
			rv = write(sd, rdn.b.pcb, rdn.bl);
			if(rv == (int)rdn.bl){
				if(rdn.flags & RAISE_ON_END) retv = OUTDONE;
				pcd->popSendData();
				continue;
			}
			if(!rv) return ERRDONE;
			if(rv > 0){
				rdn.b.pcb += rv; rdn.bl -= rv;
			}else{
				if(errno != EAGAIN) return ERRDONE;
			}
			return retv;
		}else{
			//sending a stream
			assert(pcd->pssnfirst);
			rv = write(sd, rdn.b.pcb + pcd->wcnt, rdn.bl - pcd->wcnt);
			if(!rv) return ERRDONE;
			if(rv < 0){
				if(errno != EAGAIN){ idbg("ioerrr");return ERRDONE;}
				assert(false);
			}
			if(rv < (int)(rdn.bl - pcd->wcnt)){
				pcd->wcnt += rv;
				return retv;
			}
			uint toread = rdn.bl;
			IStreamNode	&rs = *pcd->pssnfirst;
			if(rdn.flags & MUST_START){
				if(rs.sit.start() < 0) return ERRDONE;
				rdn.flags &= ~MUST_START;
			}
			if(toread > rs.sz) toread = rs.sz;
			pcd->wcnt = 0;
			while(rs.sz){
				rv = rs.sit->read(rdn.b.pb, toread);
				if(rv != (int)toread){ 
					idbg("ioerrr rv = "<<rv<<" toread = "<<toread<<" wcnt = "<<pcd->wcnt);
					idbg("rs.sz = "<<rs.sz);
					return ERRDONE;
				}
				rs.sz -= rv;
				rv = write(sd, rdn.b.pb, toread);
				if(rv == (int)toread){//written everything
					if(toread > rs.sz) toread = rs.sz;
					continue;
				}
				if(!rv){ idbg("ioerrr");return ERRDONE;};
				if(rv < 0){
					if(errno != EAGAIN){ idbg("ioerrr");return ERRDONE;}
					rv = 0;
				}
				//rv >= 0
				pcd->wcnt += rv;
				rdn.bl = toread;
				//idbg("read stream "<<retv);
				return retv;
			}
			if(rdn.flags & RAISE_ON_END) retv = OUTDONE;
			pcd->popSendData();
			pcd->popSendStream();
		}
	}
	return OUTDONE;
}
int Channel::doSendSecure(){
	//TODO:
	return BAD;
}


}//namespace tcp
}//namespace clientserver
