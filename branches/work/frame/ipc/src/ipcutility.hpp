// frame/ipc/src/ipcutility.hpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_SRC_IPC_UTILITY_HPP
#define SOLID_FRAME_IPC_SRC_IPC_UTILITY_HPP

#include "system/cassert.hpp"
#include "system/socketaddress.hpp"

#include "frame/ipc/ipcservice.hpp"

namespace solid{
namespace frame{
namespace ipc{

//*******	AddrPtrCmp		******************************************************************
#ifdef HAS_CPP11

typedef std::pair<const SocketAddressInet4&, uint16>	BaseAddress4T;
typedef std::pair<const SocketAddressInet6&, uint16>	BaseAddress6T;

typedef std::pair<const SocketAddressInet4&, uint32>	GatewayRelayAddress4T;
typedef std::pair<const SocketAddressInet6&, uint32>	GatewayRelayAddress6T;

typedef std::pair<BaseAddress4T, uint32>				RelayAddress4T;
typedef std::pair<BaseAddress6T, uint32>				RelayAddress6T;

struct SocketAddressHash{
	size_t operator()(const SocketAddressInet4* const &_rsa)const{
		return _rsa->hash();
	}
	
	size_t operator()(BaseAddress4T const &_rsa)const{
		return in_addr_hash(_rsa.first.address()) ^ _rsa.second;
	}
	size_t operator()(GatewayRelayAddress4T const &_rsa)const{
		return in_addr_hash(_rsa.first.address()) ^ _rsa.second;
	}
	
	size_t operator()(GatewayRelayAddress6T const &_rsa)const{
		return in_addr_hash(_rsa.first.address()) ^ _rsa.second;
	}
	size_t operator()(BaseAddress6T const &_rsa)const{
		return in_addr_hash(_rsa.first.address()) ^ _rsa.second;
	}
	
	size_t operator()(RelayAddress4T const &_rsa)const{
		return in_addr_hash(_rsa.first.first.address()) ^ _rsa.first.second ^ _rsa.second;
	}
	size_t operator()(RelayAddress6T const &_rsa)const{
		return in_addr_hash(_rsa.first.first.address()) ^ _rsa.first.second ^ _rsa.second;
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
	bool operator()(GatewayRelayAddress4T const &_rsa1, GatewayRelayAddress4T const &_rsa2)const{
		return _rsa1.first.address() == _rsa2.first.address() &&
		_rsa1.second == _rsa2.second;
	}
	bool operator()(GatewayRelayAddress6T const &_rsa1, GatewayRelayAddress6T const &_rsa2)const{
		return _rsa1.first.address() == _rsa2.first.address() &&
		_rsa1.second == _rsa2.second;
	}
	bool operator()(RelayAddress4T const &_rsa1, RelayAddress4T const &_rsa2)const{
		return _rsa1.first.first.address() == _rsa2.first.first.address() &&
		_rsa1.first.second == _rsa2.first.second && 
		_rsa1.second == _rsa2.second;
	}
	bool operator()(RelayAddress6T const &_rsa1, RelayAddress6T const &_rsa2)const{
		return _rsa1.first.first.address() == _rsa2.first.first.address() &&
		_rsa1.first.second == _rsa2.first.second && 
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

struct BaseAddress6Pair{
	BaseAddress6Pair(const SocketAddressInet6&	_first, uint16 _second):first(_first), second(_second){}
	BaseAddress6Pair(const BaseAddress6Pair &_rba):first(_rba.first), second(_rba.second){}
	const SocketAddressInet6&	first;
	uint16						second;
};


struct GatewayRelayAddress4Pair{
	GatewayRelayAddress4Pair(const SocketAddressInet4&	_first, uint32 _second):first(_first), second(_second){}
	GatewayRelayAddress4Pair(const GatewayRelayAddress4Pair &_rba):first(_rba.first), second(_rba.second){}
	const SocketAddressInet4&	first;
	uint32						second;
};

struct GatewayRelayAddress6Pair{
	GatewayRelayAddress6Pair(const SocketAddressInet6&	_first, uint32 _second):first(_first), second(_second){}
	GatewayRelayAddress6Pair(const GatewayRelayAddress6Pair &_rba):first(_rba.first), second(_rba.second){}
	const SocketAddressInet6&	first;
	uint32						second;
};

typedef BaseAddress4Pair 					BaseAddress4T;
typedef BaseAddress6Pair 					BaseAddress6T;


typedef GatewayRelayAddress4Pair			GatewayRelayAddress4T;
typedef GatewayRelayAddress6Pair			GatewayRelayAddress6T;

typedef std::pair<BaseAddress4T, uint32>	RelayAddress4T;
typedef std::pair<BaseAddress6T, uint32>	RelayAddress6T;


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
	
	bool operator()(RelayAddress4T const &_rsa1, RelayAddress4T const &_rsa2)const{
		if(_rsa1.first.first.address() < _rsa2.first.first.address()){
			return true;
		}else if(_rsa2.first.first.address() < _rsa1.first.first.address()){
			if(_rsa1.first.second < _rsa2.first.second){
				return true;
			}else if(_rsa2.first.second < _rsa1.first.second){
				return _rsa1.second < _rsa2.second;
			}
		}
		return false;
	}
	bool operator()(RelayAddress6T const &_rsa1, RelayAddress6T const &_rsa2)const{
		if(_rsa1.first.first.address() < _rsa2.first.first.address()){
			return true;
		}else if(_rsa2.first.first.address() < _rsa1.first.first.address()){
			if(_rsa1.first.second < _rsa2.first.second){
				return true;
			}else if(_rsa2.first.second < _rsa1.first.second){
				return _rsa1.second < _rsa2.second;
			}
		}
		return false;
	}
	
	bool operator()(GatewayRelayAddress4T const &_rsa1, GatewayRelayAddress4T const &_rsa2)const{
		if(_rsa1.first.address() < _rsa2.first.address()){
			return true;
		}else if(_rsa2.first.address() < _rsa1.first.address()){
			return _rsa1.second < _rsa2.second;
		}
		return false;
	}
	bool operator()(GatewayRelayAddress6T const &_rsa1, GatewayRelayAddress6T const &_rsa2)const{
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
}//namespace frame
}//namespace solid

#endif
