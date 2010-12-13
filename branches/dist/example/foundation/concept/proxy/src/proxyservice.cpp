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
#include "foundation/objectpointer.hpp"

#include "core/manager.hpp"
#include "core/listener.hpp"

#include "proxy/proxyservice.hpp"
#include "proxymulticonnection.hpp"

namespace fdt = foundation;

namespace concept{
namespace proxy{

concept::proxy::Service* Service::create(){
	return new Service;
}

Service::Service(){
}

Service::~Service(){
}

bool Service::insertConnection(
	const SocketDevice &_rsd,
	foundation::aio::openssl::Context *_pctx,
	bool _secure
){
	fdt::ObjectPointer<MultiConnection> conptr(new MultiConnection(_rsd));
	this->insert<AioSchedulerT>(conptr, 0);
	return true;
}

bool Service::insertConnection(
	const AddrInfoIterator &_rai,
	const char *_node,
	const char *_svc
){
	return true;
}

void Service::eraseObject(const MultiConnection &_ro){
	ObjectUidT objuid(_ro.uid());
	idbg("proxy "<<fdt::compute_service_id(objuid.first)<<' '<<fdt::compute_index(objuid.first)<<' '<<objuid.second);
}

}//namespace proxy
}//namespace concept

