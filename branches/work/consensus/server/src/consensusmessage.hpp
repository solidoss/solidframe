/* Declarations file consensusmessage.hpp
	
	Copyright 2011, 2012 Valentin Palade 
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

#ifndef SOLID_CONSENSUS_CONSENSUSMESSAGE_HPP
#define SOLID_CONSENSUS_CONSENSUSMESSAGE_HPP

#include "frame/message.hpp"
#include "frame/ipc/ipcconnectionuid.hpp"

namespace solid{
namespace consensus{
namespace server{

struct Message: Dynamic<Message, DynamicShared<frame::Message> >{
	enum{
		OnSender,
		OnPeer,
		BackOnSender
	};
	Message();
	~Message();
	
	template <class S>
	S& operator&(S &_s){
		_s.push(replicaidx, "replicaidx").push(state, "state").push(srvidx, "srvidx");
		return _s;
	}
	
	/*virtual*/ void ipcReceive(
		frame::ipc::MessageUid &_rmsguid
	);
	/*virtual*/ uint32 ipcPrepare();
	/*virtual*/ void ipcComplete(int _err);
	
	size_t use();
	size_t release();
	
	uint8							replicaidx;
	uint8							state;
	frame::IndexT					srvidx;
	
};

}//namespace server
}//namespace consensus
}//namespace solid
#endif
