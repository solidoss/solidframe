// service.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef CONCEPT_CORE_SERVICE_HPP
#define CONCEPT_CORE_SERVICE_HPP

#include "utility/dynamictype.hpp"
#include "frame/service.hpp"
#include "common.hpp"

namespace solid{
struct ResolveData;
struct SocketDevice;

namespace frame{

namespace aio{
namespace openssl{

class Context;

}//namespace openssl
}//namespace aio
}//namespace frame
}//namespace solid

namespace concept{
class Manager;
class Listener;

class Service: public solid::frame::Service{
public:
	Service(Manager &_rm);
	~Service();
	
	bool insertListener(
		const solid::ResolveData &_rai,
		bool _secure = false
	);
private:
	friend class Listener;

	virtual ObjectUidT insertConnection(
		const solid::SocketDevice &_rsd,
		solid::frame::aio::openssl::Context *_pctx = NULL,
		bool _secure = false
	);
};

}//namespace concept
#endif
