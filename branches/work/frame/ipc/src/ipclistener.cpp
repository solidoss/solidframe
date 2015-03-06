// frame/ipc/src/ipclistener.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "frame/manager.hpp"
#include "ipclistener.hpp"
#include "frame/ipc2/ipcservice.hpp"
#include "frame/aio/openssl/opensslsocket.hpp"

namespace solid{
namespace frame{
namespace ipc{

Listener::Listener(
	Service &_rsvc,
	const SocketDevice &_rsd
):BaseT(_rsd), rsvc(_rsvc){
	state = 0;
}

void Listener::execute(ExecuteContext &_rexectx){
	idbgx(Debug::ipc, "");
	cassert(this->socketOk());
	{
		ulong sm = this->grabSignalMask();
		if(sm & frame::S_KILL){
			_rexectx.close();
			return;
		}
	}
	
	uint cnt(10);
	while(cnt--){
		if(state == 0){
			switch(this->socketAccept(sd)){
				case aio::AsyncSuccess:break;
				case aio::AsyncWait:
					state = 1;
					return;
				case aio::AsyncError:
					_rexectx.close();
					return;
			}
		}
		state = 0;
		cassert(sd.ok());
		idbgx(Debug::ipc, "accepted new connection");
		rsvc.insertConnection(sd);
	}
	_rexectx.reschedule();
}

}//namespace ipc
}//namespace frame
}//namespace solid
