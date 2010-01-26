/* Declarations file common.hpp
	
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

#ifndef CS_COMMON_HPP
#define CS_COMMON_HPP

#include <utility>
#include "utility/common.hpp"

namespace foundation{

//! Some return values
enum RetValEx {
	LEAVE = CONTINUE + 1,
	REGISTER, UNREGISTER, RTOUT, WTOUT, DONE
};

//! Some signals
enum Signals{
	//simple:
	S_RAISE = 1,
	S_UPDATE = 2,
	S_SIG = 4,
	S_KILL = 1<<8,
	S_IDLE = 1<<9,
	S_ERR  = 1<<10
};

//! Some events
enum Events{
	OKDONE = 1, //Successfull asynchrounous completion
	ERRDONE = 2,//Unsuccessfull asynchrounous completion
	INDONE = 4,//Successfull input asynchrounous completion
	OUTDONE = 8,//Successfull output asynchrounous completion
	TIMEOUT = 16,//Unsuccessfull asynchrounous completion due to timeout
	SIGNALED = 32,
	IODONE = 64,
	RESCHEDULED = 128,
	TIMEOUT_RECV = 256,
	TIMEOUT_SEND = 512,
	
};

enum Consts{
	MAXTIMEOUT = (0xffffffff>>1)/1000
};
#ifdef _LP64
//64 bit architectures
#ifdef UINDEX32
//32 bit indexes
typedef uint32 IndexTp;
#define ID_MASK 0xfffffff
#else
//64 bit indexes
typedef uint64 IndexTp;
#define ID_MASK 0xffffffffffffffffL
#endif

#else

//32 bit architectures
#ifdef UINDEX64
//64 bit indexes
typedef uint64 IndexTp;
#define ID_MASK 0xffffffffffffffffL
#else
//32 bit indexes
typedef uint32 IndexTp;
#define ID_MASK 0xfffffff
#endif


#endif


typedef std::pair<IndexTp, uint32> ObjectUidTp;
typedef std::pair<IndexTp, uint32> SignalUidTp;
typedef std::pair<IndexTp, uint32> FileUidTp;
typedef std::pair<IndexTp, uint32> RequestUidTp;
}

#endif
