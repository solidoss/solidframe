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

//*******	AddrPtrCmp		******************************************************************
#ifdef HAS_CPP11

typedef std::pair<const SocketAddressInet4&, uint16>	BaseAddress4T;
typedef std::pair<const SocketAddressInet6&, uint16>	BaseAddress6T;

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

struct BaseAddress4Pair{
	BaseAddress4Pair(const SocketAddressInet4&	_first, uint16 _second):first(_first), second(_second){}
	BaseAddress4Pair(const BaseAddress4Pair &_rba):first(_rba.first), second(_rba.second){}
	const SocketAddressInet4&	first;
	uint16						second;
};

typedef BaseAddress4Pair BaseAddress4T;

struct BaseAddress6Pair{
	BaseAddress6Pair(const SocketAddressInet6&	_first, uint16 _second):first(_first), second(_second){}
	BaseAddress6Pair(const BaseAddress6Pair &_rba):first(_rba.first), second(_rba.second){}
	const SocketAddressInet6&	first;
	uint16						second;
};

typedef BaseAddress6Pair BaseAddress6T;


struct SocketAddressCompare{
	
	bool operator()(
		const SocketAddressInet4* const &_sa1,
		const SocketAddressInet4* const &_sa2
	)const{
		return *_sa1 < *_sa2;
	}
	
	bool operator()(BaseAddress4T const &_rsa1, BaseAddress4T const &_rsa2)const{
		if(_rsa1.first.address() < _rsa2.first.address()){
			return true;
		}else if(_rsa2.first.address() < _rsa1.first.address()){
			return _rsa1.second < _rsa2.second;
		}
		return false;
	}
	bool operator()(BaseAddress6T const &_rsa1, BaseAddress6T const &_rsa2)const{
		if(_rsa1.first.address() < _rsa2.first.address()){
			return true;
		}else if(_rsa2.first.address() < _rsa1.first.address()){
			return _rsa1.second < _rsa2.second;
		}
		return false;
	}
};

#endif

}//namespace ipc
}//namespace foundation

#endif
