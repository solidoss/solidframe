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

#include "frame/ipc/ipcmessage.hpp"
#include "frame/ipc/ipcconnectionuid.hpp"

namespace solid{
namespace consensus{
namespace server{

struct Message: Dynamic<Message, DynamicShared<frame::ipc::Message> >{
	Message();
	~Message();
	
	template <class S>
	S& serialize(S &_s, frame::ipc::ConnectionContext const &/*_rctx*/){
		_s.push(replicaidx, "replicaidx").push(srvidx, "srvidx");
		return _s;
	}
	
	/*virtual*/ void ipcOnReceive(frame::ipc::ConnectionContext const &_rctx, MessagePointerT &_rmsgptr);
	/*virtual*/ uint32 ipcOnPrepare(frame::ipc::ConnectionContext const &_rctx);
	/*virtual*/ void ipcOnComplete(frame::ipc::ConnectionContext const &_rctx, int _err);
	
	size_t use();
	size_t release();
	
	uint8							replicaidx;
	frame::IndexT					srvidx;
	
};

}//namespace server
}//namespace consensus
}//namespace solid
#endif
