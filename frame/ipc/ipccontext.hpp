// frame/ipc/ipccontext.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_IPC_CONTEXT_HPP
#define SOLID_FRAME_IPC_IPC_CONTEXT_HPP

#include "system/socketaddress.hpp"
#include "utility/dynamicpointer.hpp"
#include "frame/common.hpp"

namespace solid{
namespace frame{
namespace ipc{

//! A structure to uniquely indetify an IPC connection pool
/*!
	<b>Overview:</b><br>
	
	<b>Usage:</b><br>
	
*/
struct ConnectionPoolUid: UniqueId{
	ConnectionPoolUid(
		const size_t _idx = -1,
		const uint32 _uid = -1
	):UniqueId(_idx, _uid){}
	
};


struct ConnectionUid{
	
	ConnectionUid(
		const ConnectionPoolUid &_rpoolid = ConnectionPoolUid(),
		const ObjectUidT &_rconid = ObjectUidT()
	):poolid(_rpoolid), conid(_rconid){}
	
	bool isInvalid()const{
		return isInvalidConnection() || isInvalidPool();
	}
	
	bool isInvalidConnection()const{
		return conid.isInvalid();
	}
	
	bool isInvalidPool()const{
		return poolid.isInvalid();
	}
	
	ConnectionPoolUid	poolid;
	ObjectUidT			conid;
};

struct MessageUid{
	MessageUid(
		const uint32 _idx = -1,
		const uint32 _uid = -1
	):idx(_idx), uid(_uid){}
	uint32	idx;
	uint32	uid;
};

class Service;

//! Thread specific information about current ipc context
/*!
	This should be used by signals to get information about the current
	ipc context:<br>
	- the current ipc connection uid;<br>
	- the peer address and peer base port for current connection;<br>
	- the unique id of the signal<br>
	<br>
	Remember that signals can be broadcasted to multiple destinations.
	The ConnectionContext helps a signal know which ipc sessions calls its
	callbacks (Signal::ipcComplete, Signal::ipcReceive).<br>
	Also for the case when we want to wait for response, on signal
	serialization we need to use the signaluid from the current context.
	
	See concept::alpha::RemoteListSignal from alphasignals.hpp for an example.
	
*/
struct ConnectionContext{
	
	
	Service& service()const{
		return rservice;
	}
	
	ConnectionUid	connectionId()const{
		return ConnectionUid();
	}
	
// 	SocketDevice const & device()const{
// 		
// 	}
	
	uint32 messageFlags()const{
		return 0;
	}
private:
	Service				&rservice;
	
	friend class Context;
	
	ConnectionContext(
		Service &_rsrv
	):rservice(_rsrv){}
};

}//namespace ipc
}//namespace frame
}//namespace solid
#endif
