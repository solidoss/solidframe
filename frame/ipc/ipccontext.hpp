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

//! A structure to uniquely indetify an IPC connection/session
/*!
	<b>Overview:</b><br>
	
	<b>Usage:</b><br>
	
*/
struct SessionUid{
	SessionUid(
		const size_t _idx = 0xffff,
		const uint32 _uid = 0xffff
	):ssnidx(_idx), ssnuid(_uid){}
	bool isInvalid()const{
		return ssnidx == static_cast<size_t>(-1);
	}
	size_t	ssnidx;
	uint32	ssnuid;
};


struct ConnectionUid: SessionUid{
	
	ConnectionUid(
		const SessionUid &_rssnid = SessionUid(),
		const ObjectUidT &_rconid = ObjectUidT()
	):SessionUid(_rssnid), conid(_rconid){}
	
	bool isInvalid()const{
		return isInvalidConnection() || isInvalidSession();
	}
	
	bool isInvalidConnection()const{
		return conid.isInvalid();
	}
	
	bool isInvalidSession()const{
		return SessionUid::isInvalid();
	}
	ObjectUidT		conid;
};

struct MessageUid{
	MessageUid(
		const uint32 _idx = 0xffffffff,
		const uint32 _uid = 0xffffffff
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
	Service				&rservice;
	ConnectionUid 		connectionuid;
	
	
	bool isOnSender()const{
		return false;
	}
	
	bool isOnPeer()const{
		return false;
	}
	bool isBackOnSender()const{
		return false;
	}
	uint32 flags()const{
		return 0;
	}
	
	Service& service()const{
		return rservice;
	}
private:
	friend class Context;
	
	ConnectionContext(
		Service &_rsrv
	):rservice(_rsrv){}
};

}//namespace ipc
}//namespace frame
}//namespace solid
#endif
