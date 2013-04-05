/* Implementation file proxyservice.cpp
	
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

#include "system/debug.hpp"
#include "utility/dynamicpointer.hpp"

#include "core/manager.hpp"

#include "proxy/proxyservice.hpp"
#include "proxymulticonnection.hpp"

using namespace solid;

namespace concept{
namespace proxy{

Service::Service(Manager &_rm):BaseT(_rm){
}

Service::~Service(){
}

ObjectUidT Service::insertConnection(
	const solid::ResolveData &_rai,
	solid::frame::aio::openssl::Context *_pctx,
	bool _secure
){
	return frame::invalid_uid();
}
/*virtual*/ ObjectUidT Service::insertConnection(
	const solid::SocketDevice &_rsd,
	solid::frame::aio::openssl::Context *_pctx,
	bool _secure
){
	DynamicPointer<frame::aio::Object>	conptr(new MultiConnection(_rsd));
	ObjectUidT rv = this->registerObject(*conptr);
	Manager::the().scheduleAioObject(conptr);
	return rv;
}

}//namespace proxy
}//namespace concept

