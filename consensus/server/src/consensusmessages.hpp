// consensus/server/src/consensussignals.hpp
//
// Copyright (c) 2011, 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_CONSENSUS_CONSENSUSSIGNALS_HPP
#define SOLID_CONSENSUS_CONSENSUSSIGNALS_HPP

#include "frame/message.hpp"
#include "frame/ipc/ipcconnectionuid.hpp"
#include "utility/dynamicpointer.hpp"
#include "system/socketaddress.hpp"

#include "consensus/consensusrequestid.hpp"
#include "consensusmessage.hpp"

#ifdef HAS_CPP11
#include <array>
#else
#include <vector>
#endif

namespace solid{
namespace consensus{
namespace server{

struct OperationStub{
	template <class S>
	S& serialize(S &_s, frame::ipc::ConnectionContext const &/*_rctx*/){
		_s.push(operation, "opp").push(reqid, "reqid").push(proposeid, "proposeid").push(acceptid, "acceptid");
		return _s;
	}
	uint8					operation;
	consensus::RequestId	reqid;
	uint32					proposeid;
	uint32					acceptid;
};

template <uint16 Count>
struct OperationMessage;

template <>
struct OperationMessage<1>: Dynamic<OperationMessage<1>, Message>{
	template <class S>
	S& serialize(S &_s, frame::ipc::ConnectionContext const &_rctx){
		static_cast<Message*>(this)->serialize(_s, _rctx);
		
		_s.push(op, "operation");
		return _s;
	}
	
	OperationStub	op;
};

template <>
struct OperationMessage<2>: Dynamic<OperationMessage<2>, Message>{
	template <class S>
	S& serialize(S &_s, frame::ipc::ConnectionContext const &_rctx){
		static_cast<Message*>(this)->serialize(_s, _rctx);
		_s.push(op[0], "operation1");
		_s.push(op[1], "operation2");
		return _s;
	}
	
	OperationStub	op[2];
};

template <uint16 Count>
struct OperationMessage: Dynamic<OperationMessage<Count>, Message>{
	OperationMessage(){
		opsz = 0;
	}
	template <class S>
	S& serialize(S &_s, frame::ipc::ConnectionContext const &_rctx){
		static_cast<Message*>(this)->serialize(_s, _rctx);
		_s.pushArray(op, opsz, "operations");
		return _s;
	}
	
	OperationStub		op[Count];
	size_t				opsz;
};

}//namespace server
}//namespace consensus
}//namespace solid

#endif
