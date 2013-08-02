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
	void schedule(DynamicPointer<frame::Message> &_rmsgptr, uint32 _flags = 0){
		
	}
	template <class Iter>
	void schedule(Iter _beg, const Iter _end){
		while(_beg != _end){
			schedule(_beg->first, _beg->second);
			++_beg;
		}
	}
	bool consume(const char *_pb, size_t _bl){
		return false;
	}
	int fill(char *_pb, size_t _bl){
		return NOK;
	}
private:
};

}//namespace client
}//namespace binary
}//namespace protocol
}//namespace solid


#endif
