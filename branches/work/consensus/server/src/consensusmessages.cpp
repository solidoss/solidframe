/* Implementation file consensusmessages.cpp
	
	Copyright 2011, 2012 Valentin Palade 
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

#include "consensusmessages.hpp"
#include "frame/ipc/ipcservice.hpp"
//#include "example/distributed/consensus/core/consensusmanager.hpp"

#include "frame/ipc/ipcservice.hpp"

#include "system/debug.hpp"

namespace solid{
namespace consensus{
namespace server{

Message::Message():state(OnSender){}
Message::~Message(){
	
}
void Message::ipcReceive(
	frame::ipc::MessageUid &_rmsguid
){
	DynamicPointer<frame::Message>	msgptr(this);
	char							host[SocketInfo::HostStringCapacity];
	char							port[SocketInfo::ServiceStringCapacity];
	
	//TODO:!! sa not initialized !?
	SocketAddressInet4				sa;
	sa = frame::ipc::ConnectionContext::the().pairaddr;
	sa.toString(
		host,
		SocketInfo::HostStringCapacity,
		port,
		SocketInfo::ServiceStringCapacity,
		SocketInfo::NumericService | SocketInfo::NumericHost
	);
	
	if(state == OnSender){
		state = OnPeer;
		idbg((void*)this<<" on peer: baseport = "<<frame::ipc::ConnectionContext::the().baseport<<" host = "<<host<<":"<<port);
	}else if(state == OnPeer){
		state == BackOnSender;
		idbg((void*)this<<" back on sender: baseport = "<<frame::ipc::ConnectionContext::the().baseport<<" host = "<<host<<":"<<port);
	}else{
		cassert(false);
	}
	//TODO::
	//frame::Manager::specific().notify(msgptr, serverUid());
}
uint32 Message::ipcPrepare(){
	uint32 rv(0);
	rv |= frame::ipc::SynchronousSendFlag;
	rv |= frame::ipc::SameConnectorFlag;
	return rv;
}
void Message::ipcComplete(int _err){
	idbg((void*)this<<" err = "<<_err);
}

void Message::use(){
	DynamicShared<frame::Message>::use();
	idbg((void*)this<<" usecount = "<<usecount);
}
int Message::release(){
	int rv = DynamicShared<frame::Message>::release();
	idbg((void*)this<<" usecount = "<<usecount);
	return rv;
}

}//namespace server
}//namespace consensus
}//namespace distributed
