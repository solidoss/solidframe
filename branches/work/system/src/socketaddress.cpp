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

#if 0
SocketAddressInfo::~SocketAddressInfo(){
	if(!empty()){
		freeaddrinfo(ib.paddr);
		ib.paddr = NULL;
	}
}

void SocketAddressInfo::reinit(const char *_node, const char *_service){
	if(!empty()){
		freeaddrinfo(ib.paddr);
		ib.paddr = NULL;
	}
	if(!_node && !_service) return;
	int rv = getaddrinfo(_node, _service, NULL, &ib.paddr);
	if(rv != 0){
#ifdef ON_SOLARIS
		edbgx(Dbg::system, "getaddrinfo "<<rv<<' '<<gai_strerror(rv));
#elif ON_WINDOWS
		edbgx(Dbg::system, "getaddrinfo "<<rv);
#else
#endif
	}
}

void SocketAddressInfo::reinit(
	const char *_node, 
	const char *_service,
	int _flags,
	int _family,
	int _type,
	int _proto
){
	if(!empty()){
		freeaddrinfo(ib.paddr);
		ib.paddr = NULL;
	}
	if(!_node && !_service) return;
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
	getaddrinfo(_node, _service, &h, &ib.paddr);
}

void SocketAddressInfo::reinit(const char *_node, int _port){
	char buf[12];
	sprintf(buf, "%u", _port);
	reinit(_node, buf);
}

void SocketAddressInfo::reinit(
	const char *_node, 
	int _port,
	int _flags,
	int _family,
	int _type,
	int _proto
){
	char buf[12];
	sprintf(buf, "%u", _port);
	reinit(_node, buf, _flags, _family, _type, _proto);
}

// bool SocketAddressStub::operator<(const SocketAddressStub &_addr)const{
// 
// }

//-----------------------------------------------------------------
/*static*/ bool SocketAddress::equalAdresses(
	SocketAddress const & _rsa1,
	SocketAddress const & _rsa2
){
	if(_rsa1.sz == sizeof(sockaddr_in) && _rsa2.sz == sizeof(sockaddr_in)){
		return _rsa1.addrin()->sin_addr.s_addr == _rsa2.addrin()->sin_addr.s_addr;
	}else{//sockadd_in16
		cassert(false);
		return false;
	}
}

/*static*/ bool SocketAddress::compareAddressesLess(
	SocketAddress const & _rsa1,
	SocketAddress const & _rsa2
){
	if(_rsa1.sz == sizeof(sockaddr_in) && _rsa2.sz == sizeof(sockaddr_in)){
		return _rsa1.addrin()->sin_addr.s_addr < _rsa2.addrin()->sin_addr.s_addr;
	}else{//sockadd_in16
		cassert(false);
		return false;
	}
}

void SocketAddress::addr(const sockaddr* _sa, size_t _sz){
	if(_sa && _sz){
		memcpy(buf, _sa, _sz);
		sz = _sz;
	}else sz = 0;
}
void SocketAddress::clear(){
	memset(buf, 0, Capacity);
	sz = 0;
}

bool SocketAddress::operator<(const SocketAddress &_raddr)const{
	if(sz == sizeof(sockaddr_in)){
		if(addrin()->sin_addr.s_addr < _raddr.addrin()->sin_addr.s_addr){
			return true;
		}else if(addrin()->sin_addr.s_addr > _raddr.addrin()->sin_addr.s_addr){
			return false;
		}else return port() < _raddr.port();
	}else{//sockadd_in16
		cassert(false);
		return false;
	}
}
bool SocketAddress::operator==(const SocketAddress &_raddr)const{
	if(sz == sizeof(sockaddr_in)){
		return addrin()->sin_addr.s_addr == _raddr.addrin()->sin_addr.s_addr &&
			addrin()->sin_port == _raddr.addrin()->sin_port;
	}else{//sockadd_in16
		cassert(false);
		return false;
	}
}
SocketAddress::SocketAddress(const SocketAddressStub4 &_sp){
	addr((sockaddr*)_sp.addr, _sp.size());
}
SocketAddress::SocketAddress(const SocketAddressStub6 &_sp){
	addr((sockaddr*)_sp.addr, _sp.size());
}

SocketAddress::SocketAddress(const SocketAddressStub &_sp){
	addr(_sp.addr, _sp.size());
}

int SocketAddress::port()const{
	if(sz == sizeof(sockaddr_in)){
		return htons(addrin()->sin_port);
	}else{//sockaddr_in16
		cassert(false);
		return false;
	}
}
void SocketAddress::port(int _port){
	if(sz == sizeof(sockaddr_in)){
		addrin()->sin_port = ntohs(_port);
	}else{//sockadd_in16
		cassert(false);
	}
}

int SocketAddress::name(
	char* _host,
	unsigned _hostcp,
	char* _serv,
	unsigned _servcp,
	uint32 _flags
)const{
	if(!_hostcp || !_servcp) return BAD;
	if(!size()) return BAD;
	_host[0] = 0;
	_serv[0] = 0;
	int rv = getnameinfo(addr(), size(), _host, _hostcp, _serv, _servcp, _flags);
	if(rv){
		edbgx(Dbg::system, "getnameinfo: "<<strerror(errno));
		return BAD;
	}
	return OK;
}

//-----------------------------------------------------------------
/*static*/ bool SocketAddress4::equalAdresses(
	SocketAddress4 const & _rsa1,
	SocketAddress4 const & _rsa2
){
	return _rsa1.addrin()->sin_addr.s_addr == _rsa2.addrin()->sin_addr.s_addr;
}
/*static*/ bool SocketAddress4::compareAddressesLess(
	SocketAddress4 const & _rsa1,
	SocketAddress4 const & _rsa2
){
	return _rsa1.addrin()->sin_addr.s_addr < _rsa2.addrin()->sin_addr.s_addr;
}
void SocketAddress4::addr(const sockaddr* _sa, size_t _sz){
	if(_sa && _sz == Capacity){
		memcpy(buf, _sa, _sz);
	}else if(_sa){
		THROW_EXCEPTION_EX("Assigning an address with invalid size", _sz);
	}else{
		THROW_EXCEPTION("Assigning a null address");
	}
}
void SocketAddress4::clear(){
	memset(buf, 0, Capacity);
}

bool SocketAddress4::operator<(const SocketAddress4 &_raddr)const{
	if(addrin()->sin_addr.s_addr < _raddr.addrin()->sin_addr.s_addr){
		return true;
	}else if(addrin()->sin_addr.s_addr > _raddr.addrin()->sin_addr.s_addr){
		return false;
	}else return port() < _raddr.port();
}

bool SocketAddress4::operator==(const SocketAddress4 &_raddr)const{
	return addrin()->sin_addr.s_addr == _raddr.addrin()->sin_addr.s_addr &&
	addrin()->sin_port == _raddr.addrin()->sin_port;
}

SocketAddress4::SocketAddress4(const SocketAddressStub4 &_sp){
	addr((sockaddr*)_sp.addr, _sp.size());
}

SocketAddress4::SocketAddress4(const SocketAddressStub &_sp){
	addr(_sp.addr, _sp.size());
}
int SocketAddress4::port()const{
	return htons(addrin()->sin_port);
}
void SocketAddress4::port(int _port){
	addrin()->sin_port = ntohs(_port);
}

int SocketAddress4::name(
	char* _host,
	unsigned _hostcp,
	char* _serv,
	unsigned _servcp,
	uint32 _flags
)const{
	if(!_hostcp || !_servcp) return BAD;
	if(!size()) return BAD;
	_host[0] = 0;
	_serv[0] = 0;
	int rv = getnameinfo(addr(), size(), _host, _hostcp, _serv, _servcp, _flags);
	if(rv){
		edbgx(Dbg::system, "getnameinfo: "<<strerror(errno));
		return BAD;
	}
	return OK;
}
#endif
