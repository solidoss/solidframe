// alphaservice.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef ALPHASERVICE_HPP
#define ALPHASERVICE_HPP

#include "core/service.hpp"

namespace solid{
class SocketDevice;
}//namespace solid


namespace concept{

class Manager;

namespace alpha{

class Connection;

class Service: public solid::Dynamic<Service, concept::Service>{
public:
	Service(Manager &_rm);
	~Service();
	ObjectUidT insertConnection(
		solid::ResolveData &_rai,
		solid::frame::aio::openssl::Context *_pctx,
		bool _secure
	);
private:
	/*virtual*/ ObjectUidT insertConnection(
		const solid::SocketDevice &_rsd,
		solid::frame::aio::openssl::Context *_pctx,
		bool _secure
	);
private:
	struct Data;
	Data &d;
};

}//namespace alpha
}//namespace concept


#endif
