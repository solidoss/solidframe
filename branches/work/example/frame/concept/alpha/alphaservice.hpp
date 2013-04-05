/* Declarations file alphaservice.hpp
	
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

};

}//namespace alpha
}//namespace concept


#endif
