/* Implementation file consensussignals.cpp
	
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

#include "consensussignals.hpp"
#include "foundation/ipc/ipcservice.hpp"
#include "example/distributed/consensus/core/consensusmanager.hpp"

#include "foundation/ipc/ipcservice.hpp"

#include "system/debug.hpp"

namespace distributed{
namespace consensus{
namespace server{

Signal::Signal():state(OnSender){}
Signal::~Signal(){
	
}
void Signal::ipcReceived(
	foundation::ipc::SignalUid &_rsiguid
){
	DynamicPointer<fdt::Signal> sig(this);
	//_rsiguid = this->ipcsiguid;
	//ipcconid = fdt::ipc::SignalContext::the().connectionuid;
	
	char				host[SocketAddress::HostNameCapacity];
	char				port[SocketAddress::ServiceNameCapacity];
	
	SocketAddress4		sa;
	
	sa.name(
		host,
		SocketAddress::HostNameCapacity,
		port,
		SocketAddress::ServiceNameCapacity,
		SocketAddress::NumericService | SocketAddress::NumericHost
	);
	
	if(state == OnSender){
		state = OnPeer;
		idbg((void*)this<<" on peer: baseport = "<<fdt::ipc::SignalContext::the().baseport<<" host = "<<host<<":"<<port);
	}else if(state == OnPeer){
		state == BackOnSender;
		idbg((void*)this<<" back on sender: baseport = "<<fdt::ipc::SignalContext::the().baseport<<" host = "<<host<<":"<<port);
	}else{
		cassert(false);
	}
	m().signal(sig, serverUid());
}
uint32 Signal::ipcPrepare(){
	uint32 rv(0);
	rv |= foundation::ipc::Service::SynchronousSendFlag;
	rv |= foundation::ipc::Service::SameConnectorFlag;
	return rv;
}
void Signal::ipcFail(int _err){
	idbg((void*)this<<" err = "<<_err);
}
void Signal::ipcSuccess(){
	idbg((void*)this<<"");
}

void Signal::use(){
	DynamicShared<fdt::Signal>::use();
	idbg((void*)this<<" usecount = "<<usecount);
}
int Signal::release(){
	int rv = DynamicShared<fdt::Signal>::release();
	idbg((void*)this<<" usecount = "<<usecount);
	return rv;
}

}//namespace server
}//namespace consensus
}//namespace distributed
