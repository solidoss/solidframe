// listener.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef EXAMPLE_CONCEPT_AIO_LISTENER_HPP
#define EXAMPLE_CONCEPT_AIO_LISTENER_HPP

#include "frame/aio/aiosingleobject.hpp"
#include "system/socketdevice.hpp"
#include <memory>

namespace solid{
namespace frame{
namespace aio{
namespace openssl{

class Context;

}//namespace openssl
}//namespace aio
}//namespace frame
}//namespace solid

namespace concept{

class Service;

//! A simple listener
class Listener: public solid::Dynamic<Listener, solid::frame::aio::SingleObject>{
public:
	typedef Service		ServiceT;
	Listener(
		Service &_rsvc,
		const solid::SocketDevice &_rsd,
		solid::frame::aio::openssl::Context *_pctx = NULL
	);
private:
	/*virtual*/ void execute(ExecuteContext &_rexectx);
private:
	typedef std::auto_ptr<solid::frame::aio::openssl::Context> SslContextPtrT;
	ServiceT			&rsvc;
	solid::SocketDevice	sd;
	SslContextPtrT		ctxptr;
	int					state;
};

}//namespace concept

#endif
