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
#include "frame/ipc/ipcsessionuid.hpp"

namespace solid{
namespace frame{
namespace ipc{


struct Message: Dynamic<Message, frame::Message>{
	typedef std::pair<uint32, uint32>	UInt32PairT;
	
	Message(uint8 _state = 0):stt(_state){}
	virtual ~Message();
	
	MessageUid& ipcRequestMessageUid();
	MessageUid const & ipcRequestMessageUid()const;
	
	uint8& ipcState();
	uint8 const& ipcState()const;
	
	void ipcResetState(uint8 _stt = 0);
	
	bool ipcIsBackOnSender()const;
	bool ipcIsOnSender()const;
	bool ipcIsOnReceiver()const;
	
private:
	MessageUid	msguid;
	uint8		stt;
};

typedef DynamicPointer<Message>		MessagePointerT;

inline MessageUid& Message::ipcRequestMessageUid(){
	return msguid;
}
inline MessageUid const & Message::ipcRequestMessageUid()const{
	return msguid;
}

inline uint8& Message::ipcState(){
	return stt;
}
inline uint8 const& Message::ipcState()const{
	return stt;
}

inline bool Message::ipcIsOnSender()const{
	return stt == 0;
}
inline bool Message::ipcIsOnReceiver()const{
	return stt == 1;
}
inline bool Message::ipcIsBackOnSender()const{
	return stt == 2;
}

inline void Message::ipcResetState(uint8 _stt){
	stt = _stt;
}

}//namespace ipc
}//namespace frame
}//namespace solid

#endif