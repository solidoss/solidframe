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

namespace clientserver{

enum RetValEx {
	LEAVE = CONTINUE + 1,
	REGISTER, UNREGISTER, RTOUT, WTOUT, DONE
};

enum Signals{
	//simple:
	S_RAISE = 1,
	S_UPDATE = 2,
	S_CMD = 4,
	S_KILL = 1<<8,
	S_IDLE = 1<<9,
	S_ERR  = 1<<10
};

enum Events{
	OKDONE = 1, ERRDONE = 2, TIMEOUT = 4, INDONE = 8, OUTDONE = 16, SIGNALED = 32
};

enum Consts{
	MAXTIMEOUT = (0xffffffff>>1)/1000
};

typedef std::pair<ulong, uint32> ObjectUidTp;
typedef std::pair<uint32, uint32> CommandUidTp;
typedef std::pair<uint32, uint32> FileUidTp;
typedef std::pair<uint32, uint32> RequestUidTp;
}

#endif
