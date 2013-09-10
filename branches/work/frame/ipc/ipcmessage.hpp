// frame/ipc/ipcservice.hpp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_IPCMESSAGE_HPP
#define SOLID_FRAME_IPC_IPCMESSAGE_HPP

#include "system/common.hpp"
#include "frame/message.hpp"
#include "frame/ipc/ipcconnectionuid.hpp"

namespace solid{
namespace frame{
namespace ipc{

struct Message: Dynamic<Message, frame::Message>{
	enum IPCFlags{
		IPCIsRequestFlag = 1,
		IPCIsResponseFlag = 2
	};
	
	typedef DynamicPointer<Message>		MessagePointerT;
	
	Message(uint8 _flags = 0):flgs(_flags){}
	virtual ~Message();
	
	MessageUid& ipcRequestMessageUid();
	MessageUid const & ipcRequestMessageUid()const;
	
	uint8& ipcFlags();
	uint8 const& ipcFlags()const;
	
	bool ipcIsRequest()const;
	bool ipcIsResponse()const;
	
	virtual void ipcOnReceive(ConnectionContext &_ripcctx, MessagePointerT &_rmsgptr);
	//! Called by ipc module, before the signal begins to be serialized
	virtual void ipcOnPrepare(ConnectionContext &_ripcctx);
	//! Called by ipc module on peer failure detection (disconnect,reconnect)
	virtual void ipcOnComplete(ConnectionContext &_ripcctx, int _error);
	
private:
	MessageUid	msguid;
	uint8		flgs;
};

inline MessageUid& Message::ipcRequestMessageUid(){
	return msguid;
}
inline MessageUid const & Message::ipcRequestMessageUid()const{
	return msguid;
}

inline uint8& Message::ipcFlags(){
	return flgs;
}
inline uint8 const& Message::ipcFlags()const{
	return flgs;
}

inline bool Message::ipcIsRequest()const{
	return (flgs & IPCIsRequestFlag) != 0; 
}

inline bool Message::ipcIsResponse()const{
	return (flgs & IPCIsResponseFlag) != 0; 
}


}//namespace ipc
}//namespace frame
}//namespace solid

#endif
