// frame/ipc/src/ipclistener.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_SRC_IPC_LISTENER_HPP
#define SOLID_FRAME_IPC_SRC_IPC_LISTENER_HPP

#include "frame/aio/aioobject.hpp"
#include "frame/aio/aiolistener.hpp"
#include "frame/aio/aiotimer.hpp"

#include "system/socketdevice.hpp"

namespace solid{
namespace frame{
namespace aio{
namespace openssl{
class Context;
}
}

namespace ipc{

class Service;

class Listener: public Dynamic<Listener, frame::aio::Object>{
public:
	Listener(
		SocketDevice &_rsd
	):
		sock(this->proxy(), std::move(_rsd)), timer(this->proxy())
	{}
	~Listener(){
	}
private:
	Service& service(frame::aio::ReactorContext &_rctx);
	/*virtual*/ void onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent);
	void onAccept(frame::aio::ReactorContext &_rctx, SocketDevice &_rsd);
	
	
	typedef frame::aio::Listener			ListenerSocketT;
	typedef frame::aio::Timer				TimerT;
	
	ListenerSocketT		sock;
	TimerT				timer;
};


}//namespace ipc
}//namespace frame
}//namespace solid

#endif
