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

inline SocketAddressPair::SocketAddressPair(const SocketAddressInfoIterator &_it){
	if(_it){
		addr = _it.addr(); sz = _it.size();
	}else{
		addr = NULL; sz = 0;
	}
}
inline SocketAddressPair::SocketAddressPair(const SocketAddress &_rsa):addr(_rsa.addr()), sz(_rsa.size()){}
inline SocketAddressPair::SocketAddressPair(const SocketAddress4 &_rsa):addr(_rsa.addr()), sz(_rsa.size()){}

inline SocketAddressPair& SocketAddressPair::operator=(const SocketAddressInfoIterator &_it){
	if(_it){
		addr = _it.addr(); sz = _it.size();
	}else{
		addr = NULL; sz = 0;
	}
	return *this;
}

inline SocketAddressPair& SocketAddressPair::operator=(const SocketAddress &_rsa){
	addr = _rsa.addr();	sz = _rsa.size();
	return *this;
}
inline SocketAddressPair& SocketAddressPair::operator=(const SocketAddress4 &_rsa){
	addr = _rsa.addr();	sz = _rsa.size();
	return *this;
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
inline SocketAddress& SocketAddress::operator=(const SocketAddressPair &_sp){
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
inline SocketAddress4& SocketAddress4::operator=(const SocketAddressPair &_sp){
	addr(_sp.addr, _sp.size());
	return *this;
}
inline size_t SocketAddress4::hash()const{
	//TODO: improve
	return addrin()->sin_addr.s_addr ^ addrin()->sin_port;
}


#ifdef NINLINES
#undef inline
#endif

