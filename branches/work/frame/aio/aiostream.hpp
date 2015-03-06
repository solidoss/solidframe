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
#include "system/debug.hpp"

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
		
		switch(rthis.s.filterReactorEvents(rthis.reactorEvent(_rctx), !FUNCTION_EMPTY(rthis.recv_fnc), !FUNCTION_EMPTY(rthis.send_fnc))){
			case ReactorEventRecv:
				rthis.doRecv(_rctx);
				break;
			case ReactorEventSend:
				rthis.doSend(_rctx);
				break;
			case ReactorEventRecvSend:
				rthis.doRecv(_rctx);
				rthis.doTrySend(_rctx);
				break;
			case ReactorEventSendRecv:
				rthis.doSend(_rctx);
				rthis.doTryRecv(_rctx);
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
	
	static void on_connect_completion(CompletionHandler& _rch, ReactorContext &_rctx){
		ThisT &rthis = static_cast<ThisT&>(_rch);
		
		switch(rthis.s.filterReactorEvents(rthis.reactorEvent(_rctx), !FUNCTION_EMPTY(rthis.recv_fnc), !FUNCTION_EMPTY(rthis.send_fnc))){
			case ReactorEventSend:
				rthis.doConnect(_rctx);
				break;
			case ReactorEventRecvSend:
				rthis.doConnect(_rctx);
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
	
	template <class F>
	struct ConnectFunctor{
		F	f;
		
		ConnectFunctor(F &_rf):f(_rf){}
		
		void operator()(ThisT &_rthis, ReactorContext &_rctx){
			F	tmpf(f);
			_rthis.doClearSend(_rctx);
			tmpf(_rctx);
		}
	};
public:
	Stream(
		ObjectProxy const &_robj, SocketDevice &_rsd
	):CompletionHandler(_robj, on_init_completion), s(_rsd){}
	
	Stream(
		ObjectProxy const &_robj
	):CompletionHandler(_robj, on_dummy_completion){}
	
	template <typename F>
	bool postRecvSome(ReactorContext &_rctx, char *_buf, size_t _bufcp, F _f){
		if(FUNCTION_EMPTY(recv_fnc)){
			recv_fnc = RecvSomeFunctor<F>(_f);
			recv_buf = _buf;
			recv_buf_cp = _bufcp;
			recv_buf_sz = 0;
			doPostRecvSome(_rctx);
			errorClear(_rctx);
			return false;
		}else{
			//TODO: set proper error
			error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
			return true;
		}
	}
	
	template <typename F>
	bool recvSome(ReactorContext &_rctx, char *_buf, size_t _bufcp, F _f, size_t &_sz){
		if(FUNCTION_EMPTY(recv_fnc)){
			bool	can_retry;
			int		rv = s.recv(_buf, _bufcp, can_retry);
			if(rv > 0){
				_sz = rv;
				errorClear(_rctx);
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
					errorClear(_rctx);
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
		if(FUNCTION_EMPTY(send_fnc)){
			send_fnc = _f;
			send_buf = _buf;
			send_buf_cp = _bufcp;
			send_buf_sz = 0;
			doPostSendAll(_rctx);
			errorClear(_rctx);
			return false;
		}else{
			//TODO: set proper error
			error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
			cassert(false);
			return true;
		}
	}
	
	template <typename F>
	bool sendAll(ReactorContext &_rctx, char *_buf, size_t _bufcp, F _f){
		if(FUNCTION_EMPTY(send_fnc)){
			bool	can_retry;
			int		rv = s.send(_buf, _bufcp, can_retry);
			if(rv > 0){
				_bufcp -= rv;
				errorClear(_rctx);
				if(_bufcp){
					send_buf = _buf + rv;
					send_buf_cp = _bufcp;
					send_buf_sz = 0;
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
					errorClear(_rctx);
					return false;
				}else{
					//TODO: set proper error
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
	bool connect(ReactorContext &_rctx, SocketAddressStub const &_rsas, F _f){
		if(FUNCTION_EMPTY(send_fnc)){
			errorClear(_rctx);
			if(s.create(_rsas)){
				completionCallback(&on_completion);
				addDevice(_rctx, s.device(), ReactorWaitReadOrWrite);
				
				bool	can_retry;
				bool	rv = s.connect(_rsas, can_retry);
				if(rv){
					
				}else if(can_retry){
					completionCallback(&on_connect_completion);
					send_fnc = ConnectFunctor<F>(_f);
					return false;
				}else{
					//TODO: set proper error
					systemError(_rctx, specific_error_back());
					error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
				}
			}else{
				//TODO: set proper error
				error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
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
		if(!FUNCTION_EMPTY(recv_fnc)){
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
	
	void doTryRecv(ReactorContext &_rctx){
		if(!FUNCTION_EMPTY(recv_fnc)){
			bool	can_retry;
			int		rv = s.recv(recv_buf, recv_buf_cp - recv_buf_sz, can_retry);
			if(rv > 0){
				recv_buf_sz += rv;
				recv_buf += rv;
			}else if(rv == 0){
				error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
				recv_buf_sz = recv_buf_cp = 0;
				
			}else if(rv == -1){
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
	
	void doSend(ReactorContext &_rctx){
		if(!FUNCTION_EMPTY(send_fnc)){
			bool	can_retry;
			int		rv = s.send(send_buf, send_buf_cp - send_buf_sz, can_retry);
			if(rv > 0){
				send_buf_sz += rv;
				send_buf += rv;
			}else if(rv == 0){
				edbg("");
				send_buf_sz = send_buf_cp = 0;
				error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
			}else if(rv < 0){
				edbg(""<<can_retry);
				send_buf_sz = send_buf_cp = 0;
				error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
			}
			send_fnc(*this, _rctx);
		}
	}
	
	void doTrySend(ReactorContext &_rctx){
		if(!FUNCTION_EMPTY(send_fnc)){
			bool	can_retry;
			int		rv = s.send(send_buf, send_buf_cp - send_buf_sz, can_retry);
			
			if(rv > 0){
				send_buf_sz += rv;
				send_buf += rv;
			}else if(rv == 0){
				error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
				send_buf_sz = send_buf_cp = 0;
				
			}else if(rv == -1){
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
		edbg("");
		//TODO: set propper error
		error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
		
		if(!FUNCTION_EMPTY(send_fnc)){
			send_buf_sz = send_buf_cp = 0;
			send_fnc(*this, _rctx);
		}
		if(!FUNCTION_EMPTY(recv_fnc)){
			recv_buf_sz = recv_buf_cp = 0;
			recv_fnc(*this, _rctx);
		}
	}
	
	void doConnect(ReactorContext &_rctx){
		cassert(!FUNCTION_EMPTY(send_fnc));
		completionCallback(&on_completion);
		send_fnc(*this, _rctx);
	}
	
	void doClearRecv(ReactorContext &_rctx){
		FUNCTION_CLEAR(recv_fnc);
		recv_buf = nullptr;
		recv_buf_sz = 0;
		recv_buf_cp = 0;
	}
	
	void doClearSend(ReactorContext &_rctx){
		FUNCTION_CLEAR(send_fnc);
		send_buf = nullptr;
		send_buf_sz = 0;
		recv_buf_cp = 0;
	}
private:
	typedef FUNCTION<void(ThisT&, ReactorContext&)>		RecvFunctionT;
	typedef FUNCTION<void(ThisT&, ReactorContext&)>		SendFunctionT;
	
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
