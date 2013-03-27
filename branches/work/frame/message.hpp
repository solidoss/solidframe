/* Declarations file message.hpp
	
	Copyright 2013 Valentin Palade 
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

#ifndef SOLID_FRAME_SIGNAL_HPP
#define SOLID_FRAME_SIGNAL_HPP

#include "frame/common.hpp"

#include "utility/streampointer.hpp"
#include "utility/dynamictype.hpp"

#include <string>

namespace solid{

class TimeSpec;

template <class T, class C>
class DynamicPointer;

struct SocketAddressStub;

namespace frame{

namespace ipc{
	struct ConnectionUid;
	struct MessageUid;
}

class MessageSteward;
class Object;

struct Message: Dynamic<Message>{
	enum IPCStatus{
		IpcNone = 0,
		IpcOnSender,
		IpcOnPeer,
		IpcBackOnSender
	};
	Message();
	virtual ~Message();
	//! Called by ipc module after the message was successfully parsed
	/*!
		\param _waitingmessageuid	The UID of the waiting message - one can 
			configure a Request message to wait within the ipc module until
			either the response was received or a communication error occured.
			The transmited request must keep the id of the waiting message 
			(given by the ipc module when calling Message's ipcPrepare method)
			so that when the response comes back, it gives it back to the 
			ipc module.
	*/
	virtual void ipcReceive(
		ipc::MessageUid &_waitingmessageuid
	);
	//! Called by ipc module, before the signal begins to be serialized
	virtual uint32 ipcPrepare();
	//! Called by ipc module on peer failure detection (disconnect,reconnect)
	virtual void ipcComplete(int _error);
	
	//! Called by the MessageSteward
	virtual int execute(
		DynamicPointer<Message> &_rthis_ptr,
		uint32 _evs,
		MessageSteward &_rms,
		const MessageUidT &_rmsguid,
		TimeSpec &_rts
	);

	//! Called by the MessageSteward when receiving a message for a message
	/*!
		If it returns OK, the message is rescheduled for execution
	*/
	virtual int receiveMessage(
		DynamicPointer<Message> &_rmsgptr,
		const ObjectUidT& _from = ObjectUidT(),
		const ipc::ConnectionUid *_conid = NULL
	);
};

}//namespace frame
}//namespace solid

#endif
