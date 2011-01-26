#ifndef DISTRIBUTED_CONCEPT_CORE_SIGNALS_HPP
#define DISTRIBUTED_CONCEPT_CORE_SIGNALS_HPP

#include "foundation/signal.hpp"
#include "utility/dynamicpointer.hpp"

#include <string>

namespace fdt = foundation;

struct ConceptSignal: Dynamic<ConceptSignal, DynamicShared<foundation::Signal> >{
	ConceptSignal();
	~ConceptSignal();
	int ipcReceived(
		foundation::ipc::SignalUid &_rsiguid,
		const foundation::ipc::ConnectionUid &_rconid,
		const SockAddrPair &_peeraddr, int _peerbaseport
	);
	uint32 ipcPrepare();
	void ipcFail(int _err);
	
	void use();
	int release();
	
	bool							waitresponse;
	uint32 							reqid;
	int16							sentcount;
	fdt::ObjectUidT					senderuid;
	foundation::ipc::ConnectionUid	ipcconid;
	foundation::ipc::SignalUid		ipcsiguid;
};

struct InsertSignal: Dynamic<InsertSignal, ConceptSignal>{
	InsertSignal(const std::string&, uint32 _pos);
	
	template <class S>
	S& operator&(S &_s){
		_s.pushContainer(ppthlst, "strlst").push(err, "error").push(tout,"timeout");
		_s.push(requid, "requid").push(strpth, "strpth").push(fromv, "from");
		_s.push(siguid.idx, "siguid.idx").push(siguid.uid,"siguid.uid");
		return _s;
	}
};

struct FetchSignal: Dynamic<FetchSignal, ConceptSignal>{
	FetchSignal(const std::string&);
};

struct EraseSignal: Dynamic<EraseSignal, ConceptSignal>{
	EraseSignal(const std::string&);
	EraseSignal();
};

#endif
