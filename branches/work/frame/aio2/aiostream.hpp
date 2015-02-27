// frame/aio/aiostream.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_STREAM_HPP
#define SOLID_FRAME_AIO_STREAM_HPP

#include "system/common.hpp"
#include "system/socketdevice.hpp"
#include "aiocompletion.hpp"

namespace solid{
namespace frame{
namespace aio{

struct	ObjectProxy;
struct	ReactorContext;

template <class Sock>
class Stream: public CompletionHandler{
	typedef Stream<Sock>	ThisT;
	
	static void on_posted_recv_some(ReactorContext &_rctx, Event const&){
		ThisT	&rthis = static_cast<ThisT&>(*completion_handler(_rctx));
		
		rthis.doTryRecv(_rctx);
	}
	
	static void on_posted_send_all(ReactorContext &_rctx, Event const&){
		ThisT	&rthis = static_cast<ThisT&>(*completion_handler(_rctx));
		
		rthis.doTrySend(_rctx);
	}
	static void on_completion(CompletionHandler& _rch, ReactorContext &_rctx){
		ThisT &rthis = static_cast<ThisT&>(_rch);
		
		switch(rthis.s.filterReactorEvents(rthis.reactorEvent(_rctx), !rthis.recv_fnc.empty(), !rthis.send_fnc.empty())){
			case ReactorEventRecv:
				rthis.doRecv(_rctx);
				break;
			case ReactorEventSend:
				rthis.doSend(_rctx);
				break;
			case ReactorEventRecvSend:
				rthis.doRecv(_rctx);
				rthis.doSend(_rctx);
				break;
			case ReactorEventSendRecv:
				rthis.doSend(_rctx);
				rthis.doRecv(_rctx);
				break;
			case ReactorEventHangup:
			case ReactorEventError:
				rthis.doError(_rctx);
				break;
			case ReactorEventClear:
				rthis.doClearRecv(_rctx);
				rthis.doClearSend(_rctx);
				break;
			default:
				cassert(false);
		}
	}
	
	static void on_init_completion(CompletionHandler& _rch, ReactorContext &_rctx){
		ThisT &rthis = static_cast<ThisT&>(_rch);
		rthis.completionCallback(&on_completion);
		rthis.addDevice(_rctx, rthis.s.device(), ReactorWaitReadOrWrite);
	}
	
	template <class F>
	struct RecvSomeFunctor{
		F	f;
		
		RecvSomeFunctor(F &_rf):f(_rf){}
		
		void operator()(ThisT &_rthis, ReactorContext &_rctx){
			F				tmpf(f);
			const size_t	recv_sz = _rthis.recv_buf_sz;
			_rthis.doClearRecv(_rctx);
			tmpf(_rctx, recv_sz);
		}
	};
	
	template <class F>
	struct SendAllFunctor{
		F	f;
		
		SendAllFunctor(F &_rf):f(_rf){}
		
		void operator()(ThisT &_rthis, ReactorContext &_rctx){
			if(_rthis.send_buf_sz == _rthis.send_buf_cp){
				F	tmpf(f);
				_rthis.doClearSend(_rctx);
				tmpf(_rctx);
			}
		}
	};
public:
	Stream(
		ObjectProxy const &_robj, SocketDevice &_rsd
	):CompletionHandler(_robj, on_init_completion), s(_rsd){}
	
	template <typename F>
	bool postRecvSome(ReactorContext &_rctx, char *_buf, size_t _bufcp, F _f){
		if(recv_fnc.empty()){
			recv_fnc = RecvSomeFunctor<F>(_f);
			recv_buf = _buf;
			recv_buf_cp = _bufcp;
			doPostRecvSome(_rctx);
			return false;
		}else{
			//TODO: set proper error
			error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
			return true;
		}
	}
	
	template <typename F>
	bool recvSome(ReactorContext &_rctx, char *_buf, size_t _bufcp, F _f, size_t &_sz){
		if(recv_fnc.empty()){
			bool	can_retry;
			int		rv = s.recv(_buf, _bufcp, can_retry);
			if(rv > 0){
				_sz = rv;
			}else if(rv == 0){
				error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
				_sz = 0;
			}else if(rv == -1){
				_sz = 0;
				if(can_retry){
					recv_buf = _buf;
					recv_buf_cp = _bufcp;
					recv_buf_sz = 0;
					recv_fnc = RecvSomeFunctor<F>(_f);
					return false;
				}else{
					error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
				}
			}
		}else{
			//TODO: set proper error
			error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
		}
		return true;
	}
	
	
	template <typename F>
	bool postSendAll(ReactorContext &_rctx, const char *_buf, size_t _bufcp, F _f){
		if(send_fnc.empty()){
			send_fnc = _f;
			send_buf = _buf;
			send_buf_sz = _bufcp;
			doPostSendAll(_rctx);
			return false;
		}else{
			//TODO: set proper error
			error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
			return true;
		}
	}
	
	template <typename F>
	bool sendAll(ReactorContext &_rctx, char *_buf, size_t _bufcp, F _f){
		if(send_fnc.empty()){
			bool	can_retry;
			int		rv = s.send(_buf, _bufcp, can_retry);
			if(rv > 0){
				_bufcp -= rv;
				if(_bufcp){
					send_buf = _buf + rv;
					send_buf_cp = _bufcp;
					send_buf_sz = rv;
					send_fnc = SendAllFunctor<F>(_f);
					return false;
				}
			}else if(rv == 0){
				error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
			}else if(rv == -1){
				if(can_retry){
					send_buf = _buf;
					send_buf_cp = _bufcp;
					send_buf_sz = 0;
					send_fnc = SendAllFunctor<F>(_f);
					return false;
				}else{
					error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
				}
			}
		}else{
			//TODO: set proper error
			error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
		}
		return true;
	}
private:
	void doPostRecvSome(ReactorContext &_rctx){
		EventFunctionT	evfn(&on_posted_recv_some);
		reactor(_rctx).post(_rctx, evfn, Event(), *this);
	}
	void doPostSendAll(ReactorContext &_rctx){
		EventFunctionT	evfn(&on_posted_send_all);
		reactor(_rctx).post(_rctx, evfn, Event(), *this);
	}
	
	void doRecv(ReactorContext &_rctx){
		if(!recv_fnc.empty()){
			bool	can_retry;
			int		rv = s.recv(recv_buf, recv_buf_cp - recv_buf_sz, can_retry);
			if(rv > 0){
				recv_buf_sz += rv;
				recv_buf += rv;
			}else if(rv == 0){
				recv_buf_sz = recv_buf_cp = 0;
				error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
			}else if(rv < 0){
				recv_buf_sz = recv_buf_cp = 0;
				error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
			}
			recv_fnc(*this, _rctx);
		}
	}
	
	void doSend(ReactorContext &_rctx){
		if(!send_fnc.empty()){
			bool	can_retry;
			int		rv = s.send(send_buf, send_buf_cp - send_buf_sz, can_retry);
			if(rv > 0){
				send_buf_sz += rv;
				send_buf += rv;
			}else if(rv == 0){
				send_buf_sz = send_buf_cp = 0;
				error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
			}else if(rv < 0){
				send_buf_sz = send_buf_cp = 0;
				error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
			}
			send_fnc(*this, _rctx);
		}
	}
	
	void doTryRecv(ReactorContext &_rctx){
		if(!recv_fnc.empty()){
			bool	can_retry;
			int		rv = s.recv(recv_buf, recv_buf_cp, can_retry);
			
			if(rv > 0){
				recv_buf_sz += rv;
				recv_buf += rv;
			}else if(rv == 0){
				error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
				recv_buf_sz = recv_buf_cp = 0;
				
			}else if(rv == -1){
				recv_buf_sz = 0;
				if(can_retry){
					return;
				}else{
					recv_buf_sz = recv_buf_cp = 0;
					error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
				}
			}
			recv_fnc(*this, _rctx);
		}
	}
	void doTrySend(ReactorContext &_rctx){
		if(!send_fnc.empty()){
			bool	can_retry;
			int		rv = s.send(send_buf, send_buf_cp - send_buf_sz, can_retry);
			
			if(rv > 0){
				send_buf_sz += rv;
			}else if(rv == 0){
				error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
				send_buf_sz = send_buf_cp = 0;
				
			}else if(rv == -1){
				send_buf_sz = 0;
				if(can_retry){
					return;
				}else{
					send_buf_sz = send_buf_cp = 0;
					error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
				}
			}
			send_fnc(*this, _rctx);
		}
	}
	void doError(ReactorContext &_rctx){
		//TODO: set propper error
		error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
		
		if(!send_fnc.empty()){
			recv_buf_sz = recv_buf_cp = 0;
			send_fnc(*this, _rctx);
		}
		if(!recv_fnc.empty()){
			send_buf_sz = send_buf_cp = 0;
			recv_fnc(*this, _rctx);
		}
	}
	
	void doClearRecv(ReactorContext &_rctx){
		recv_fnc.clear();
		recv_buf = nullptr;
		recv_buf_sz = 0;
		recv_buf_cp = 0;
	}
	
	void doClearSend(ReactorContext &_rctx){
		send_fnc.clear();
		send_buf = nullptr;
		send_buf_sz = 0;
		recv_buf_cp = 0;
	}
	
private:
	typedef boost::function<void(ThisT&, ReactorContext&)>		RecvFunctionT;
	typedef boost::function<void(ThisT&, ReactorContext&)>		SendFunctionT;
	Sock			s;
	
	char 			*recv_buf;
	size_t			recv_buf_sz;
	size_t			recv_buf_cp;
	RecvFunctionT	recv_fnc;
	
	const char		*send_buf;
	size_t			send_buf_sz;
	size_t			send_buf_cp;
	SendFunctionT	send_fnc;
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif
