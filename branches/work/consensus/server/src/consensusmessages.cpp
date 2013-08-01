// consensus/server/src/consensusmessages.cpp
//
// Copyright (c) 2011, 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "consensus/consensusregistrar.hpp"
#include "consensusmessages.hpp"
#include "frame/ipc/ipcservice.hpp"
#include "frame/manager.hpp"

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
	frame::Manager::specific().notify(msgptr, Registrar::the().objectUid(srvidx));
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

size_t Message::use(){
	size_t rv = DynamicShared<frame::Message>::use();
	idbg((void*)this<<" usecount = "<<usecount);
	return rv;
}
size_t Message::release(){
	size_t rv = DynamicShared<frame::Message>::release();
	idbg((void*)this<<" usecount = "<<usecount);
	return rv;
}

}//namespace server
}//namespace consensus
}//namespace distributed
