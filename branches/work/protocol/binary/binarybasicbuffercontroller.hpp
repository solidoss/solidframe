// protocol/binary/binarybasicbuffercontroller.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_PROTOCOL_BINARY_BASIC_BUFFER_CONTROLLER_HPP
#define SOLID_PROTOCOL_BINARY_BASIC_BUFFER_CONTROLLER_HPP

#include "system/common.hpp"

namespace solid{
namespace protocol{
namespace binary{

template <uint16 DataCp, uint16 RecvCp = DataCp * 2, uint16 SendCp = DataCp * 2>
struct BasicBufferController{
	enum{
		DataCapacity = DataCp,
		RecvCapacity = RecvCp,
		SendCapacity = SendCp
	};
	char *sendBuffer(){
		return sndbuf;
	}
	char *recvBuffer(){
		return rcvbuf;
	}
	
private:
	char rcvbuf[RecvCp];
	char sndbuf[SendCp];
};


}//namespace binary
}//namespace protocol
}//namespace solid


#endif
