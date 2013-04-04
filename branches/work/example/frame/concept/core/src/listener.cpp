/* Implementation file listener.cpp
	
	Copyright 2007, 2008, 2013 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

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
):BaseT(_rsd), rsvc(_rsvc), ctxptr(_pctx){
	state = 0;
}

int Listener::execute(ulong, TimeSpec&){
	idbg("here");
	cassert(this->socketOk());
	if(notified()){
		Locker<Mutex>	lock(this->mutex());
		ulong sm = this->grabSignalMask();
		if(sm & frame::S_KILL) return BAD;
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
		//TODO: one may do some filtering on sd based on sd.remoteAddress()
		if(ctxptr.get()){
			rsvc.insertConnection(sd, ctxptr.get(), true);
		}else{
			rsvc.insertConnection(sd);
		}
	}
	
	return OK;
}

}//namespace concept

