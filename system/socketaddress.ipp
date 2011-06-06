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

inline SockAddrPair::SockAddrPair(const AddrInfoIterator &_it){
	if(_it){
		addr = _it.addr(); sz = _it.size();
	}else{
		addr = NULL; sz = 0;
	}
}
inline SockAddrPair::SockAddrPair(SocketAddress &_rsa):addr(_rsa.addr()), sz(_rsa.size()){}

inline SockAddrPair& SockAddrPair::operator=(const AddrInfoIterator &_it){
	if(_it){
		addr = _it.addr(); sz = _it.size();
	}else{
		addr = NULL; sz = 0;
	}
	return *this;
}

inline SockAddrPair& SockAddrPair::operator=(SocketAddress &_rsa){
	addr = _rsa.addr();	sz = _rsa.size();
	return *this;
}

//----------------------------------------------------------------------------
inline SocketAddress::SocketAddress(const AddrInfoIterator &_it){
	if(_it){
		addr(_it.addr(), _it.size());
	}else{
		sz = 0;
	}
}
inline SocketAddress& SocketAddress::operator=(const AddrInfoIterator &_it){
	if(_it){
		addr(_it.addr(), _it.size());
	}else{
		sz = 0;
	}
	return *this;
}
inline SocketAddress& SocketAddress::operator=(const SockAddrPair &_sp){
	addr(_sp.addr, _sp.size());
	return *this;
}


#ifdef NINLINES
#undef inline
#endif

