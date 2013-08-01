// frame/ipc/src/ipclistener.cpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "frame/manager.hpp"
#include "ipclistener.hpp"

namespace solid{
namespace frame{
namespace ipc{

Listener::Listener(
	Service &_rsvc,
	const SocketDevice &_rsd,
	Service::Types	_type,
	frame::aio::openssl::Context *_pctx
):BaseT(_rsd), rsvc(_rsvc), type(_type), pctx(_pctx){
	state = 0;
}

int Listener::execute(ulong, TimeSpec&){
	idbgx(Debug::ipc, "");
	cassert(this->socketOk());
	{
		ulong sm = this->grabSignalMask();
		if(sm & frame::S_KILL){
			return BAD;
		}
	}
	
	uint cnt(10);
	while(cnt--){
		if(state == 0){
			switch(this->socketAccept(sd)){
				case BAD: return BAD;
				case OK:break;
				case NOK:
					state = 1;
					return NOK;
			}
		}
		state = 0;
		cassert(sd.ok());
		idbgx(Debug::ipc, "accepted new connection");
		//TODO: filtering on sd based on sd.remoteAddress()
		if(pctx.get()){
			rsvc.insertConnection(sd, pctx.get(), true);
		}else{
			rsvc.insertConnection(sd, pctx.get(), false);
		}
	}
	return OK;
}
}//namespace ipc
}//namespace frame
}//namespace solid
