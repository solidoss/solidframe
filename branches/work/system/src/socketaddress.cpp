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

AddrInfo::~AddrInfo(){
	if(!empty()){
		freeaddrinfo(ib.paddr);
		ib.paddr = NULL;
	}
}

void AddrInfo::reinit(const char *_node, const char *_service){
	if(!empty()){
		freeaddrinfo(ib.paddr);
		ib.paddr = NULL;
	}
	if(!_node && !_service) return;
	int rv = getaddrinfo(_node, _service, NULL, &ib.paddr);
	if(rv != 0){
#ifdef ON_SOLARIS
		edbgx(Dbg::system, "getaddrinfo "<<rv<<' '<<gai_strerror(rv));
#else
#endif
	}
}

void AddrInfo::reinit(
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

void AddrInfo::reinit(const char *_node, int _port){
	char buf[12];
	sprintf(buf, "%u", _port);
	reinit(_node, buf);
}

void AddrInfo::reinit(
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

// bool SockAddrPair::operator<(const SockAddrPair &_addr)const{
// 
// }

Inet4SockAddrPair::Inet4SockAddrPair(const SockAddrPair &_rsap):addr((sockaddr_in*)_rsap.addr)/*, size(_rsap.size)*/{
	cassert(_rsap.family() == AddrInfo::Inet4);
	cassert(_rsap.size() == size());
}
Inet4SockAddrPair::Inet4SockAddrPair(const SocketAddress &_rsa):addr((sockaddr_in*)_rsa.addr())/*, size(_rsa.size())*/{
	cassert(_rsa.family() == AddrInfo::Inet4);
	cassert(_rsa.size() == size());
}
int Inet4SockAddrPair::port()const{
	return htons(addr->sin_port);
}
void Inet4SockAddrPair::port(uint16 _port){
	addr->sin_port = ntohs(_port);
}
bool Inet4SockAddrPair::operator<(const Inet4SockAddrPair &_addr)const{
	return addr->sin_addr.s_addr < _addr.addr->sin_addr.s_addr;
}

Inet6SockAddrPair::Inet6SockAddrPair(const SockAddrPair &_rsap):addr((sockaddr_in6*)_rsap.addr)/*, size(_rsap.size)*/{
	cassert(_rsap.family() == AddrInfo::Inet6);
	cassert(_rsap.size() == size());
}
Inet6SockAddrPair::Inet6SockAddrPair(const SocketAddress &_rsa):addr((sockaddr_in6*)_rsa.addr())/*,size(_rsa.size())*/{
	cassert(_rsa.family() == AddrInfo::Inet6);
	cassert(_rsa.size() == size());
}
bool Inet6SockAddrPair::operator<(const Inet6SockAddrPair &_addr)const{
	//return addr->sin6_addr.s_addr < _addr.addr->sin6_addr.s_addr;
	return memcmp(
		(const void*)addr->sin6_addr.s6_addr,
		(const void*)_addr.addr->sin6_addr.s6_addr,
		sizeof(in6_addr)
	) < 0;
}
int Inet6SockAddrPair::port()const{
	return htons(addr->sin6_port);
}
void Inet6SockAddrPair::port(uint16 _port){
	addr->sin6_port = ntohs(_port);
}


void SocketAddress::addr(const sockaddr* _sa, size_t _sz){
	if(_sa && _sz){
		memcpy(buf, _sa, _sz);
		sz = _sz;
	}else sz = 0;
}
void SocketAddress::clear(){
	memset(buf, 0, MaxSockAddrSz);
}

bool SocketAddress::operator<(const SocketAddress &_raddr)const{
	//return (*(uint32*)addr()) < (*(uint32*)_raddr.addr());
	if(sz == sizeof(sockaddr_in)){
		return addrin()->sin_addr.s_addr < _raddr.addrin()->sin_addr.s_addr;
	}else{//sockadd_in16
		cassert(false);
		return false;
	}
}

SocketAddress::SocketAddress(const Inet4SockAddrPair &_sp){
	addr((sockaddr*)_sp.addr, _sp.size());
}
SocketAddress::SocketAddress(const Inet6SockAddrPair &_sp){
	addr((sockaddr*)_sp.addr, _sp.size());
}

SocketAddress::SocketAddress(const SockAddrPair &_sp){
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

#ifdef NINLINES
#include "system/socketaddress.ipp"
#endif
