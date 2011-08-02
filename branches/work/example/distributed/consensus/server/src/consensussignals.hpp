#ifndef CONSENSUSSIGNALS_HPP
#define CONSENSUSSIGNALS_HPP

#include "foundation/signal.hpp"
#include "foundation/ipc/ipcconnectionuid.hpp"
#include "utility/dynamicpointer.hpp"
#include "system/socketaddress.hpp"

#include "example/distributed/consensus/core/consensusrequestid.hpp"
#include "consensussignal.hpp"

#ifdef HAVE_CPP11
#include <array>
#else
#include <vector>
#endif


namespace consensus{

struct OperationStub{
	template <class S>
	S& operator&(S &_s){
		_s.push(operation, "opp").push(reqid, "reqid").push(proposeid, "proposeid").push(acceptid, "acceptid");
		return _s;
	}
	uint8		operation;
	RequestId	reqid;
	uint32		proposeid;
	uint32		acceptid;
};

template <uint16 Count>
struct OperationSignal;

template <>
struct OperationSignal<1>: Dynamic<OperationSignal<1>, Signal>{
	template <class S>
	S& operator&(S &_s){
		static_cast<Signal*>(this)->operator&<S>(_s);
		_s.push(op, "operation");
		return _s;
	}
	
	OperationStub	op;
};

template <>
struct OperationSignal<2>: Dynamic<OperationSignal<2>, Signal>{
	template <class S>
	S& operator&(S &_s){
		static_cast<Signal*>(this)->operator&<S>(_s);
		_s.push(op[1], "operation1");
		_s.push(op[1], "operation2");
		return _s;
	}
	
	OperationStub	op[2];
};

template <uint16 Count>
struct OperationSignal: Dynamic<OperationSignal<Count>, Signal>{
	OperationSignal(){
#ifndef HAVE_CPP11		
		oparr.capacity(Count);
#endif
	}
	template <class S>
	S& operator&(S &_s){
		static_cast<Signal*>(this)->operator&<S>(_s);
		_s.pushContainer(oparr, "operation_array");
		return _s;
	}
	
#ifdef HAVE_CPP11
	typedef std::array<OperationStub, Count>	OperationStubArrayT;
#else
	typedef std::vector<OperationStub>			OperationStubArrayT;
#endif
	OperationStubArrayT		oparr;
};

}//namespace consensus

#endif
