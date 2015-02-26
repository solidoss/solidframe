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
		
	}
	
	static void on_posted_send_all(ReactorContext &_rctx, Event const&){
		
	}
	static void on_completion(CompletionHandler& _rch, ReactorContext &_rctx){
		ThisT &rthis = static_cast<ThisT&>(_rch);
		
		switch(rthis.reactorEvent(_rctx)){
			case ReactorEventRecv:
				if(!rthis.recv_fnc.empty()){
					SocketDevice	sd;
					RecvFunctionT	tmpf(std::move(rthis.recv_fnc));
					//rthis.doAccept(_rctx, sd);
					
					//tmpf(_rctx, sd);
				}break;
			case ReactorEventSend:
			case ReactorEventRecvSend:
				
			case ReactorEventError:
				if(!rthis.recv_fnc.empty()){
					SocketDevice	sd;
					FunctionT		tmpf(std::move(rthis.recv_fnc));
					//TODO: set err
					tmpf(_rctx, -1);
				}
				if(!rthis.send_fnc.empty()){
					SocketDevice	sd;
					SendFunctionT	tmpf(std::move(rthis.send_fnc));
					//TODO: set err
					tmpf(_rctx);
				}break;
			case ReactorEventClear:
				rthis.f.clear();
				break;
			default:
				cassert(false);
		}
	}
	
	static void on_init_completion(CompletionHandler& _rch, ReactorContext &_rctx){
		ThisT &rthis = static_cast<ThisT&>(_rch);
		rthis.completionCallback(&on_completion);
		rthis.addDevice(_rctx, rthis.sd, ReactorWaitReadWrite);
	}
public:
	Stream(
		ObjectProxy const &_robj, SocketDevice &_rsd
	){}
	
	template <typename F>
	bool postRecvSome(ReactorContext &_rctx, char *_buf, size_t _bufcp, F _f){
		if(recv_fnc.empty()){
			recv_fnc = _f;
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
		return false;
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
		return false;
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
private:
	typedef boost::function<void(ReactorContext&, size_t)>		RecvFunctionT;
	typedef boost::function<void(ReactorContext&)>				SendFunctionT;
	Sock			s;
	
	char 			*recv_buf;
	size_t			recv_buf_cp;
	RecvFunctionT	recv_fnc;
	
	const char		*send_buf;
	size_t			send_buf_sz;
	SendFunctionT	send_fnc;
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif
