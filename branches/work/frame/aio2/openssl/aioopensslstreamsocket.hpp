// frame/aio/openssl/aioopensslsocket.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_OPENSSL_AIOOPENSSLSOCKET_HPP
#ifndef SOLID_FRAME_AIO_OPENSSL_AIOOPENSSLSOCKET_HPP

#include "frame/aio2/aioplainsocket.hpp"
#include "frame/aio2/aiocommon.hpp"
#include "openssl/ssl.h"

namespace solid{
namespace frame{
namespace aio{
namespace openssl{

class Socket: PlainSocket{
public:
	Socket();
	
	AsyncE socketSendAll(
		const size_t _idx,
		const char* _pb, const size_t _bcp,
		ERROR_NS::error_code *_preterr = NULL,
		uint32 _flags = 0
	);
	void ioRun(const size_t _idx, const bool _hasread, const bool _haswrite);
private:
	SSL	*pssl;
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif
