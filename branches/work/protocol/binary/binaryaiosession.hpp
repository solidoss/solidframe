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
	int executeRecv(
		frame::aio::SingleObject &_raioobj,
		ulong _evs,
		ConCtx &_rconctx,
		Des &_rdes,
		BufCtl &_rbufctl,
		Com &_rcom
		
	){
		typedef BufCtl BufCtlT;
		if(_evs & frame::ERRDONE){
			idbg("ioerror "<<_evs<<' '<<_raioobj.socketEventsGrab());
			return done();
		}
		if(_evs & frame::INDONE){
			char	tmpbuf[BufCtlT::DataCapacity];
			idbg("receive data of size "<<_raioobj.socketRecvSize());
			if(!BaseT::consume(_rdes, _rconctx, _rbufctl.recvBuffer(), _raioobj.socketRecvSize(), _rcom, tmpbuf, BufCtlT::DataCapacity)){
				return done();
			}
		}
		bool reenter = false;
		if(!_raioobj.socketHasPendingRecv()){
			switch(_raioobj.socketRecv(BaseT::recvBufferOffset(_rbufctl.recvBuffer()), BaseT::recvBufferCapacity(_rbufctl.recvCapacity()))){
				case BAD: return done();
				case OK:{
					char	tmpbuf[BufCtlT::DataCapacity];
					idbg("receive data of size "<<_raioobj.socketRecvSize());
					if(!BaseT::consume(_rdes, _rconctx, _rbufctl.recvBuffer(), _raioobj.socketRecvSize(), _rcom, tmpbuf, BufCtlT::DataCapacity)){
						idbg("error consume");
						return done();
					}
					reenter = true;
				}break;
				default:
					break;
			}
		}
		return reenter ? OK : NOK;
	}
	
	template <class ConCtx, class Ser, class BufCtl, class Com> 
	int executeSend(
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
				idbg("send data of size "<<rv);
				switch(_raioobj.socketSend(_rbufctl.sendBuffer(), rv)){
					case BAD: 
						return done();
					case NOK:
						cnt = 0;
						break;
					default:
						break;
				}
			}
			if(cnt == 0){
				reenter = true;
			}
		}
		
		return reenter ? OK : NOK;
	}
	template <class ConCtx, class Ser, class Des, class BufCtl, class Com> 
	int execute(
		frame::aio::SingleObject &_raioobj,
		ulong _evs,
		ConCtx &_rconctx,
		Ser &_rser,
		Des &_rdes,
		BufCtl &_rbufctl,
		Com &_rcom
		
	){
		const int rcvrv = executeRecv(_raioobj, _evs, _rconctx, _rdes, _rbufctl, _rcom);
		if(rcvrv == BAD) return done();
		const int sndrv = executeSend(_raioobj, _evs, _rconctx, _rser, _rbufctl, _rcom);
		if(sndrv == BAD) return done();
		return (sndrv == OK || rcvrv == OK) ? OK : NOK;
	}

private:
	int done(){
		return BAD;
	}
};

}//namespace binary
}//namepsace protocol
}//namespace solid
#endif
