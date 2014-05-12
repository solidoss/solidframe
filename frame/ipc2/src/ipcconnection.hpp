// frame/ipc/src/ipcconnection.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_SRC_IPC_CONNECTION_HPP
#define SOLID_FRAME_IPC_SRC_IPC_CONNECTION_HPP

#include "frame/aio/aiosingleobject.hpp"
#include "system/socketdevice.hpp"
#include "frame/ipc2/ipcsessionuid.hpp"

namespace solid{
namespace frame{
namespace aio{

namespace openssl{
class Context;
}//namespace openssl

}//namespace aio

namespace ipc{

class Service;

class Connection: public Dynamic<Connection, frame::aio::SingleObject>{
public:
	Connection(
		Service &_rsvc,
		const SocketDevice &_rsd
	);
	Connection(
		Service &_rsvc,
		const SessionUid &_rssnuid,
		const ConnectionUid &_rconuid,
		aio::openssl::Context *_psslctx = NULL
	);
	~Connection();
private:
	/*virtual*/ void execute(ExecuteContext &_rexectx);
private:
	Service				&rsvc;
	SessionUid			ssnid;
	ConnectionUid		conid;
};


}//namespace ipc
}//namespace frame
}//namespace solid

#endif
