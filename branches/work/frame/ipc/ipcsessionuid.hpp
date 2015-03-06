// frame/ipc/ipcconnectionuid.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_IPC_CONNECTION_UID_HPP
#define SOLID_FRAME_IPC_IPC_CONNECTION_UID_HPP

#include "system/socketaddress.hpp"
#include "utility/dynamicpointer.hpp"
#include "frame/common.hpp"

namespace solid{
namespace frame{
namespace ipc{

struct Message;
struct Session;

enum{
	LocalNetworkId = 0,
	InvalidNetworkId = -1
};

//! A structure to uniquely indetify an IPC connection/session
/*!
	<b>Overview:</b><br>
	
	<b>Usage:</b><br>
	
*/
struct SessionUid{
	SessionUid(
		const uint16 _idx = 0xffff,
		const uint16 _uid = 0xffff
	):ssnidx(_idx), ssnuid(_uid){}
	bool isInvalid()const{
		return ssnidx == 0xffff;
	}
	uint16	ssnidx;
	uint16	ssnuid;
};


struct ConnectionUid{
	ConnectionUid(
		const uint16 _idx = 0xffff,
		const uint16 _uid = 0xffff
	):conidx(_idx), conuid(_uid){}
	bool isInvalid()const{
		return conidx == 0xffff;
	}
	uint16	conidx;
	uint16	conuid;
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
	typedef DynamicPointer<Message>		MessagePointerT;
	static const ConnectionContext& the();
	
	Service				&rservice;
	SessionUid 			sessionuid;
	int 				listenport;
	int					pairport;
	
	
	MessagePointerT& requestMessage(const Message &_rmsg)const;
	
	Service& service()const{
		return rservice;
	}
private:
	friend class Context;
	friend class Session;
	
	Session				*psession;
	
	ConnectionContext(
		Service &_rsrv
	):rservice(_rsrv){}
};

typedef uint32 SerializationTypeIdT;
#define SERIALIZATION_INVALIDID ((SerializationTypeIdT)0)

}//namespace ipc
}//namespace frame
}//namespace solid
#endif
