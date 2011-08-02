#ifndef CONSENSUSSIGNAL_HPP
#define CONSENSUSSIGNAL_HPP

#include "foundation/signal.hpp"
#include "foundation/ipc/ipcconnectionuid.hpp"

namespace consensus{

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


}//namespace consensus

#endif
