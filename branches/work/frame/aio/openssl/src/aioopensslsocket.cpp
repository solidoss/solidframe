// frame/aio/openssl/src/aioopensslsocket.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "frame/aio2/openssl/aioopensslsocket.hpp"
#include "system/socketdevice.hpp"
#include "system/common.hpp"

#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"

namespace solid{
namespace frame{
namespace aio{
namespace openssl{

Socket::Socket(){
	::SSL_set_mode(pssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
	::SSL_set_mode(pssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
	//::SSL_set_mode(pssl, SSL_MODE_RELEASE_BUFFERS);
}
AsyncE Socket::sendAll(
	const size_t _idx,
	const char* _pb, const size_t _bcp,
	ERROR_NS::error_code *_preterr,
	uint32 _flags = 0
){
	if(!pssl){
		return PlainSocket::sendAll(_idx, _pb, _bcp, _preterr, _flags);
	}
	if(this->psendaction){
		return AsyncError;
	}
	uint8		want = ActionData::WantReschedule;
	if(this->precvaction && this->precvaction->wantIO()){
		want = ActionData::WantRecvDone;
	}else{
		int	rv = ::SSL_write(pssl, _pb, _bcp);
		switch(::SSL_get_error(pssl, rv)){
			case SSL_ERROR_NONE:
			case SSL_ERROR_ZERO_RETURN:
			case SSL_ERROR_WANT_READ:
			case SSL_ERROR_WANT_WRITE:
			default:
				return AsyncError;
		}
	}
}

void Socket::ioRun(const size_t _idx, const bool _hasread, const bool _haswrite){
	if(_hasread){
		if(precvaction && precvaction->want == ActionData::WantRead){
			//call action callback
			//precvaction...
			if(psendaction && psendaction->want == ActionData::WantRecvDone){
				if(precvaction == NULL || !precvaction->wantIO()){
					//call action callback
					//psendaction...
				}
			}
		}else if(psendaction && precvaction->want == ActionData::WantRead){
		}
	}
}

}//namespace openssl
}//namespace aio
}//namespace frame
}//namespace solid


