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
#include "frame/ipc/ipccontext.hpp"

namespace solid{
namespace frame{
namespace ipc{
//-----------------------------------------------------------------------------
MessageWriter::MessageWriter(){}
//-----------------------------------------------------------------------------
MessageWriter::~MessageWriter(){}
//-----------------------------------------------------------------------------
void MessageWriter::enqueue(PendingSendMessageVectorT const& _rmsgvec){
	
}
//-----------------------------------------------------------------------------
void MessageWriter::enqueue(
	MessagePointerT &_rmsgptr,
	const size_t _msg_type_idx,
	ulong _flags
){
	
}
//-----------------------------------------------------------------------------
uint16 MessageWriter::write(const char *_pbuf, uint16 _bufsz, ConnectionContext &_rctx, ErrorConditionT &_rerror){
	return 0;
}
//-----------------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
