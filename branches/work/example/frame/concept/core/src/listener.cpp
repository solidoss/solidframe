// listener.cpp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "frame/aio/openssl/opensslsocket.hpp"
#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "system/mutex.hpp"

#include "core/common.hpp"
#include "core/manager.hpp"
#include "core/service.hpp"
#include "listener.hpp"

using namespace solid;

namespace concept{

Listener::Listener(
	Service &_rsvc,
	const solid::SocketDevice &_rsd,
	solid::frame::aio::openssl::Context *_pctx
):BaseT(_rsd, true), rsvc(_rsvc), ctxptr(_pctx){
	state = 0;
}

/*virtual*/ void Listener::execute(ExecuteContext &_rexectx){
	idbg("here");
	cassert(this->socketOk());
	if(notified()){
		Locker<Mutex>	lock(Manager::specific().mutex(*this));
		ulong sm = this->grabSignalMask();
		if(sm & frame::S_KILL){ 
			_rexectx.close();
			return;
		}
	}
	
	solid::uint cnt(10);
	
	while(cnt--){
		if(state == 0){
			switch(this->socketAccept(sd)){
				case frame::aio::AsyncError:
					_rexectx.close();
					return;
				case frame::aio::AsyncSuccess:break;
				case frame::aio::AsyncWait:
					state = 1;
					return;
			}
		}
		state = 0;
		cassert(sd.ok());
		//TODO: one may do some filtering on sd based on sd.remoteAddress()
		if(ctxptr.get()){
			rsvc.insertConnection(sd, ctxptr.get(), true);
		}else{
			rsvc.insertConnection(sd);
		}
	}
	_rexectx.reschedule();
}

}//namespace concept

