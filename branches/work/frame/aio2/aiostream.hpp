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

namespace solid{
namespace frame{
namespace aio{

struct	ObjectProxy;
struct	ReactorContex;

template <class Sock>
class Stream{
public:
	Stream(ObjectProxy &_robj, SocketDevice &_rsd);
	Stream(ObjectProxy &_robj);
	
	template <typename F>
	bool recvSome(ReactorContext &_rctx, char *_buf, size_t _bufcp, F _f){
		
	}
	
	template <typename F>
	bool recvSome(ReactorContext &_rctx, char *_buf, size_t _bufcp, size_t &_sz, F _f){
		if(!hasPendingRecv(_rctx)){
			if(s.recv(_rctc, _buf, _bufcp, _sz)){
				return true;
			}else{
				RecvSomeCommand cmd(*this, _buf, _bufcp, _f);
				fnc[0] = cmd;
				return false;
			}
		}else{
			_rctx.error(error_condition(OP_ALREADY_IN_PROGRESS));
			return true;
		}
	}
	
	template <typename F>
	bool recvAll(ReactorContext &_rctx, char *_buf, size_t _bufcp, F _f){
		
	}
	
	template <typename F>
	bool recvAll(ReactorContext &_rctx, char *_buf, size_t _bufcp, size_t &_sz, F _f){
		
	}
	
	template <typename F>
	bool sendSome(ReactorContext &_rctx, const char *_buf, size_t _bufcp, F _f){
		
	}
	
	template <typename F>
	bool sendSome(ReactorContext &_rctx, const char *_buf, size_t _bufcp, size_t &_sz, F _f){
		
	}
	
	template <typename F>
	bool sendAll(ReactorContext &_rctx, const char *_buf, size_t _bufcp, F _f){
		
	}
	
	template <typename F>
	bool sendAll(ReactorContext &_rctx, char *_buf, size_t _bufcp, size_t &_sz, F _f){
		
	}
private:
	Sock		s;
	FunctionT	fnc[2];
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif
