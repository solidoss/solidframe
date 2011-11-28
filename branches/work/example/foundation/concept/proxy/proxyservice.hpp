/* Declarations file proxyservice.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PROXYSERVICE_HPP
#define PROXYSERVICE_HPP

#include "core/service.hpp"

namespace concept{
namespace proxy{

class MultiConnection;
//class Talker;

class Service: public Dynamic<Service, concept::Service>{
public:
	static concept::proxy::Service* create();
	Service();
	~Service();
	bool insertConnection(
		const SocketDevice &_rsd,
		foundation::aio::openssl::Context *_pctx,
		bool _secure
	);
/*	int insertTalker(
		const SocketAddressInfoIterator &_rai,
		const char *_node,
		const char *_svc
	);*/
	bool insertConnection(
		const SocketAddressInfoIterator &_rai,
		const char *_node,
		const char *_svc
	);
	void eraseObject(const MultiConnection &_ro);
};

}//namespace echo
}//namespace concept
#endif
