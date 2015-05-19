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
#include "frame/ipc/ipccontext.hpp"
#include "utility/dynamictype.hpp"

namespace solid{
namespace frame{
namespace ipc{

class Service;

struct Message: Dynamic<Message>{
	
	Message(uint8 _state = 0):stt(_state){}
	Message(Message const &_rmsg): msguid(_rmsg.msguid), stt(_rmsg.stt){}
	
	virtual ~Message();
	
	bool isOnSender()const{
		return stt == 0;
	}
	bool isOnPeer()const{
		return stt == 1;
	}
	bool isBackOnSender()const{
		return stt == 2;
	}
	
private:
	friend class Service;
	template <class S, class T>
	static void serialize(S &_rs, T &_rt, const char *_name){
		_rs.push(_rt, _name);
		_rs.pushCross(static_cast<Message&>(_rt).msguid.index, "msguid_idx");
		_rs.pushCross(static_cast<Message&>(_rt).msguid.unique, "msguid_uid");
		_rs.push(static_cast<Message&>(_rt).stt, "state");
	}
	void nextState(){
		++stt;
		if(stt == 3) stt = 0;
	}
private:
	MessageUid	msguid;
	uint8		stt;
};

typedef DynamicPointer<Message>		MessagePointerT;


}//namespace ipc
}//namespace frame
}//namespace solid

#endif
