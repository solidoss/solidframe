// consensus/server/src/consensusmessage.hpp
//
// Copyright (c) 2011, 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
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
