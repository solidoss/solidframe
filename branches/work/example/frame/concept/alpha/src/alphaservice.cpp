/* Implementation file alphaservice.cpp
	
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
#include "core/manager.hpp"


#include "serialization/binary.hpp"

#include "frame/aio/openssl/opensslsocket.hpp"

#include "alpha/alphaservice.hpp"
#include "alphaconnection.hpp"

using namespace solid;

namespace concept{
namespace alpha{

#ifdef HAS_SAFE_STATIC

struct InitServiceOnce{
	InitServiceOnce(Manager &_rm);
};

InitServiceOnce::InitServiceOnce(Manager &_rm){
	Connection::initStatic(_rm);
}

Service::Service(Manager &_rm):BaseT(_rm){
	static InitServiceOnce once(_rm);
}

#else
void once_init(){
	Connection::initStatic(_rm);
}

Service::Service(Manager &_rm):BaseT(_rm){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_init, once);
}

#endif

Service::~Service(){
}


ObjectUidT Service::insertConnection(
	solid::ResolveData &_rai,
	solid::frame::aio::openssl::Context *_pctx,
	bool _secure
){
	DynamicPointer<frame::aio::Object>	conptr(new Connection(_rai));
	ObjectUidT rv = this->registerObject(*conptr);
	Manager::the().scheduleAioObject(conptr);
	return rv;
}
/*virtual*/ ObjectUidT Service::insertConnection(
	const solid::SocketDevice &_rsd,
	solid::frame::aio::openssl::Context *_pctx,
	bool _secure
){
	DynamicPointer<frame::aio::Object>	conptr(new Connection(_rsd));
	ObjectUidT rv = this->registerObject(*conptr);
	Manager::the().scheduleAioObject(conptr);
	return rv;
}

}//namespace alpha
}//namespace concept

