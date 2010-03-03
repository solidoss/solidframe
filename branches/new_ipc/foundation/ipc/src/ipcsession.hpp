/* Declarations file ipcconnection.hpp
	
	Copyright 2010 Valentin Palade
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

#ifndef IPC_CONNECTION_HPP
#define IPC_CONNECTION_HPP

#include "iodata.hpp"
#include "system/timespec.hpp"

#include "utility/dynamicpointer.hpp"

struct SocketAddress;
struct Inet4SockAddrPair;
struct Inet6SockAddrPair;
struct TimeSpec;
struct AddrInfoIterator;


namespace foundation{

struct Signal;

namespace ipc{

class Session{
public:
	typedef std::pair<const Inet4SockAddrPair*, int> Addr4PairT;
	typedef std::pair<const Inet6SockAddrPair*, int> Addr6PairT;
	Session(
		const Inet4SockAddrPair &_raddr,
		uint32 _keepalivetout
	);
	Session(
		const Inet4SockAddrPair &_raddr,
		int _basport,
		uint32 _keepalivetout
	);
	
	const Inet4SockAddrPair* peerAddr4()const;
	const Addr4PairT* baseAddr4()const;
};

}//namespace ipc
}//namespace foundation


#endif
