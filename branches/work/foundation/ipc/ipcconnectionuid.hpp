/* Declarations file ipcsessionuid.hpp
	
	Copyright 2010 Valentin Palade 
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

#ifndef FOUNDATION_IPC_IPC_SESSION_UID_HPP
#define FOUNDATION_IPC_IPC_SESSION_UID_HPP

#include "system/common.hpp"
#include "system/socketaddress.hpp"
#include "utility/dynamicpointer.hpp"

struct C;
namespace foundation{
namespace ipc{
//! A structure to uniquely indetify an IPC connection/session
/*!
	<b>Overview:</b><br>
	
	<b>Usage:</b><br>
	
*/
struct ConnectionUid{
	ConnectionUid(
		uint32 _id = 0,
		uint16 _sesidx = 0,
		uint16 _sesuid = 0
	):tid(_id), idx(_sesidx), uid(_sesuid){}
	uint32	tid;
	uint16	idx;
	uint16	uid;
};

struct SignalUid{
	SignalUid(uint32 _idx = 0xffffffff, uint32 _uid = 0xffffffff):idx(_idx), uid(_uid){}
	uint32	idx;
	uint32	uid;
};

//! Thread specific information about current ipc context
/*!
	This should be used by signals to get information about the current
	ipc context:<br>
	- the current ipc connection uid;<br>
	- the peer address and peer base port for current connection;<br>
	- the unique id of the signal<br>
	<br>
	Remember that signals can be broadcasted to multiple destinations.
	The SignalContext helps a signal know which ipc sessions calls its
	callbacks (Signal::ipcFail, Signal::ipcSuccess, Signal::ipcReceive).<br>
	Also for the case when we want to wait for response, on signal
	serialization we need to use the signaluid from the current context.
	
	See concept::alpha::RemoteListSignal from alphasignals.hpp for an example.
	
	
	
*/
	
struct SignalContext{
	static const SignalContext& the();
	ConnectionUid 		connectionuid;
	SignalUid			signaluid;
	SocketAddressPair	pairaddr;
	int 				baseport;
private:
	friend class Context;
	
	SignalContext(uint32 _tkrid):connectionuid(_tkrid), baseport(-1){}
};

typedef uint32 SerializationTypeIdT;
#define SERIALIZATION_INVALIDID ((SerializationTypeIdT)0)

}//namespace ipc
}//namespace foundation

#endif
