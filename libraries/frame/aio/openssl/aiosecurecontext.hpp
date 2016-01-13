// frame/aio/openssl/aiosecurecontext.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO_OPENSSL_SECURE_CONTEXT_HPP
#define SOLID_FRAME_AIO_OPENSSL_SECURE_CONTEXT_HPP

#include "openssl/ssl.h"
#include "system/error.hpp"

namespace solid{
namespace frame{
namespace aio{
namespace openssl{

class Socket;
class Context{
public:
	static Context create(const SSL_METHOD* = NULL);
	
	
	Context();
	
	Context(Context && _rctx);
	
	Context& operator=(Context && _rctx);
	
	~Context();
	
	bool ok()const;
	
	//!Use it on client side to load the certificates
	ErrorCodeT loadFile(const char *_path);
	//!Use it on client side to load the certificates
	ErrorCodeT loadPath(const char *_path);
	
	//!Use it on server side to load the certificates
	ErrorCodeT loadCertificateFile(const char *_path);
	//!Use it on server side to load the certificates
	ErrorCodeT loadPrivateKeyFile(const char *_path);
	
private:
	Context(Context const&);
	Context& operator=(Context const&);
private:
	friend class Socket;
	SSL_CTX	*pctx;
};

}//namespace openssl
}//namespace aio
}//namespace frame
}//namespace solid


#endif
