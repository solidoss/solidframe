// frame/ipc/src/ipcprotocol.cpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "frame/ipc/ipcprotocol.hpp"
#include "ipcutility.hpp"

namespace solid{
namespace frame{
namespace ipc{
//-----------------------------------------------------------------------------
/*virtual*/ Deserializer::~Deserializer(){}
//-----------------------------------------------------------------------------
/*virtual*/ Serializer::~Serializer(){}
//-----------------------------------------------------------------------------
/*virtual*/ Protocol::~Protocol(){}
//-----------------------------------------------------------------------------
bool PacketHeader::isOk()const{
	bool rv = true;
	switch(type_){
		case SwitchToNewMessageTypeE:
		case SwitchToOldMessageTypeE:
		case ContinuedMessageTypeE:
		case SwitchToOldCanceledMessageTypeE:
		case ContinuedCanceledMessageTypeE:
		case KeepAliveTypeE:
			break;
		default:
			rv = false;
			break;
	}
	
	if(size() > Protocol::MaxPacketDataSize){
		rv = false;
	}
	
	return rv;
}
//-----------------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
