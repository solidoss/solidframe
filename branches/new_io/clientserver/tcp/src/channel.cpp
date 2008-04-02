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
	if(ok()) close(sd);
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

ulong Channel::yieldRequest()const{
	return pcd->flags & IO_YIELD_FLAGS;
}

int Channel::arePendingSends(){
	return pcd->arePendingSends();
}
//TODO: try to only do connect here
// and create the socket, bind it to a certain interface in 'create'
int Channel::connect(const AddrInfoIterator &_it){
	//in order to make the selector wait for data out, just add an empty buffer to snddq.
	if(ok()){	close(sd);}
	
	sd = socket(_it.family(), _it.type(), _it.protocol());
	if(!ok()) return BAD;
	if(fcntl(sd, F_SETFL, O_NONBLOCK) < 0){
		close(sd);
		sd = -1;
		return BAD;
	}
	int rv = ::connect(sd, _it.addr(), _it.size());
	if(rv < 0){
		if(errno != EINPROGRESS) return BAD;
		rv = NOK;
		pcd->pushSend((const char*) NULL, 0, RAISE_ON_END | IS_BUFFER);
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
			pcd->flags |= OUTTOUT;
		}
	}
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
		pcd->setRecv(_pb, _bl, _flags | IS_BUFFER);
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
		ulong sendsize = STREAM_MAX_WRITE_ONCE;
		if(sendsize > _sl) sendsize = _sl;
		ulong toread = _bl;
		if(toread > sendsize) toread = sendsize;
		_sl -= sendsize;
		while(sendsize){
			rv = _rsi->read(_pb, toread);
			if(rv != (int)toread){ 
				idbg("error reading from stream rv = "<<rv<<" toread = "<<toread<<" streamlen = "<<_sl); 
				return BAD;
			}
			sendsize -= rv;
			rv = write(sd, _pb, toread);
			if(!rv){
				idbg("error writing to socket"); 
				return BAD;
			}else if(rv == (int)toread){
				if(sendsize < toread) toread = sendsize;
			}else{
				if(rv < 0){
					if(errno != EAGAIN){
						idbg("some other socket error");
						return BAD;
					}
					rv = 0;
				}
				pcd->flags |= OUTTOUT;
				_sl += sendsize;
				idbg("pending stream send "<<_sl);
				pcd->pushSend(_pb, _sl ? _bl : toread, _flags & (~IS_BUFFER));
				pcd->pushSend(_rsi,_sl);
				pcd->wcnt = rv;
				return NOK;
			}
		}
		if(_sl){
			//sent only a part of the stream, do a YIELD
			pcd->wcnt = rv;
			pcd->flags |= OUTYIELD;
			pcd->pushSend(_pb, _bl, _flags & (~IS_BUFFER));
			pcd->pushSend(_rsi,_sl);
		}
	}else{
		idbg("send pending stream "<<_sl);
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
	cassert(!pcd->rdn.b.pb);
	int rv;
	ulong toread = _bl;
	ulong recvsize = STREAM_MAX_READ_ONCE;
	if(recvsize > _sl) recvsize = _sl;
	_sl -= recvsize;
	pcd->rcvsz = 0;
	if(!(_flags & PREPARE_ONLY)){
		if(toread > recvsize) toread = recvsize;
		while(recvsize){
			rv = read(sd, _pb, toread);
			if(!rv) return BAD;
			if(rv > 0){
				_rsi->write(_pb, rv);
				recvsize -= rv;
				if(toread > recvsize) toread = recvsize;
			}else{
				if(errno != EAGAIN) return BAD;
				pcd->flags |= INTOUT;
				break;
			}
		}
		_sl += recvsize;
		if(!_sl) return OK;
	}
	if(!(_flags & TRY_ONLY)){
		pcd->setRecv(_pb, _bl, _flags & (~IS_BUFFER));
		pcd->setRecv(_rsi, _sl);
	}
	return NOK;
}
int Channel::doRecvSecure(OStreamIterator &_rsi, uint64 _sl, char* _pb, uint32 _bl, uint32 _flags){
	//TODO:
	return BAD;
}

//--- Interface used by ConnectionSelector

int Channel::doRecvPlain(){
	cassert(pcd->rdn.b.pb);
	int rv;
	pcd->flags &= ~INTOUT;
	if(pcd->rdn.flags & IS_BUFFER){
		//we've got a buffer
		rv = read(sd, pcd->rdn.b.pb, pcd->rdn.bl);
		if(rv <= 0) return ERRDONE;
		pcd->rcvsz = rv;
	}else{
		pcd->rcvsz = 0;
		//we've got a stream
		ulong readsize = STREAM_MAX_READ_ONCE;
		if(readsize > pcd->rsn.sz) readsize = pcd->rsn.sz;
		ulong toread = readsize;
		if(toread > readsize) toread = readsize;
		pcd->rsn.sz -= readsize;
		while(readsize){
			rv = read(sd, pcd->rdn.b.pb, toread);
			if(!rv) return ERRDONE;
			if(rv > 0){
				pcd->rsn.sit->write(pcd->rdn.b.pb, rv);
				readsize -= rv;
				if(toread > readsize) toread = readsize;
			}else{
				if(errno != EAGAIN) return ERRDONE;
				pcd->flags |= INTOUT;
				return 0;
			}
		}
		pcd->rsn.sz += readsize;
		if(pcd->rsn.sz) return INYIELD;
	}
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
	pcd->flags &= ~OUTYIELD;
	while(pcd->arePendingSends()){
		//idbg("another pending send");
		DataNode	&rdn = *pcd->psdnfirst;
		
		pcd->flags &= ~OUTTOUT;
		
		if(rdn.flags & IS_BUFFER){
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
		}else{
			//sending a stream
			cassert(pcd->pssnfirst);
			idbg("write ptr "<<(void*)(rdn.b.pcb + pcd->wcnt)<<" wcnt = "<<pcd->wcnt<<" bl = "<<rdn.bl<<" len = "<<rdn.bl - pcd->wcnt);
			{
				ulong towrite = rdn.bl - pcd->wcnt;
				if(towrite){
					rv = write(sd, rdn.b.pcb + pcd->wcnt, towrite);
				
					if(!rv){
						idbg("ioerr");
						return ERRDONE;
					}
				}
				if(rv < 0){
					if(errno != EAGAIN){
						idbg("ioerr "<<strerror(errno));
						return ERRDONE;
					}
					cassert(false);
				}
				
				if(rv < (int)towrite){
					pcd->wcnt += rv;
					return retv;
				}
			}
			IStreamNode	&rs = *pcd->pssnfirst;
			
			if(rdn.flags & MUST_START){
				if(rs.sit.start() < 0) return ERRDONE;
				rdn.flags &= ~MUST_START;
			}
			
			ulong sendsize = STREAM_MAX_WRITE_ONCE;
			if(sendsize > rs.sz) sendsize = rs.sz;
			
			ulong toread = rdn.bl;
			if(toread > sendsize) toread = sendsize;
			
			rs.sz -= sendsize;
			
			pcd->wcnt = 0;
			
			while(sendsize){
				rv = rs.sit->read(rdn.b.pb, toread);
				
				if(rv != (int)toread){ 
					idbg("ioerrr rv = "<<rv<<" toread = "<<toread<<" wcnt = "<<pcd->wcnt);
					idbg("rs.sz = "<<rs.sz);
					return ERRDONE;
				}
				
				sendsize -= rv;
				
				rv = write(sd, rdn.b.pb, toread);
				
				if(rv == (int)toread){//written everything
					if(toread > sendsize) toread = sendsize;
					continue;
				}
				
				if(!rv){
					idbg("ioerrr");
					return ERRDONE;
				}
				
				if(rv < 0){
					if(errno != EAGAIN){
						idbg("ioerrr");
						return ERRDONE;
					}
					rv = 0;
					pcd->flags |= OUTTOUT;
				}
				//rv >= 0
				pcd->wcnt = rv;
				rdn.bl = toread;
				idbg("otout");
				rs.sz += sendsize;
				return retv;
			}//while
			if(rs.sz){
				pcd->flags |= OUTYIELD;
				pcd->wcnt = rv;
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
