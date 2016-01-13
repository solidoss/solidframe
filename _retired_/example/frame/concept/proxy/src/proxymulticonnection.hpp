// proxymulticonnection.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef PROXYMULTICONNECTION_HPP
#define PROXYMULTICONNECTION_HPP

#include "utility/queue.hpp"
#include <string>
#include <deque>
#include "frame/aio/aiomultiobject.hpp"
#include "system/socketaddress.hpp"
namespace solid{
class SocketAddress;
}
namespace concept{

namespace proxy{

class Service;

class MultiConnection: public solid::Dynamic<MultiConnection, solid::frame::aio::MultiObject>{
public:
	MultiConnection(const char *_node = NULL, const char *_srv = NULL);
	MultiConnection(const solid::SocketDevice &_rsd);
	~MultiConnection();
private:
	/*virtual*/ void execute(ExecuteContext &_rexectx);
	solid::AsyncE doReadAddress();
	solid::AsyncE doProxy(const solid::TimeSpec &_tout);
	solid::AsyncE doRefill();
	void state(int _st){
		st  = _st;
	}
	int state()const{
		return st;
	}
private:
	enum{
		READ_ADDR,
		READ_PORT,
		REGISTER_CONNECTION,
		CONNECT,
		CONNECT_TOUT,
		SEND_REMAINS,
		PROXY
	};
	struct Buffer{
		enum{
			Capacity = 2*1024
		};
		Buffer():size(0), usecnt(0){}
		char		data[Capacity];
		unsigned	size;
		unsigned	usecnt;
	};
	struct Stub{
		Buffer			recvbuf;
	};
	solid::ResolveData		rd;
	solid::ResolveIterator	it;
	int						st;
	bool					b;
	std::string				addr;
	std::string				port;
	Stub					stubs[2];
	char*					bp;
	char*					be;
};

}//namespace proxy
}//namespace concept

#endif
