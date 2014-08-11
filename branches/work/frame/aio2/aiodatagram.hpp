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

namespace solid{
namespace frame{
namespace aio{

struct	ObjectProxy;
struct	ReactorContex;

class Datagram{
public:
	Datagram(ObjectProxy &_robj, SocketDevice &_rsd);
	Datagram(ObjectProxy &_robj);
	
	template <typename F>
	bool recvFrom(
		ReactorContext &_rctx,
		char *_buf, size_t _bufcp,
		F _f
	){
		
	}
	
	template <typename F>
	bool recvFrom(
		ReactorContext &_rctx,
		char *_buf, size_t _bufcp,
		size_t &_sz,
		SocketAddressInet &_addr,
		F _f
	){
		
	}
	
	template <typename F>
	bool sendTo(
		ReactorContext &_rctx,
		const char *_buf, size_t _bufcp,
		SocketAddressStub const &_addrstub,
		F _f
	){
		
	}
	
	template <typename F>
	bool sendTo(
		ReactorContext &_rctx,
		const char *_buf, size_t _bufcp,
		SocketAddressStub const &_addrstub,
		size_t &_sz,
		F _f
	){
		
	}
	
private:
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif
