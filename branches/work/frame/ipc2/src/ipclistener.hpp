// frame/ipc/src/ipclistener.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_SRC_IPC_LISTENER_HPP
#define SOLID_FRAME_IPC_SRC_IPC_LISTENER_HPP

#include "frame/aio/aiosingleobject.hpp"
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

class Listener: public Dynamic<Listener, frame::aio::SingleObject>{
public:
	Listener(
		Service &_rsvc,
		const SocketDevice &_rsd,
		frame::aio::openssl::Context *_pctx = NULL
	);
private:
	/*virtual*/ void execute(ExecuteContext &_rexectx);
private:
	typedef std::auto_ptr<frame::aio::openssl::Context> SslContextPtrT;
	Service				&rsvc;
	SocketDevice		sd;
	SslContextPtrT		pctx;
	int					state;
};


}//namespace ipc
}//namespace frame
}//namespace solid

#endif
