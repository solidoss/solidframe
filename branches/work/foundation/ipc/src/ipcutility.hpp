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
#ifdef HAVE_CPP11

struct SockAddrHash{
	size_t operator()(const SocketAddressPair4*const &_psa)const{
		return _psa->addr->sin_addr.s_addr ^ _psa->addr->sin_port;
	}
	
	typedef std::pair<const SocketAddressPair4*, int>	PairProcAddr;
	
	size_t operator()(const PairProcAddr* const &_psa)const{
		return _psa->first->addr->sin_addr.s_addr ^ _psa->second;
	}
};

struct SockAddrEqual{
	bool operator()(const SocketAddressPair4*const &_psa1, const SocketAddressPair4*const &_psa2)const{
		return _psa1->addr->sin_addr.s_addr == _psa2->addr->sin_addr.s_addr &&
		_psa1->addr->sin_port == _psa2->addr->sin_port;
	}
	
	typedef std::pair<const SocketAddressPair4*, int>	PairProcAddr;
	
	bool operator()(const PairProcAddr* const &_psa1, const PairProcAddr* const &_psa2)const{
		return _psa1->first->addr->sin_addr.s_addr == _psa2->first->addr->sin_addr.s_addr &&
		_psa1->second == _psa2->second;
	}
};

#else

struct Inet4AddrPtrCmp{
	
	bool operator()(const SocketAddressPair4*const &_sa1, const SocketAddressPair4*const &_sa2)const{
		//TODO: optimize
		cassert(_sa1 && _sa2); 
		if(*_sa1 < *_sa2){
			return true;
		}else if(!(*_sa2 < *_sa1)){
			return _sa1->port() < _sa2->port();
		}
		return false;
	}
	
	typedef std::pair<const SocketAddressPair4*, int>	PairProcAddr;
	
	bool operator()(const PairProcAddr* const &_sa1, const PairProcAddr* const &_sa2)const{
		cassert(_sa1 && _sa2); 
		cassert(_sa1->first && _sa2->first); 
		if(*_sa1->first < *_sa2->first){
			return true;
		}else if(!(*_sa2->first < *_sa1->first)){
			return _sa1->second < _sa2->second;
		}
		return false;
	}
};

struct Inet6AddrPtrCmp{
	
	bool operator()(const SocketAddressPair6*const &_sa1, const SocketAddressPair6*const &_sa2)const{
		//TODO: optimize
		cassert(_sa1 && _sa2); 
		if(*_sa1 < *_sa2){
			return true;
		}else if(!(*_sa2 < *_sa1)){
			return _sa1->port() < _sa2->port();
		}
		return false;
	}
	
	typedef std::pair<const SocketAddressPair6*, int>	PairProcAddr;
	
	bool operator()(const PairProcAddr* const &_sa1, const PairProcAddr* const &_sa2)const{
		cassert(_sa1 && _sa2); 
		cassert(_sa1->first && _sa2->first); 
		if(*_sa1->first < *_sa2->first){
			return true;
		}else if(!(*_sa2->first < *_sa1->first)){
			return _sa1->second < _sa2->second;
		}
		return false;
	}
};

#endif

}//namespace ipc
}//namespace foundation

#endif