#ifndef CONSENSUSREQUESTS_HPP
#define CONSENSUSREQUESTS_HPP

#include "distributed/consensus/consensusrequest.hpp"

#include <string>

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

void mapSignals();


struct StoreRequest: Dynamic<StoreRequest, distributed::consensus::WriteRequestSignal>{
	StoreRequest(const std::string&, uint32 _pos);
	StoreRequest();
	
	/*virtual*/ void sendThisToConsensusObject();
	
	template <class S>
	S& operator&(S &_s){
		static_cast<distributed::consensus::WriteRequestSignal*>(this)->operator&<S>(_s);
		_s.push(v,"value");
		return _s;
	}
	uint32	v;
};

struct FetchRequest: Dynamic<FetchRequest, distributed::consensus::WriteRequestSignal>{
	FetchRequest(const std::string&);
    FetchRequest();
	
	/*virtual*/ void sendThisToConsensusObject();
	
	template <class S>
	S& operator&(S &_s){
		return static_cast<distributed::consensus::WriteRequestSignal*>(this)->operator&<S>(_s);
	}
};

struct EraseRequest: Dynamic<EraseRequest, distributed::consensus::WriteRequestSignal>{
	EraseRequest(const std::string&);
	EraseRequest();
	
	/*virtual*/ void sendThisToConsensusObject();
	
	template <class S>
	S& operator&(S &_s){
		return static_cast<distributed::consensus::WriteRequestSignal*>(this)->operator&<S>(_s);
	}
};

#endif
