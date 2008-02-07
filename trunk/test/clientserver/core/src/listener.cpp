/* Implementation file listener.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "system/debug.h"
#include "system/synchronization.h"

#include "clientserver/tcp/station.h"
#include "clientserver/tcp/channel.h"


#include "core/common.h"
#include "core/server.h"
#include "core/service.h"
#include "core/listener.h"

namespace test{

Listener::~Listener(){
	test::Server &rs = test::Server::the();
	rs.service(*this).removeListener(*this);
}

int Listener::execute(ulong, TimeSpec&){
	idbg("here");
	Server &rs = Server::the();
	Service	&rsrvc = rs.service(*this);
	if(signaled()){
		{
		Mutex::Locker	lock(rs.mutex(*this));
		ulong sm = this->grabSignalMask();
		if(sm & clientserver::S_KILL) return BAD;
		}
	}
	station().accept(chvec);
	for(ChannelVecTp::iterator it(chvec.begin()); it != chvec.end(); ++it){
		if(rsrvc.insertConnection(rs, *it)){
			delete *it;
		}
	}
	return OK;
}

}//namespace test

