#ifndef CONSENSUSSIGNALS_HPP
#define CONSENSUSSIGNALS_HPP

#include "foundation/signal.hpp"
#include "foundation/ipc/ipcconnectionuid.hpp"
#include "utility/dynamicpointer.hpp"
#include "system/socketaddress.hpp"

#include "example/distributed/consensus/core/consensusrequestid.hpp"

namespace consensus{

struct OpperationStub{
	template <class S>
	S& operator&(S &_s){
		_s.push(opperation, "opp").push(reqid, "reqid").push(proposeid, "proposeid").push(acceptid, "acceptid");
		return _s;
	}
	uint8		opperation;
	RequestId	reqid;
	uint32		proposeid;
	uint32		acceptid;
};

struct Signal: Dynamic<Signal, DynamicShared<foundation::Signal> >{
	Signal();
	~Signal();
	void ipcReceived(
		foundation::ipc::SignalUid &_rsiguid
	);
	template <class S>
	S& operator&(S &_s){
		_s.push(replicaidx, "replicaidx");
	}
	
	uint32 ipcPrepare();
	void ipcFail(int _err);
	void ipcSuccess();
	
	void use();
	int release();
	
	uint8							replicaidx;
};
template <uint16 Count>
struct OpperationSignal;

template <>
struct OpperationSignal<1>: Dynamic<OpperationSignal<1>, DynamicShared<foundation::Signal> >{
	template <class S>
	S& operator&(S &_s){
		static_cast<Signal*>(this)->operator&<S>(_s);
		_s.push(id.requid, "id.requid").push(id.senderuid, "sender");
		return _s;
	}
	
	
	OpperationStub	op[1];
};


}//namespace consensus

#endif
