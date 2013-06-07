/* Implementation file socketaddress.cpp
	
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "system/socketaddress.hpp"
#include "system/exception.hpp"

namespace solid{

#ifdef NINLINES
#include "system/socketaddress.ipp"
#endif

//-----------------------------------------------------------------------
//			synchronous_resolve
//-----------------------------------------------------------------------
void ResolveData::delete_addrinfo(void *_pv){
	freeaddrinfo(reinterpret_cast<addrinfo*>(_pv));
}

ResolveData synchronous_resolve(const char *_node, const char *_service){
	if(!_node && !_service){
		return ResolveData();
	}
	addrinfo *paddr = NULL;
	getaddrinfo(_node, _service, NULL, &paddr);
	return ResolveData(paddr);
}
ResolveData synchronous_resolve(
	const char *_node, 
	const char *_service, 
	int _flags,
	int _family,
	int _type,
	int _proto
){
	if(!_node && !_service){
		return ResolveData();
	}
	if(_family < 0) _family = AF_UNSPEC;
	if(_type < 0) _type = 0;
	if(_proto < 0) _proto = 0;
	addrinfo h;
	h.ai_flags = _flags;
	h.ai_family = _family;
	h.ai_socktype = _type;
	h.ai_protocol = _proto;
	h.ai_addrlen = 0;
	h.ai_addr = NULL;
	h.ai_canonname = NULL;
	h.ai_next = NULL;
	addrinfo *paddr = NULL;
	getaddrinfo(_node, _service, &h, &paddr);
	return ResolveData(paddr);
}


ResolveData synchronous_resolve(const char *_node, int _port){
	char buf[12];
	sprintf(buf, "%u", _port);
	return synchronous_resolve(_node, buf);
}
ResolveData synchronous_resolve(
	const char *_node, 
	int _port,
	int _flags,
	int _family,
	int _type,
	int _proto
){
	char buf[12];
	sprintf(buf, "%u", _port);
	return synchronous_resolve(_node, buf, _flags, _family, _type, _proto);
}

std::ostream& operator<<(std::ostream& _ros, const SocketAddressInet4& _rsa){
	char host[SocketInfo::HostStringCapacity];
	char service[SocketInfo::ServiceStringCapacity];
	_rsa.toString(host, SocketInfo::HostStringCapacity, service, SocketInfo::ServiceStringCapacity, SocketInfo::NumericHost | SocketInfo::NumericService);
	_ros<<host<<':'<<service;
	return _ros;
}

std::ostream& operator<<(std::ostream& _ros, const SocketAddressInet& _rsa){
	char host[SocketInfo::HostStringCapacity];
	char service[SocketInfo::ServiceStringCapacity];
	_rsa.toString(host, SocketInfo::HostStringCapacity, service, SocketInfo::ServiceStringCapacity, SocketInfo::NumericHost | SocketInfo::NumericService);
	_ros<<host<<':'<<service;
	return _ros;
}


}//namespace solid

