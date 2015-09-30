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

#include "system/function.hpp"
#include "system/common.hpp"

#include "frame/ipc/ipccontext.hpp"
#include "utility/dynamictype.hpp"

namespace solid{
namespace frame{
namespace ipc{

class Service;
class Connection;

struct Message: Dynamic<Message>{
	enum Flags{
		WaitResponseFlagE	= (1<<0),
		SynchronousFlagE	= (1<<1),
		IdempotentFlagE		= (1<<2), 
	};
	
	static bool is_synchronous(const uint32 _flags){
		return (_flags & SynchronousFlagE) != 0;
	}
	static bool is_asynchronous(const uint32 _flags){
		return (_flags & SynchronousFlagE) == 0;
	}
	static bool is_waiting_response(const uint32 _flags){
		return (_flags & WaitResponseFlagE) != 0;
	}
	
	static bool is_idempotent(const uint32 _flags){
		return (_flags & IdempotentFlagE) != 0;
	}
	
	Message(uint8 _state = 0):stt(_state){}
	Message(Message const &_rmsg): requid(_rmsg.requid), stt(_rmsg.stt){}
	
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
	
	uint8 state()const{
		return stt;
	}
	
	RequestUid const& requestId()const{
		return requid;
	}
	
	template <class S>
	void serialize(S &_rs, frame::ipc::ConnectionContext &_rctx){
		if(S::IsSerializer){
			//because a message can be sent to multiple destinations (usign DynamicPointer)
			//on serialization we cannot use/modify the values stored by ipc::Message
			//so, we'll use ones store in the context. Because the context is volatile
			//we'll store as values.
			_rs.pushCrossValue(_rctx.request_uid.index, "requid_idx");
			_rs.pushCrossValue(_rctx.request_uid.unique, "requid_idx");
			_rs.pushValue(_rctx.message_state, "state");
		}else{
			_rs.pushCross(requid.index, "requid_idx");
			_rs.pushCross(requid.unique, "requid_uid");
			_rs.push(stt, "state");
		}
	}
	
private:
	friend class Service;
	friend class TestEntryway;
	friend class Connection;
	
	template <class S, class T>
	static void serialize(S &_rs, T &_rt, const char *_name){
		//here we do only pushes so we can have access to context
		//using the above "serialize" function
		_rs.push(_rt, _name);
		_rs.push(static_cast<Message&>(_rt), "message_base");
	}
	
	void adjustState(){
		if(stt == 3) stt = 0;
	}
private:
	RequestUid	requid;
	uint8		stt;
};

typedef DynamicPointer<Message>															MessagePointerT;
typedef FUNCTION<void(ConnectionContext&, MessagePointerT &, ErrorConditionT const &)>	ResponseHandlerFunctionT;


}//namespace ipc
}//namespace frame
}//namespace solid

#endif
