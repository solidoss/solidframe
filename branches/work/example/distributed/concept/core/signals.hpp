#ifndef DISTRIBUTED_CONCEPT_CORE_SIGNALS_HPP
#define DISTRIBUTED_CONCEPT_CORE_SIGNALS_HPP

#include "foundation/signal.hpp"
#include "foundation/ipc/ipcconnectionuid.hpp"
#include "utility/dynamicpointer.hpp"

#include <string>

namespace fdt = foundation;

namespace std{

template <class S>
S& operator&(pair<std::string, int64> &_v, S &_s){
	return _s.push(_v.first, "first").push(_v.second, "second");
}

template <class S>
S& operator&(foundation::ObjectUidT &_v, S &_s){
	return _s.push(_v.first, "first").push(_v.second, "second");
}
}
///\endcond

void mapSignals();

struct ConceptSignal: Dynamic<ConceptSignal, DynamicShared<foundation::Signal> >{
	enum{
		OnSender,
		OnPeer,
		BackOnSender
	};
	ConceptSignal();
	~ConceptSignal();
	bool ipcReceived(
		foundation::ipc::SignalUid &_rsiguid,
		const foundation::ipc::ConnectionUid &_rconid,
		const SockAddrPair &_peeraddr, int _peerbaseport
	);
	template <class S>
	S& operator&(S &_s){
		_s.push(requid, "requid").push(senderuid, "sender");
		_s.push(st, "state");
		if(waitresponse || !S::IsSerializer){//on peer
			_s.push(ipcsiguid.idx, "siguid.idx").push(ipcsiguid.uid,"siguid.uid");
		}else{//on sender
			foundation::ipc::SignalContext &rsigctx(foundation::ipc::DynamicContextPointerT::specificContext());
			_s.push(rsigctx.waitid.idx, "siguid.idx").push(rsigctx.waitid.uid,"siguid.uid");
		}
		return _s;
	}
	uint32 ipcPrepare();
	void ipcFail(int _err);
	void ipcSuccess();
	
	void use();
	int release();
	
	bool							waitresponse;
	uint8							st;
	uint32 							requid;
	int8							sentcount;
	fdt::ObjectUidT					senderuid;
	foundation::ipc::ConnectionUid	ipcconid;
	foundation::ipc::SignalUid		ipcsiguid;
};

struct InsertSignal: Dynamic<InsertSignal, ConceptSignal>{
	InsertSignal(const std::string&, uint32 _pos);
	InsertSignal();
	
	template <class S>
	S& operator&(S &_s){
		return static_cast<ConceptSignal*>(this)->operator&<S>(_s);
	}
};

struct FetchSignal: Dynamic<FetchSignal, ConceptSignal>{
	FetchSignal(const std::string&);
    FetchSignal();
	template <class S>
	S& operator&(S &_s){
		return static_cast<ConceptSignal*>(this)->operator&<S>(_s);
	}
};

struct EraseSignal: Dynamic<EraseSignal, ConceptSignal>{
	EraseSignal(const std::string&);
	EraseSignal();
	template <class S>
	S& operator&(S &_s){
		return static_cast<ConceptSignal*>(this)->operator&<S>(_s);
	}
};

#endif
