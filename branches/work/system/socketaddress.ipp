/* Inline implementation file socketaddress.ipp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef NINLINES
#define inline
#endif

inline SocketAddressStub::SocketAddressStub(const SocketAddressInfoIterator &_it){
	if(_it){
		addr = _it.addr(); sz = _it.size();
	}else{
		addr = NULL; sz = 0;
	}
}
inline SocketAddressStub::SocketAddressStub(const SocketAddress &_rsa):addr(_rsa.addr()), sz(_rsa.size()){}
inline SocketAddressStub::SocketAddressStub(const SocketAddress4 &_rsa):addr(_rsa.addr()), sz(_rsa.size()){}

inline SocketAddressStub& SocketAddressStub::operator=(const SocketAddressInfoIterator &_it){
	if(_it){
		addr = _it.addr(); sz = _it.size();
	}else{
		addr = NULL; sz = 0;
	}
	return *this;
}

inline SocketAddressStub& SocketAddressStub::operator=(const SocketAddress &_rsa){
	addr = _rsa.addr();	sz = _rsa.size();
	return *this;
}
inline SocketAddressStub& SocketAddressStub::operator=(const SocketAddress4 &_rsa){
	addr = _rsa.addr();	sz = _rsa.size();
	return *this;
}
inline uint16 SocketAddressStub::port()const{
	if(sz == SocketAddressStub4::size()){
		return htons(reinterpret_cast<const sockaddr_in*>(addr)->sin_port);
	}else{
		return htons(reinterpret_cast<const sockaddr_in6*>(addr)->sin6_port);
	}
}
//----------------------------------------------------------------------------
inline /*static*/ bool SocketAddressStub4::equalAdresses(
	SocketAddressStub4 const & _rsa1,
	SocketAddressStub4 const & _rsa2
){
	return _rsa1.addr->sin_addr.s_addr == _rsa2.addr->sin_addr.s_addr;
}
inline /*static*/ bool SocketAddressStub4::compareAddressesLess(
	SocketAddressStub4 const & _rsa1,
	SocketAddressStub4 const & _rsa2
){
	return _rsa1.addr->sin_addr.s_addr < _rsa2.addr->sin_addr.s_addr;
}

inline SocketAddressStub4::SocketAddressStub4(
	const SocketAddressStub &_rsap
):addr((sockaddr_in*)_rsap.addr)/*, size(_rsap.size)*/{
	cassert(_rsap.family() == SocketAddressInfo::Inet4);
	cassert(_rsap.size() == size());
}
inline SocketAddressStub4::SocketAddressStub4(
	const SocketAddress &_rsa
):addr((sockaddr_in*)_rsa.addr())/*, size(_rsa.size())*/{
	cassert(_rsa.family() == SocketAddressInfo::Inet4);
	cassert(_rsa.size() == size());
}
inline SocketAddressStub4::SocketAddressStub4(
	const SocketAddress4 &_rsa
):addr((sockaddr_in*)_rsa.addr()){
}
inline int SocketAddressStub4::port()const{
	return htons(addr->sin_port);
}
inline void SocketAddressStub4::port(uint16 _port){
	addr->sin_port = ntohs(_port);
}
inline bool SocketAddressStub4::operator<(
	const SocketAddressStub4 &_addr
)const{
	//return addr->sin_addr.s_addr < _addr.addr->sin_addr.s_addr;
	if(addr->sin_addr.s_addr < _addr.addr->sin_addr.s_addr){
		return true;
	}else if(addr->sin_addr.s_addr > _addr.addr->sin_addr.s_addr){
		return false;
	}else return port() < _addr.port();
}
inline bool SocketAddressStub4::operator==(
	const SocketAddressStub4 &_addr
)const{
	return (addr->sin_addr.s_addr == _addr.addr->sin_addr.s_addr) && (port() == _addr.port());
}
inline size_t SocketAddressStub4::hash()const{
	return addr->sin_addr.s_addr ^ addr->sin_port;
}
inline size_t SocketAddressStub4::addressHash()const{
	return addr->sin_addr.s_addr;
}
//----------------------------------------------------------------------------

inline /*static*/ bool SocketAddressStub6::equalAdresses(
	SocketAddressStub6 const & _rsa1,
	SocketAddressStub6 const & _rsa2
){
	return memcmp(
		(const void*)_rsa1.addr->sin6_addr.s6_addr,
		(const void*)_rsa2.addr->sin6_addr.s6_addr,
		sizeof(in6_addr)
	) == 0;
}
inline /*static*/ bool SocketAddressStub6::compareAddressesLess(
	SocketAddressStub6 const & _rsa1,
	SocketAddressStub6 const & _rsa2
){
	return memcmp(
		(const void*)_rsa1.addr->sin6_addr.s6_addr,
		(const void*)_rsa2.addr->sin6_addr.s6_addr,
		sizeof(in6_addr)
	) < 0;
}

inline SocketAddressStub6::SocketAddressStub6(
	const SocketAddressStub &_rsap
):addr((sockaddr_in6*)_rsap.addr)/*, size(_rsap.size)*/{
	cassert(_rsap.family() == SocketAddressInfo::Inet6);
	cassert(_rsap.size() == size());
}
inline SocketAddressStub6::SocketAddressStub6(
	const SocketAddress &_rsa
):addr((sockaddr_in6*)_rsa.addr())/*,size(_rsa.size())*/{
	cassert(_rsa.family() == SocketAddressInfo::Inet6);
	cassert(_rsa.size() == size());
}
inline bool SocketAddressStub6::operator<(
	const SocketAddressStub6 &_addr
)const{
	//return addr->sin6_addr.s_addr < _addr.addr->sin6_addr.s_addr;
	//TODO:
	return (memcmp(
		(const void*)addr->sin6_addr.s6_addr,
		(const void*)_addr.addr->sin6_addr.s6_addr,
		sizeof(in6_addr)
	) < 0);
}
inline bool SocketAddressStub6::operator==(
	const SocketAddressStub6 &_addr
)const{
	return (memcmp(
		(const void*)addr->sin6_addr.s6_addr,
		(const void*)_addr.addr->sin6_addr.s6_addr,
		sizeof(in6_addr)
	) < 0) && (port() == _addr.port());
}
inline int SocketAddressStub6::port()const{
	return htons(addr->sin6_port);
}
inline void SocketAddressStub6::port(uint16 _port){
	addr->sin6_port = ntohs(_port);
}

inline size_t SocketAddressStub6::hash()const{
	return 0;
}
inline size_t SocketAddressStub6::addressHash()const{
	return 0;
}
//----------------------------------------------------------------------------
inline SocketAddress::SocketAddress(const SocketAddressInfoIterator &_it){
	if(_it){
		addr(_it.addr(), _it.size());
	}else{
		sz = 0;
	}
}
inline SocketAddress& SocketAddress::operator=(const SocketAddressInfoIterator &_it){
	if(_it){
		addr(_it.addr(), _it.size());
	}else{
		sz = 0;
	}
	return *this;
}
inline SocketAddress& SocketAddress::operator=(const SocketAddressStub &_sp){
	addr(_sp.addr, _sp.size());
	return *this;
}
inline size_t SocketAddress::hash()const{
	if(sz == sizeof(sockaddr_in)){
		//TODO: improve
		return addrin()->sin_addr.s_addr ^ addrin()->sin_port;
	}else{//ipv6
		//TODO:
		return 0;
	}
}

inline size_t SocketAddress::addressHash()const{
	if(sz == sizeof(sockaddr_in)){
		//TODO: improve
		return addrin()->sin_addr.s_addr;
	}else{//ipv6
		//TODO:
		return 0;
	}
}

//----------------------------------------------------------------------------
inline SocketAddress4::SocketAddress4(const SocketAddressInfoIterator &_it){
	if(_it){
		addr(_it.addr(), _it.size());
	}else{
		clear();
	}
}
inline SocketAddress4& SocketAddress4::operator=(const SocketAddressInfoIterator &_it){
	if(_it){
		addr(_it.addr(), _it.size());
	}else{
		clear();
	}
	return *this;
}
inline SocketAddress4& SocketAddress4::operator=(const SocketAddressStub &_sp){
	addr(_sp.addr, _sp.size());
	return *this;
}
inline size_t SocketAddress4::hash()const{
	//TODO: improve
	return addrin()->sin_addr.s_addr ^ addrin()->sin_port;
}

inline size_t SocketAddress4::addressHash()const{
	return addrin()->sin_addr.s_addr;
}


#ifdef NINLINES
#undef inline
#endif

