/* Declarations file service.hpp
	
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

class Listener;

class Service: public solid::frame::Service{
public:
	Service();
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
