// protocol/binary/client/clientsession.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_PROTOCOL_BINARY_CLIENT_SESSION_HPP
#define SOLID_PROTOCOL_BINARY_CLIENT_SESSION_HPP

#include "frame/message.hpp"
#include "utility/dynamicpointer.hpp"

namespace solid{
namespace protocol{
namespace binary{
namespace client{

class Session{
public:
	Session():rcvbufoff(0){}
	
	void schedule(DynamicPointer<frame::Message> &_rmsgptr, uint32 _flags = 0){
		
	}
	char * recvBufferOffset(char *_pbuf)const{
		return _pbuf + rcvbufoff;
	}
	const size_t recvBufferCapacity(const size_t _cp)const{
		return _cp - rcvbufoff;
	}
	template <class Iter>
	void schedule(Iter _beg, const Iter _end){
		while(_beg != _end){
			schedule(_beg->first, _beg->second);
			++_beg;
		}
	}
	template <class C>
	bool consume(const char *_pb, size_t _bl, C &_rc, char *_tmpbuf, const size_t _tmpbufcp){
		return false;
	}
	template <class C>
	int fill(char *_pb, size_t _bl, C &_rc, char *_tmpbuf, const size_t _tmpbufcp){
		return NOK;
	}
private:
	uint16		rcvbufoff;
};

}//namespace client
}//namespace binary
}//namespace protocol
}//namespace solid


#endif
