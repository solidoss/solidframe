// protocol/binary/binaryspecificbuffercontroller.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_PROTOCOL_BINARY_SPECIFIC_BUFFER_CONTROLLER_HPP
#define SOLID_PROTOCOL_BINARY_SPECIFIC_BUFFER_CONTROLLER_HPP

#include "system/common.hpp"
#include "system/specific.hpp"

namespace solid{
namespace protocol{
namespace binary{


template <uint16 DataCp, uint16 RecvCp = DataCp * 2, uint16 SendCp = DataCp * 2>
struct SpecificBufferController{
	enum{
		DataCapacity = DataCp,
		RecvCapacity = RecvCp,
		SendCapacity = SendCp
	};
	static const int 	rcv_spec_id;
	static const size_t rcv_spec_cp;
	static const int 	snd_spec_id;
	static const size_t snd_spec_cp;
	
	SpecificBufferController():rcvbuf(NULL), sndbuf(NULL){}
	
	~SpecificBufferController(){
		clear();
	}
	void clearSend(){
		if(sndbuf){
			Specific::pushBuffer(sndbuf, snd_spec_id);
		}
	}
	void clearRecv(){
		if(rcvbuf){
			Specific::pushBuffer(rcvbuf, rcv_spec_id);
		}
	}
	void clear(){
		clearSend();
		clearRecv();
	}
	void prepareSend(){
		if(!sndbuf){
			sndbuf = Specific::popBuffer(snd_spec_id);
		}
	}
	void prepareRecv(){
		if(!rcvbuf){
			rcvbuf = Specific::popBuffer(rcv_spec_id);
		}
	}
	
	size_t sendCapacity()const{
		return snd_spec_cp;
	}
	size_t recvCapacity()const{
		return rcv_spec_cp;
	}
	char *sendBuffer(){
		prepareSend();
		return sndbuf;
	}
	char *recvBuffer(){
		prepareRecv();
		return rcvbuf;
	}
	
private:
	char *rcvbuf;
	char *sndbuf;
};
template <uint16 DataCp, uint16 RecvCp, uint16 SendCp>
const int		SpecificBufferController<DataCp, RecvCp, SendCp>::rcv_spec_id = Specific::sizeToIndex(RecvCp);
template <uint16 DataCp, uint16 RecvCp, uint16 SendCp>
const size_t	SpecificBufferController<DataCp, RecvCp, SendCp>::rcv_spec_cp = Specific::indexToCapacity(rcv_spec_id);
template <uint16 DataCp, uint16 RecvCp, uint16 SendCp>
const int		SpecificBufferController<DataCp, RecvCp, SendCp>::snd_spec_id = Specific::sizeToIndex(SendCp);
template <uint16 DataCp, uint16 RecvCp, uint16 SendCp>
const size_t	SpecificBufferController<DataCp, RecvCp, SendCp>::snd_spec_cp = Specific::indexToCapacity(snd_spec_id);

}//namespace binary
}//namespace protocol
}//namespace solid


#endif
