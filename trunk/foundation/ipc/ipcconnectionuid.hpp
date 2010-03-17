/* Declarations file ipcsessionuid.hpp
	
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

#ifndef IPC_SESSION_UID_HPP
#define IPC_SESSION_UID_HPP

#include "system/common.hpp"

namespace foundation{
namespace ipc{
//! A structure to uniquely indetify an IPC connection/session
/*!
	<b>Overview:</b><br>
	
	<b>Usage:</b><br>
	
*/
struct ConnectionUid{
	ConnectionUid(
		uint32 _id = 0,
		uint16 _sesidx = 0,
		uint16 _sesuid = 0
	):id(_id), sessionidx(_sesidx), sessionuid(_sesuid){}
	uint32	id;
	uint16	sessionidx;
	uint16	sessionuid;
};

struct SignalUid{
	SignalUid(uint32 _idx = 0xffffffff, uint32 _uid = 0xffffffff):idx(_idx), uid(_uid){}
	uint32	idx;
	uint32	uid;
};

}//namespace ipc
}//namespace foundation

#endif
