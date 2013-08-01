// proxyservice.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef PROXYSERVICE_HPP
#define PROXYSERVICE_HPP

#include "core/service.hpp"

namespace concept{
namespace proxy{

class Service: public solid::Dynamic<Service, concept::Service>{
public:
	Service(Manager &_rm);
	~Service();
	ObjectUidT insertConnection(
		const solid::ResolveData &_rai,
		solid::frame::aio::openssl::Context *_pctx,
		bool _secure
	);
private:
	/*virtual*/ ObjectUidT insertConnection(
		const solid::SocketDevice &_rsd,
		solid::frame::aio::openssl::Context *_pctx,
		bool _secure
	);
};

}//namespace echo
}//namespace concept
#endif
