/* Declarations file ipcnode.hpp
	
	Copyright 2013 Valentin Palade 
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

#ifndef SOLID_FRAME_IPC_SRC_IPC_NODE_HPP
#define SOLID_FRAME_IPC_SRC_IPC_NODE_HPP

#include "frame/aio/aiomultiobject.hpp"
#include "frame/ipc/ipcconnectionuid.hpp"

namespace solid{
namespace frame{
namespace ipc{

class Service;

class Node: public Dynamic<Node, frame::aio::MultiObject>{
public:
	typedef Service							ServiceT;
	
	Node(
		const SocketDevice &_rsd,
		Service &_rservice,
		uint16 _id
	);
	~Node();
	int execute(ulong _sig, TimeSpec &_tout);
private:
	struct Data;
	Data &d;
};



}//namespace ipc
}//namespace frame
}//namespace solid

#endif
