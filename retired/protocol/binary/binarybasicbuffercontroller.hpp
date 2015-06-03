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
	size_t sendCapacity()const{
		return SendCapacity;
	}
	size_t recvCapacity()const{
		return RecvCapacity;
	}
	char *sendBuffer(){
		return sndbuf;
	}
	char *recvBuffer(){
	
		return rcvbuf;
	}
	
	void clearSend(){
	}
	void clearRecv(){
	}
	void clear(){
	}
	void prepareSend(){
	}
	void prepareRecv(){
	}
private:
	char rcvbuf[RecvCapacity];
	char sndbuf[SendCapacity];
};


}//namespace binary
}//namespace protocol
}//namespace solid


#endif
