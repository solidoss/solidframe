// frame/aio/aioplainsocket.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_INTERNET_SOCKET_HPP
#define SOLID_FRAME_AIO_INTERNET_SOCKET_HPP

#include "frame/aio2/aiocommon.hpp"
#include "system/socketdevice.hpp"
#include <cerrno>

namespace solid{
namespace frame{
namespace aio{

class PlainSocket{
public:
	
	PlainSocket(SocketDevice &_rsd):sd(_rsd){
		if(sd.ok()){
			sd.makeNonBlocking();
		}
	}
	
	
	ReactorEventsE filterReactorEvents(
		const  ReactorEventsE _evt,
		const bool /*_pending_recv*/,
		const bool /*_pendign_send*/
	) const {
		return _evt;
	}
	
	int recv(char *_pb, size_t _bl, bool &_can_retry){
		int rv = sd.recv(_pb, _bl);
		_can_retry = (errno == EAGAIN/* || errno == EWOULDBLOCK*/);
		return rv;
	}
	
	int send(const char *_pb, size_t _bl, bool &_can_retry){
		int rv = sd.send(_pb, _bl);
		_can_retry = (errno == EAGAIN/* || errno == EWOULDBLOCK*/);
		return rv;
	}
	
	SocketDevice const& device()const{
		return sd;
	}
	
private:
	SocketDevice sd;
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif
