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
		IPCIsRequestFlag = 1
	};
	Message(uint8 _flags = 0):flgs(_flags){}
	virtual ~Message();
	
	MessageUid& requestMessageUid();
	MessageUid const & requestMessageUid()const;
	
	uint8& flags();
	uint8 const& flags()const;
	
	bool isRequest()const;
	
	virtual void onReceive(ConnectionContext &_ripcctx);
	//! Called by ipc module, before the signal begins to be serialized
	virtual void onPrepare(ConnectionContext &_ripcctx);
	//! Called by ipc module on peer failure detection (disconnect,reconnect)
	virtual void onComplete(ConnectionContext &_ripcctx, int _error);
	
private:
	MessageUid	msguid;
	uint8		flgs;
};

inline MessageUid& Message::requestMessageUid(){
	return msguid;
}
inline MessageUid const & Message::requestMessageUid()const{
	return msguid;
}

inline uint8& Message::flags(){
	return flgs;
}
inline uint8 const& Message::flags()const{
	return flgs;
}

inline bool Message::isRequest()const{
	return (flgs & IPCIsRequestFlag) != 0; 
}


}//namespace ipc
}//namespace frame
}//namespace solid

#endif
