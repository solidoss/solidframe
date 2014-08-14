// frame/aio/aiolistener.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_LISTENER_HPP
#define SOLID_FRAME_AIO_LISTENER_HPP

#include "system/common.hpp"
#include "system/socketdevice.hpp"

namespace solid{
namespace frame{
namespace aio{

struct	ObjectProxy;
struct	ReactorContex;

class Listener{
public:
	Listener(ObjectProxy &_robj, SocketDevice &_rsd);
	Listener(ObjectProxy &_robj);
	
	
	//returns true when an error (like operation already in progress) occurs
	template <typename F>
	bool accept(ReactorContext &_rctx, F _f){
		
	}
	
	template <typename F>
	bool accept(ReactorContext &_rctx, SocketDevice &_rsd, F _f){
		
	}
private:
	
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif
