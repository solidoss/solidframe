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
class Stream{
public:
	Stream(
		ObjectProxy const &_robj, SocketDevice &_rsd
	){}
	
	template <typename F>
	bool postRecvSome(ReactorContext &_rctx, char *_buf, size_t _bufcp, F _f){
		return false;
	}
	
	template <typename F>
	bool recvSome(ReactorContext &_rctx, char *_buf, size_t _bufcp, F _f, size_t &_sz){
		if(!f[0].empty()){
#if 0
			if(s.recv(_rctc, _buf, _bufcp, _sz)){
				return true;
			}else{
				RecvSomeCommand cmd(*this, _buf, _bufcp, _f);
				f[0] = cmd;
				return false;
			}
#endif
			return false;
		}else{
			//TODO: set proper error
			//error(_rctx, ERROR_NS::error_condition(-1, _rctx.error().category()));
			return true;
		}
	}
	
	
	template <typename F>
	bool postSendAll(ReactorContext &_rctx, const char *_buf, size_t _bufcp, F _f){
		return false;
	}
	
	template <typename F>
	bool sendAll(ReactorContext &_rctx, char *_buf, size_t _bufcp, F _f){
		return false;
	}
private:
	typedef boost::function<void(ReactorContext&, SocketDevice&)>		FunctionT;
	Sock		s;
	FunctionT	f[2];
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif
