// frame/ipc/src/ipcmessagereader.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_SRC_IPC_MESSAGE_READER_HPP
#define SOLID_FRAME_IPC_SRC_IPC_MESSAGE_READER_HPP

#include "system/common.hpp"
#include "system/error.hpp"

namespace solid{
namespace frame{
namespace ipc{

struct ConnectionContext;

class MessageReader{
public:
	MessageReader();
	~MessageReader();
	uint16 read(const char *_pbuf, uint16 _bufsz, ConnectionContext &_rctx, ErrorConditionT &_rerror);
private:
	
};

}//namespace ipc
}//namespace frame
}//namespace solid


#endif
