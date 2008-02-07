/* Declarations file connectoruid.h
	
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

#ifndef IPC_CONNECTOR_UID_H
#define IPC_CONNECTOR_UID_H

#include "system/common.h"

namespace clientserver{
namespace ipc{
//! A structure to uniquely indetify a connector
/*!
	<b>Overview:</b><br>
	
	An ipc connector somehow resembles a tcp connection. The difference is 
	that (for now) there cant be more than one connector to a process.
	
	If a process is restarted, and a new connector is created, its uid
	will be different from the previous one. This will ensure for example
	that one cannot send a response to a restarted process. 
	
	<b>Usage:</b><br>
	The basic idea is that you send requests identifing the peer process
	either by its base address (see clientserver::ipc::Service) or by
	its unique id, while you'll mostly send responses using ConnectorUid.
	
*/
struct ConnectorUid{
	ConnectorUid(uint32 _tkrid = 0, uint16 _procid = 0, uint16 _procuid = 0):tkrid(_tkrid), procid(_procid), procuid(_procuid){}
	uint32	tkrid;
	uint16	procid;
	uint16	procuid;
};

}//namespace ipc
}//namespace clientserver

#endif
