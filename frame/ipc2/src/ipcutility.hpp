// frame/ipc/src/ipcutility.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
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

struct SocketAddressHash{
	size_t operator()(const SocketAddressInet* const &_rsa)const{
		return _rsa->hash();
	}
};

struct SocketAddressEqual{
	bool operator()(
		const SocketAddressInet* const &_rsa1,
		const SocketAddressInet* const &_rsa2
	)const{
		return *_rsa1 == *_rsa2;
	}
};

}//namespace ipc
}//namespace frame
}//namespace solid

#endif
