/* Declarations file consensussignals.hpp
	
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
	S& operator&(S &_s){
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
	S& operator&(S &_s){
		static_cast<Message*>(this)->operator&<S>(_s);
		_s.push(op, "operation");
		return _s;
	}
	
	OperationStub	op;
};

template <>
struct OperationMessage<2>: Dynamic<OperationMessage<2>, Message>{
	template <class S>
	S& operator&(S &_s){
		static_cast<Message*>(this)->operator&<S>(_s);
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
	S& operator&(S &_s){
		static_cast<Message*>(this)->operator&<S>(_s);
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
