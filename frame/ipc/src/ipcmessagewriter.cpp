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

namespace solid{
namespace frame{
namespace ipc{
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
