// frame/ipc/src/ipcmessagereader.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "ipcmessagewriter.hpp"
#include "frame/ipc/ipcconfiguration.hpp"
#include "system/specific.hpp"

namespace solid{
namespace frame{
namespace ipc{

struct Serializer: public SerializerT, public SpecificObject{
};
//-----------------------------------------------------------------------------
MessageWriter::MessageWriter(){}
//-----------------------------------------------------------------------------
MessageWriter::~MessageWriter(){}
//-----------------------------------------------------------------------------
// Needs:
// max prequeue size
// max multiplexed message count
// max waiting message count
// function message_complete
//
void MessageWriter::enqueue(
	MessagePointerT &_rmsgptr,
	const size_t _msg_type_idx,
	ulong _flags,
	Configuration const &_rconfig,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx
){
	
	if(
		write_q.size() < _rconfig.max_writer_multiplex_message_count or _rmsgptr.empty()
	){
		//put message in write_q
		uint32		idx;
		
		if(cache_stk.size()){
			idx = cache_stk.top();
			cache_stk.pop();
		}else{
			idx = message_vec.size();
			message_vec.push_back(MessageStub()); 
		}
		
		MessageStub		&rmsgstub(message_vec[idx]);
		rmsgstub.flags = _flags;
		rmsgstub.msg_type_idx = _msg_type_idx;
		rmsgstub.msgptr = std::move(_rmsgptr);
		
		write_q.push(WriteStub(idx));
	}else if(_rconfig.max_writer_pending_message_count == 0 or pending_message_q.size() < _rconfig.max_writer_pending_message_count){
		pending_message_q.push(PendingMessageStub(_rmsgptr, _msg_type_idx, _flags));
	}else{
		//fail to enqueue message
	}
}
//-----------------------------------------------------------------------------
// Does:
// prepare message
// serialize messages on buffer
// completes messages - those that do not need wait for response
// keeps a serializer for every message that is multiplexed
// compress the filled buffer
// 
// Needs:
// 
// 
uint16 MessageWriter::write(
	const char *_pbuf, uint16 _bufsz,
	Configuration const &_rconfig,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx, ErrorConditionT &_rerror
){
	return 0;
}
//-----------------------------------------------------------------------------
void MessageWriter::completeMessage(
	MessageUid const &_rmsguid,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx
){
	
}
//-----------------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
