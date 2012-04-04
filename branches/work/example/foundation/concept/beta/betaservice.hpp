/* Declarations file betaservice.hpp
	
	Copyright 2012 Valentin Palade 
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

#ifndef BETASERVICE_HPP
#define BETASERVICE_HPP

#include "core/service.hpp"

class SocketDevice;

namespace concept{

class Manager;

namespace beta{

class Connection;

class Service: public Dynamic<Service, concept::Service>{
public:
	static Service* create(Manager &);
	Service();
	~Service();
	ObjectUidT insertConnection(
		const SocketDevice &_rsd,
		foundation::aio::openssl::Context *_pctx,
		bool _secure
	);
	ObjectUidT insertConnection(
		SocketAddressInfo &_rai,
		foundation::aio::openssl::Context *_pctx,
		bool _secure
	);
	
	void insertObject(Connection &_ro, const ObjectUidT &_ruid);
	void eraseObject(const Connection &_ro);
};

}//namespace beta
}//namespace concept


#endif
