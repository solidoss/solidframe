// protocol/binary/binaryaiosession.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_PROTOCOL_BINARY_AIO_SESSION_HPP
#define SOLID_PROTOCOL_BINARY_AIO_SESSION_HPP

#include "protocol/binary/binarysession.hpp"
#include "frame/aio/aiosingleobject.hpp"

namespace solid{
namespace protocol{
namespace binary{

template <class Msg, class MsgCtx, class Ctl = BasicController>
class AioSession: public binary::Session<Msg, MsgCtx, Ctl>{
	typedef binary::Session<Msg, MsgCtx, Ctl>	BaseT;
public:
	AioSession(){}
	
	template <class T>
	AioSession(T &_rt):BaseT(_rt){
		
	}
	template <class ConCtx, class Des, class BufCtl, class Com> 
	AsyncE executeRecv(
		frame::aio::SingleObject &_raioobj,
		ulong _evs,
		ConCtx &_rconctx,
		Des &_rdes,
		BufCtl &_rbufctl,
		Com &_rcom
		
	){
		typedef BufCtl BufCtlT;
		if(_evs & frame::EventDoneError){
			idbgx(Debug::proto_bin, "ioerror "<<_evs<<' '<<_raioobj.socketEventsGrab());
			return done();
		}
		if(_evs & frame::EventDoneRecv){
			char	tmpbuf[BufCtlT::DataCapacity];
			idbgx(Debug::proto_bin, "receive data of size "<<_raioobj.socketRecvSize());
			this->ctl.onRecv(_rconctx, _raioobj.socketRecvSize());
			if(!BaseT::consume(_rdes, _rconctx, _rbufctl.recvBuffer(), _raioobj.socketRecvSize(), _rcom, tmpbuf, BufCtlT::DataCapacity)){
				return done();
			}
		}
		bool reenter = false;
		if(!_raioobj.socketHasPendingRecv()){
			switch(_raioobj.socketRecv(BaseT::recvBufferOffset(_rbufctl.recvBuffer()), BaseT::recvBufferCapacity(_rbufctl.recvCapacity()))){
				case frame::aio::AsyncSuccess:{
					char	tmpbuf[BufCtlT::DataCapacity];
					idbgx(Debug::proto_bin, "receive data of size "<<_raioobj.socketRecvSize());
					this->ctl.onRecv(_rconctx, _raioobj.socketRecvSize());
					if(!BaseT::consume(_rdes, _rconctx, _rbufctl.recvBuffer(), _raioobj.socketRecvSize(), _rcom, tmpbuf, BufCtlT::DataCapacity)){
						idbgx(Debug::proto_bin, "error consume");
						return done();
					}
					reenter = true;
				}break;
				case frame::aio::AsyncError: return done();
				default:
					break;
			}
		}
		return reenter ? AsyncSuccess : AsyncWait;
	}
	
	template <class ConCtx, class Ser, class BufCtl, class Com> 
	AsyncE executeSend(
		frame::aio::SingleObject &_raioobj,
		ulong _evs,
		ConCtx &_rconctx,
		Ser &_rser,
		BufCtl &_rbufctl,
		Com &_rcom
		
	){
		typedef BufCtl BufCtlT;
		bool reenter = false;
		if(!_raioobj.socketHasPendingSend()){
			int		cnt = 8;
			char	tmpbuf[BufCtlT::DataCapacity];
			while((cnt--) > 0){
				int rv = BaseT::fill(_rser, _rconctx, _rbufctl.sendBuffer(), _rbufctl.sendCapacity(), _rcom, tmpbuf, BufCtlT::DataCapacity);
				if(rv < 0) return done();
				if(rv == 0){
					_rbufctl.clearSend();//release the buffer as we have nothing to send
					break;
				}
				idbgx(Debug::proto_bin, "send data of size "<<rv);
				this->ctl.onSend(_rconctx, rv);
				switch(_raioobj.socketSend(_rbufctl.sendBuffer(), rv)){
					case frame::aio::AsyncWait:
						cnt = 0;
						break;
					case frame::aio::AsyncError: 
						return done();
					default:
						break;
				}
			}
			if(cnt == 0){
				reenter = true;
			}
		}
		
		return reenter ? AsyncSuccess : AsyncWait;
	}
	template <class ConCtx, class Ser, class Des, class BufCtl, class Com> 
	AsyncE execute(
		frame::aio::SingleObject &_raioobj,
		ulong _evs,
		ConCtx &_rconctx,
		Ser &_rser,
		Des &_rdes,
		BufCtl &_rbufctl,
		Com &_rcom
		
	){
		const AsyncE rcvrv = executeRecv(_raioobj, _evs, _rconctx, _rdes, _rbufctl, _rcom);
		if(rcvrv == AsyncError) return done();
		const AsyncE sndrv = executeSend(_raioobj, _evs, _rconctx, _rser, _rbufctl, _rcom);
		if(sndrv == AsyncError) return done();
		return (sndrv == AsyncSuccess || rcvrv == AsyncSuccess) ? AsyncSuccess : AsyncWait;
	}

private:
	AsyncE done(){
		return AsyncError;
	}
};

}//namespace binary
}//namepsace protocol
}//namespace solid
#endif
