/* Declarations file ipcutility.hpp
	
	Copyright 2012 Valentin Palade 
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

#ifndef FOUNDATION_IPC_SRC_IPC_UTILITY_HPP
#define FOUNDATION_IPC_SRC_IPC_UTILITY_HPP

#include "system/cassert.hpp"
#include "system/socketaddress.hpp"

#include "foundation/ipc/ipcservice.hpp"

namespace foundation{
class Visitor;
}

namespace foundation{
namespace ipc{

typedef std::pair<const SocketAddressInet4&, uint16>	BaseAddress4T;
typedef std::pair<const SocketAddressInet6&, uint16>	BaseAddress6T;

//*******	AddrPtrCmp		******************************************************************
#ifdef HAS_CPP11

struct SocketAddressHash{
	size_t operator()(const SocketAddressInet4* const &_rsa)const{
		return _rsa->hash();
	}
	
	size_t operator()(BaseAddress4T const &_rsa)const{
		return hash(_rsa.first.address()) ^ _rsa.second;
	}
	size_t operator()(BaseAddress6T const &_rsa)const{
		return hash(_rsa.first.address()) ^ _rsa.second;
	}
};

struct SocketAddressEqual{
	bool operator()(
		const SocketAddressInet4* const &_rsa1,
		const SocketAddressInet4* const &_rsa2
	)const{
		return *_rsa1 == *_rsa2;
	}
	
	bool operator()(BaseAddress4T const &_rsa1, BaseAddress4T const &_rsa2)const{
		return _rsa1.first.address() == _rsa2.first.address() &&
		_rsa1.second == _rsa2.second;
	}
	bool operator()(BaseAddress6T const &_rsa1, BaseAddress6T const &_rsa2)const{
		return _rsa1.first.address() == _rsa2.first.address() &&
		_rsa1.second == _rsa2.second;
	}
};

#else

struct SocketAddressCompare{
	
	bool operator()(
		SocketAddressStub4 const &_sa1,
		SocketAddressStub4 const &_sa2
	)const{
		return _sa1 < _sa2;
	}
	
	bool operator()(BaseAddress4T const &_rsa1, BaseAddress4T const &_rsa2)const{
		if(SocketAddress4::compareAddressesLess(_rsa1.first, _rsa2.first)){
			return true;
		}else if(!SocketAddress4::compareAddressesLess(_rsa2.first, _rsa1.first)){
			return _rsa1.second < _rsa2.second;
		}
		return false;
	}
	bool operator()(BaseAddress6T const &_rsa1, BaseAddress6T const &_rsa2)const{
		if(SocketAddress6::compareAddressesLess(_rsa1.first, _rsa2.first)){
			return true;
		}else if(!SocketAddress6::compareAddressesLess(_rsa2.first, _rsa1.first)){
			return _rsa1.second < _rsa2.second;
		}
		return false;
	}
};

#endif

}//namespace ipc
}//namespace foundation

#endif