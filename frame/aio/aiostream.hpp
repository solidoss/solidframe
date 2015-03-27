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
	typedef Stream<Sock>			ThisT;
	typedef ERROR_NS::error_code	ErrorCodeT;
	
	static void on_init_completion(CompletionHandler& _rch, ReactorContext &_rctx){
		ThisT &rthis = static_cast<ThisT&>(_rch);
		rthis.completionCallback(&on_completion);
		rthis.addDevice(_rctx, rthis.s.device(), ReactorWaitReadOrWrite);
	}
	
	static void on_completion(CompletionHandler& _rch, ReactorContext &_rctx){
		ThisT &rthis = static_cast<ThisT&>(_rch);
		
		switch(rthis.s.filterReactorEvents(rthis.reactorEvent(_rctx))){
			case ReactorEventNone:
				break;
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
				rthis.doClear(_rctx);
				break;
			default:
				cassert(false);
		}
	}
	
	//-------------
	static void on_posted_recv_some(ReactorContext &_rctx, Event const&){
		ThisT	&rthis = static_cast<ThisT&>(*completion_handler(_rctx));
		rthis.doRecv(_rctx);
	}
	
	static void on_posted_send_all(ReactorContext &_rctx, Event const&){
		ThisT	&rthis = static_cast<ThisT&>(*completion_handler(_rctx));
		
		rthis.doSend(_rctx);
	}
	
	static void on_dummy(ThisT &_rthis, ReactorContext &_rctx){
		
	}
	
	//-------------
	template <class F>
	struct RecvSomeFunctor{
		F	f;
		
		RecvSomeFunctor(F &_rf):f(_rf){}
		
		void operator()(ThisT &_rthis, ReactorContext &_rctx){
			if(_rthis.doTryRecv(_rctx)){
				F				tmpf(f);
				const size_t	recv_sz = _rthis.recv_buf_sz;
				_rthis.doClearRecv(_rctx);
				tmpf(_rctx, recv_sz);
			}
		}
	};
	
	template <class F>
	struct SendAllFunctor{
		F	f;
		
		SendAllFunctor(F &_rf):f(_rf){}
		
		void operator()(ThisT &_rthis, ReactorContext &_rctx){
			if(_rthis.doTrySend(_rctx)){
				if(_rthis.send_buf_sz == _rthis.send_buf_cp){
					F	tmpf(f);
					_rthis.doClearSend(_rctx);
					tmpf(_rctx);
				}
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
	
	template <class F>
	struct SecureConnectFunctor{
		F	f;
		
		SecureConnectFunctor(F &_rf):f(_rf){}
		
		void operator()(ThisT &_rthis, ReactorContext &_rctx){
			_rthis.errorClear(_rctx);
			bool		can_retry;
			ErrorCodeT	err;
			if(_rthis.s.secureConnect(can_retry, err)){
			}else if(can_retry){
				return;
			}else{
				_rthis.systemError(_rctx, err);
				_rthis.error(_rctx, ErrorConditionT(-1, _rctx.error().category()));
			}
			F	tmpf(f);
			_rthis.doClearRecv(_rctx);
			tmpf(_rctx);
		}
	};
	
	template <class F>
	struct SecureAcceptFunctor{
		F	f;
		
		SecureAcceptFunctor(F &_rf):f(_rf){}
		
		void operator()(ThisT &_rthis, ReactorContext &_rctx){
			_rthis.errorClear(_rctx);
			bool		can_retry;
			ErrorCodeT	err;
			if(_rthis.s.secureAccept(can_retry, err)){
			}else if(can_retry){
				return;
			}else{
				_rthis.systemError(_rctx, err);
				_rthis.error(_rctx, ErrorConditionT(-1, _rctx.error().category()));
			}
			F	tmpf(f);
			_rthis.doClearRecv(_rctx);
			tmpf(_rctx);
		}
	};
	
	template <class F>
	struct SecureShutdownFunctor{
		F	f;
		
		SecureShutdownFunctor(F &_rf):f(_rf){}
		
		void operator()(ThisT &_rthis, ReactorContext &_rctx){
			_rthis.errorClear(_rctx);
			bool		can_retry;
			ErrorCodeT	err;
			if(_rthis.s.secureShutdown(can_retry, err)){
			}else if(can_retry){
				return;
			}else{
				_rthis.systemError(_rctx, err);
				_rthis.error(_rctx, ErrorConditionT(-1, _rctx.error().category()));
			}
			F	tmpf(f);
			_rthis.doClearSend(_rctx);
			tmpf(_rctx);
		}
	};
	
public:
	explicit Stream(
		ObjectProxy const &_robj, SocketDevice &_rsd
	):CompletionHandler(_robj, on_init_completion), s(_rsd){}
	
	template <class Ctx>
	Stream(
		ObjectProxy const &_robj, SocketDevice &_rsd, Ctx &_rctx
	):CompletionHandler(_robj, on_init_completion), s(_rctx, _rsd){}
	
	Stream(
		ObjectProxy const &_robj
	):CompletionHandler(_robj, on_dummy_completion){}
	
	template <class Ctx>
	explicit Stream(
		ObjectProxy const &_robj, Ctx &_rctx
	):CompletionHandler(_robj, on_dummy_completion), s(_rctx){}
	
	
	bool hasPendingRecv()const{
		return !FUNCTION_EMPTY(recv_fnc);
	}
	
	bool hasPendingSend()const{
		return !FUNCTION_EMPTY(send_fnc);
	}
	
	SocketDevice reset(ReactorContext &_rctx, SocketDevice &_rnewdev = dummy_socket_device()){
		if(s.device().ok()){
			remDevice(_rctx, s.device());
		}
		SocketDevice sd(s.reset(_rnewdev));
		if(s.device().ok()){
			addDevice(_rctx, s.device(), ReactorWaitReadOrWrite);
		}
		return sd;
	} 
	
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
			error(_rctx, ErrorConditionT(-1, _rctx.error().category()));
			return true;
		}
	}
	
	template <typename F>
	bool recvSome(ReactorContext &_rctx, char *_buf, size_t _bufcp, F _f, size_t &_sz){
		if(FUNCTION_EMPTY(recv_fnc)){
			recv_buf = _buf;
			recv_buf_cp = _bufcp;
			recv_buf_sz = 0;
			
			if(doTryRecv(_rctx)){
				return true;
			}else{
				recv_fnc = RecvSomeFunctor<F>(_f);
				return false;
			}
			
		}else{
			//TODO: set proper error
			error(_rctx, ErrorConditionT(-1, _rctx.error().category()));
		}
		return true;
	}
	
	
	template <typename F>
	bool postSendAll(ReactorContext &_rctx, const char *_buf, size_t _bufcp, F _f){
		if(FUNCTION_EMPTY(send_fnc)){
			send_fnc = SendAllFunctor<F>(_f);
			send_buf = _buf;
			send_buf_cp = _bufcp;
			send_buf_sz = 0;
			doPostSendAll(_rctx);
			errorClear(_rctx);
			return false;
		}else{
			//TODO: set proper error
			error(_rctx, ErrorConditionT(-1, _rctx.error().category()));
			cassert(false);
			return true;
		}
	}
	
	template <typename F>
	bool sendAll(ReactorContext &_rctx, char *_buf, size_t _bufcp, F _f){
		if(FUNCTION_EMPTY(send_fnc)){
			send_buf = _buf;
			send_buf_cp = _bufcp;
			send_buf_sz = 0;
			
			if(doTrySend(_rctx)){
				if(send_buf_sz == send_buf_cp){
					return true;
				}else{
					send_fnc = SendAllFunctor<F>(_f);
				}
			}else{
				return false;
			}
			
		}else{
			//TODO: set proper error
			error(_rctx, ErrorConditionT(-1, _rctx.error().category()));
		}
		return true;
	}
	
	template <typename F>
	bool connect(ReactorContext &_rctx, SocketAddressStub const &_rsas, F _f){
		if(FUNCTION_EMPTY(send_fnc)){
			errorClear(_rctx);
			ErrorCodeT	err;
			if(s.create(_rsas, err)){
				completionCallback(&on_completion);
				addDevice(_rctx, s.device(), ReactorWaitReadOrWrite);
				
				bool		can_retry;
				bool		rv = s.connect(_rsas, can_retry, err);
				if(rv){
					
				}else if(can_retry){
					send_fnc = ConnectFunctor<F>(_f);
					return false;
				}else{
					systemError(_rctx, err);
					//TODO: set proper error
					error(_rctx, ErrorConditionT(-1, _rctx.error().category()));
				}
			}else{
				//TODO: set proper error
				error(_rctx, ErrorConditionT(-1, _rctx.error().category()));
				systemError(_rctx, err);
			}
			
		}else{
			//TODO: set proper error
			error(_rctx, ErrorConditionT(-1, _rctx.error().category()));
		}
		return true;
	}
	
	void shutdown(ReactorContext &_rctx){
		s.shutdown();
	}
	
	template <typename F>
	bool secureConnect(ReactorContext &_rctx, F _f){
		if(FUNCTION_EMPTY(send_fnc)){
			errorClear(_rctx);
			bool		can_retry;
			ErrorCodeT	err;
			if(s.secureConnect(can_retry, err)){
			}else if(can_retry){
				send_fnc = SecureConnectFunctor<F>(_f);
				return false;
			}else{
				systemError(_rctx, err);
				error(_rctx, ErrorConditionT(-1, _rctx.error().category()));
			}
		}
		return true;
	}
	
	template <typename F>
	bool secureAccept(ReactorContext &_rctx, F _f){
		if(FUNCTION_EMPTY(recv_fnc)){
			errorClear(_rctx);
			bool		can_retry;
			ErrorCodeT	err;
			if(s.secureAccept(can_retry, err)){
			}else if(can_retry){
				recv_fnc = SecureAcceptFunctor<F>(_f);
				return false;
			}else{
				systemError(_rctx, err);
				error(_rctx, ErrorConditionT(-1, _rctx.error().category()));
			}
		}
		return true;
	}
	
	template <typename F>
	bool secureShutdown(ReactorContext &_rctx, F _f){
		if(FUNCTION_EMPTY(send_fnc)){
			errorClear(_rctx);
			bool		can_retry;
			ErrorCodeT	err;
			if(s.secureShutdown(can_retry, err)){
			}else if(can_retry){
				send_fnc = SecureShutdownFunctor<F>(_f);
				return false;
			}else{
				systemError(_rctx, err);
				error(_rctx, ErrorConditionT(-1, _rctx.error().category()));
			}
		}
		return true;
	}
	
	void secureRenegotiate(ReactorContext &_rctx){
		ErrorCodeT	err = s.renegotiate();
		if(err){
			systemError(_rctx, err);
			error(_rctx, ErrorConditionT(-1, _rctx.error().category()));
		}
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
			errorClear(_rctx);
			recv_fnc(*this, _rctx);
		}
	}
	
	void doSend(ReactorContext &_rctx){
		if(!FUNCTION_EMPTY(send_fnc)){
			errorClear(_rctx);
			send_fnc(*this, _rctx);
		}
	}
	bool doTryRecv(ReactorContext &_rctx){
		bool		can_retry;
		ErrorCodeT	err;
		int			rv = s.recv(recv_buf, recv_buf_cp - recv_buf_sz, can_retry, err);
		if(rv > 0){
			recv_buf_sz += rv;
			recv_buf += rv;
		}else if(rv == 0){
			error(_rctx, ErrorConditionT(-1, _rctx.error().category()));
			recv_buf_sz = recv_buf_cp = 0;
		}else if(rv < 0){
			if(can_retry){
				return false;
			}else{
				recv_buf_sz = recv_buf_cp = 0;
				error(_rctx, ErrorConditionT(-1, _rctx.error().category()));
				systemError(_rctx, err);
			}
		}
		return true;
	}
	
	bool doTrySend(ReactorContext &_rctx){
		bool		can_retry;
		ErrorCodeT	err;
		int			rv = s.send(send_buf, send_buf_cp - send_buf_sz, can_retry, err);
		
		if(rv > 0){
			send_buf_sz += rv;
			send_buf += rv;
		}else if(rv == 0){
			error(_rctx, ErrorConditionT(-1, _rctx.error().category()));
			send_buf_sz = send_buf_cp = 0;
		}else if(rv < 0){
			if(can_retry){
				return false;
			}else{
				send_buf_sz = send_buf_cp = 0;
				error(_rctx, ErrorConditionT(-1, _rctx.error().category()));
				systemError(_rctx, err);
			}
		}
		return true;
	}
	
	void doError(ReactorContext &_rctx){
		edbg("");
		//TODO: set propper error
		error(_rctx, ErrorConditionT(-1, _rctx.error().category()));
		
		if(!FUNCTION_EMPTY(send_fnc)){
			send_buf_sz = send_buf_cp = 0;
			send_fnc(*this, _rctx);
		}
		if(!FUNCTION_EMPTY(recv_fnc)){
			recv_buf_sz = recv_buf_cp = 0;
			recv_fnc(*this, _rctx);
		}
	}
	
	void doClearRecv(ReactorContext &_rctx){
		FUNCTION_CLEAR(recv_fnc);
		recv_buf = nullptr;
		recv_buf_sz = recv_buf_cp = 0;
	}
	
	void doClearSend(ReactorContext &_rctx){
		FUNCTION_CLEAR(send_fnc);
		send_buf = nullptr;
		send_buf_sz = send_buf_cp = 0;
	}
	
	void doClear(ReactorContext &_rctx){
		doClearRecv(_rctx);
		doClearSend(_rctx);
		remDevice(_rctx, s.device());
		recv_fnc = &on_dummy;//we prevent new send/recv calls
		send_fnc = &on_dummy;
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
