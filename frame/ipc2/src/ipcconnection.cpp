// frame/ipc/src/ipcconnection.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "frame/manager.hpp"
#include "ipcconnection.hpp"
#include "frame/ipc2/ipcservice.hpp"
#include "frame/aio/openssl/opensslsocket.hpp"

namespace solid{
namespace frame{
namespace ipc{

Connection::Connection(
	Service &_rsvc,
	const SocketDevice &_rsd
):BaseT(_rsd), rsvc(_rsvc){
}

Connection::~Connection(){
	
}
void Connection::execute(ExecuteContext &_rexectx){
}

}//namespace ipc
}//namespace frame
}//namespace solid
