// frame/ipc/src/ipcmessagewriter.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_SRC_IPC_MESSAGE_WRITER_HPP
#define SOLID_FRAME_IPC_SRC_IPC_MESSAGE_WRITER_HPP

#include "system/common.hpp"
#include "system/error.hpp"

#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "frame/ipc/ipcserialization.hpp"



namespace solid{
namespace frame{
namespace ipc{

struct Configuration;

class MessageWriter{
public:
	MessageWriter();
	~MessageWriter();
	
	void enqueue(
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		ulong _flags,
		Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx
	);
	
	uint16 write(
		const char *_pbuf,
		uint16 _bufsz,
		Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx,
		ErrorConditionT &_rerror
	);
	void completeMessage(
		MessageUid const &_rmsguid,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx
	);
private:
	struct MessageStub{
		MessageStub(
			MessagePointerT &_rmsgptr,
			const size_t _msg_type_idx,
			ulong _flags
		): msgptr(std::move(_rmsgptr)), msg_type_idx(_msg_type_idx), flags(_flags){}
		
		MessagePointerT msgptr;
		const size_t	msg_type_idx;
		ulong			flags;
	};
	typedef Queue<MessageStub>							MessageQueueT;
private:
};


}//namespace ipc
}//namespace frame
}//namespace solid


#endif
