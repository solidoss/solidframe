// frame/aio/aiodatagram.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_DATAGRAM_HPP
#define SOLID_FRAME_AIO_DATAGRAM_HPP

#include "system/common.hpp"
#include "system/socketdevice.hpp"

#include "aiocompletion.hpp"

namespace solid{
namespace frame{
namespace aio{

struct	ObjectProxy;
struct	ReactorContex;

template <class Sock>
class Datagram: public CompletionHandler{
	typedef Datagram<Sock>	ThisT;
	
	static void on_init_completion(CompletionHandler& _rch, ReactorContext &_rctx){
		ThisT &rthis = static_cast<ThisT&>(_rch);
		rthis.completionCallback(&on_completion);
		rthis.addDevice(_rctx, rthis.s.device(), ReactorWaitReadOrWrite);
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
	
	static void on_connect_completion(CompletionHandler& _rch, ReactorContext &_rctx){
		ThisT &rthis = static_cast<ThisT&>(_rch);
		
		switch(rthis.s.filterReactorEvents(rthis.reactorEvent(_rctx), !FUNCTION_EMPTY(rthis.recv_fnc), !FUNCTION_EMPTY(rthis.send_fnc))){
			case ReactorEventSend:
			case ReactorEventRecvSend:
				rthis.doConnect(_rctx);
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
	//-----------
	static void on_posted_recv(ReactorContext &_rctx, Event const&){
		ThisT	&rthis = static_cast<ThisT&>(*completion_handler(_rctx));
		rthis.doRecv(_rctx);
	}
	
	static void on_posted_send(ReactorContext &_rctx, Event const&){
		ThisT	&rthis = static_cast<ThisT&>(*completion_handler(_rctx));
		
		rthis.doSend(_rctx);
	}
	static void on_dummy(ThisT &_rthis, ReactorContext &_rctx){
		
	}
	//-----------
	template <class F>
	struct RecvFunctor{
		F	f;
		
		RecvFunctor(F &_rf):f(_rf){}
		
		void operator()(ThisT &_rthis, ReactorContext &_rctx){
			size_t	recv_sz = 0;
			
			if(!_rctx.error()){
				bool	can_retry;
				int		rv = _rthis.s.recv(_rthis.recv_buf, _rthis.recv_buf_cp, can_retry);
				
				if(rv > 0){
					recv_sz = rv;
				}else if(rv == 0){
					_rthis.error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
				}else if(rv == -1){
					if(can_retry){
						return;
					}else{
						_rthis.error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
					}
				}
			}
			
			F				tmpf(f);
			
			_rthis.doClearRecv(_rctx);
			tmpf(_rctx, recv_sz);
		}
	};
	
	template <class F>
	struct RecvFromFunctor{
		F	f;
		
		RecvFromFunctor(F &_rf):f(_rf){}
		
		void operator()(ThisT &_rthis, ReactorContext &_rctx){
			size_t			recv_sz = 0;
			SocketAddress	addr;
			
			if(!_rctx.error()){
				bool			can_retry;
				int				rv = _rthis.s.recvFrom(_rthis.recv_buf, _rthis.recv_buf_cp, addr, can_retry);
				
				if(rv > 0){
					recv_sz = rv;
				}else if(rv == 0){
					_rthis.error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
				}else if(rv == -1){
					if(can_retry){
						return;
					}else{
						_rthis.error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
					}
				}
			}
			
			F				tmpf(f);
			
			_rthis.doClearRecv(_rctx);
			tmpf(_rctx, addr, recv_sz);
		}
	};
	
	template <class F>
	struct SendFunctor{
		F	f;
		
		SendFunctor(F &_rf):f(_rf){}
		
		void operator()(ThisT &_rthis, ReactorContext &_rctx){
			if(!_rctx.error()){
				bool	can_retry;
				int		rv = _rthis.s.send(_rthis.send_buf, _rthis.send_buf_cp, can_retry);
				
				if(rv == _rthis.send_buf_cp){
				}else if(rv >= 0){
					_rthis.error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
				}else if(rv == -1){
					if(can_retry){
						return;
					}else{
						_rthis.error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
					}
				}
			}
			
			F	tmpf(f);
			_rthis.doClearSend(_rctx);
			tmpf(_rctx);
		}
	};
	
	template <class F>
	struct SendToFunctor{
		F	f;
		
		SendToFunctor(F &_rf):f(_rf){}
		
		void operator()(ThisT &_rthis, ReactorContext &_rctx){
			if(!_rctx.error()){
				bool	can_retry;
				int		rv = _rthis.s.sendTo(_rthis.send_buf, _rthis.send_buf_cp, _rthis.send_addr, can_retry);
				
				if(rv == _rthis.send_buf_cp){
				}else if(rv >= 0){
					_rthis.error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
				}else if(rv == -1){
					if(can_retry){
						return;
					}else{
						_rthis.error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
					}
				}
			}
			
			F	tmpf(f);
			_rthis.doClearSend(_rctx);
			tmpf(_rctx);
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
	Datagram(
		ObjectProxy const &_robj, SocketDevice &_rsd
	):CompletionHandler(_robj, on_init_completion), s(_rsd){}
	
	Datagram(
		ObjectProxy const &_robj
	):CompletionHandler(_robj, on_dummy_completion){}
	
	
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
	
	template <typename F>
	bool postRecvFrom(
		ReactorContext &_rctx,
		char *_buf, size_t _bufcp,
		F _f
	){
		if(FUNCTION_EMPTY(recv_fnc)){
			recv_fnc = RecvFromFunctor<F>(_f);
			recv_buf = _buf;
			recv_buf_cp = _bufcp;
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
	bool postRecv(
		ReactorContext &_rctx,
		char *_buf, size_t _bufcp,
		F _f
	){
		if(FUNCTION_EMPTY(recv_fnc)){
			recv_fnc = RecvFunctor<F>(_f);
			recv_buf = _buf;
			recv_buf_cp = _bufcp;
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
	bool recvFrom(
		ReactorContext &_rctx,
		char *_buf, size_t _bufcp,
		F _f,
		SocketAddress &_raddr,
		size_t &_sz
	){
		if(FUNCTION_EMPTY(recv_fnc)){
			bool	can_retry;
			int		rv = s.recvFrom(_buf, _bufcp, _raddr, can_retry);
			if(rv == _bufcp){
				_sz = rv;
				errorClear(_rctx);
			}else if(rv >= 0){
				error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
				_sz = 0;
			}else if(rv == -1){
				_sz = 0;
				if(can_retry){
					recv_buf = _buf;
					recv_buf_cp = _bufcp;
					recv_fnc = RecvFromFunctor<F>(_f);
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
	bool recv(
		ReactorContext &_rctx,
		char *_buf, size_t _bufcp,
		F _f,
		size_t &_sz
	){
		if(FUNCTION_EMPTY(recv_fnc)){
			bool	can_retry;
			int		rv = s.recv(_buf, _bufcp, can_retry);
			if(rv == _bufcp){
				_sz = rv;
				errorClear(_rctx);
			}else if(rv >= 0){
				error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
				_sz = 0;
			}else if(rv == -1){
				_sz = 0;
				if(can_retry){
					recv_buf = _buf;
					recv_buf_cp = _bufcp;
					recv_fnc = RecvFunctor<F>(_f);
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
	bool postSendTo(
		ReactorContext &_rctx,
		const char *_buf, size_t _bufcp,
		SocketAddressStub const &_addrstub,
		F _f
	){
		if(FUNCTION_EMPTY(send_fnc)){
			send_fnc = SendToFunctor<F>(_f);
			send_buf = _buf;
			send_buf_cp = _bufcp;
			send_addr = _addrstub;
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
	bool postSend(
		ReactorContext &_rctx,
		const char *_buf, size_t _bufcp,
		F _f
	){
		if(FUNCTION_EMPTY(send_fnc)){
			send_fnc = SendFunctor<F>(_f);
			send_buf = _buf;
			send_buf_cp = _bufcp;
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
	bool sendTo(
		ReactorContext &_rctx,
		const char *_buf, size_t _bufcp,
		SocketAddressStub const &_addrstub,
		F _f
	){
		if(FUNCTION_EMPTY(send_fnc)){
			bool	can_retry;
			int		rv = s.sendTo(_buf, _bufcp, _addrstub, can_retry);
			
			if(rv == _bufcp){
				errorClear(_rctx);
			}else if(rv >= 0){
				error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
			}else if(rv == -1){
				if(can_retry){
					send_buf = _buf;
					send_buf_cp = _bufcp;
					send_addr = _addrstub;
					send_fnc = SendToFunctor<F>(_f);
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
	bool send(
		ReactorContext &_rctx,
		const char *_buf, size_t _bufcp,
		F _f,
		size_t &_sz
	){
		if(FUNCTION_EMPTY(send_fnc)){
			bool	can_retry;
			int		rv = s.sendTo(_buf, _bufcp, can_retry);
			
			if(rv == _bufcp){
				errorClear(_rctx);
			}else if(rv >= 0){
				error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
			}else if(rv == -1){
				if(can_retry){
					send_buf = _buf;
					send_buf_cp = _bufcp;
					send_fnc = SendFunctor<F>(_f);
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
private:
	void doPostRecvSome(ReactorContext &_rctx){
		EventFunctionT	evfn(&on_posted_recv);
		reactor(_rctx).post(_rctx, evfn, Event(), *this);
	}
	void doPostSendAll(ReactorContext &_rctx){
		EventFunctionT	evfn(&on_posted_send);
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
	
	
	void doError(ReactorContext &_rctx){
		edbg("");
		//TODO: set propper error
		error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
		
		if(!FUNCTION_EMPTY(send_fnc)){
			send_fnc(*this, _rctx);
		}
		if(!FUNCTION_EMPTY(recv_fnc)){
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
		recv_buf_cp = 0;
	}
	
	void doClearSend(ReactorContext &_rctx){
		FUNCTION_CLEAR(send_fnc);
		send_buf = nullptr;
		recv_buf_cp = 0;
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
	
	char 				*recv_buf;
	size_t				recv_buf_cp;
	RecvFunctionT		recv_fnc;
	
	const char			*send_buf;
	size_t				send_buf_cp;
	SendFunctionT		send_fnc;
	SocketAddress		send_addr;
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif
