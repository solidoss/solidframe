// frame/aio/aiosocket.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_SOCKET_HPP
#define SOLID_FRAME_AIO_SOCKET_HPP

#include "frame/aio/aiocommon.hpp"
#include "system/socketdevice.hpp"
#include <cerrno>

namespace solid{
namespace frame{
namespace aio{

class	Socket{
public:
	
	Socket(SocketDevice &_rsd):sd(_rsd){
		if(sd.ok()){
			sd.makeNonBlocking();
		}
	}
	
	Socket(){
	}
	
	SocketDevice reset(SocketDevice &_rsd){
		SocketDevice tmpsd = sd;
		sd = _rsd;
		return tmpsd;
	}
	
	void shutdown(){
		if(sd.ok()){
			sd.shutdownReadWrite();
		}
	}
	
	bool create(SocketAddressStub const &_rsas){
		bool rv = sd.create(_rsas.family());
		if(rv){
			rv = sd.makeNonBlocking();
		}
		return rv;
	}
	
	bool connect(SocketAddressStub const &_rsas, bool &_can_retry){
		AsyncE rv = sd.connectNonBlocking(_rsas);
		switch(rv){
			case AsyncError:
				_can_retry = false;
				return false;
			case AsyncSuccess: _can_retry = false; return true;
			case AsyncWait: _can_retry = true; return false;
		}
		return false;
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
	
	int recvFrom(char *_pb, size_t _bl, SocketAddress &_addr, bool &_can_retry){
		int rv = sd.recv(_pb, _bl, _addr);
		_can_retry = (errno == EAGAIN/* || errno == EWOULDBLOCK*/);
		return rv;
	}
	
	int sendTo(const char *_pb, size_t _bl, SocketAddressStub const &_rsas, bool &_can_retry){
		int rv = sd.send(_pb, _bl, _rsas);
		_can_retry = (errno == EAGAIN/* || errno == EWOULDBLOCK*/);
		return rv;
	}
private:
	SocketDevice sd;
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif
