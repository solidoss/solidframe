#ifndef CONSENSUSREQUESTS_HPP
#define CONSENSUSREQUESTS_HPP

#include "consensus/consensusrequest.hpp"

#include <string>

namespace std{

template <class S>
S& operator&(pair<std::string, solid::int64> &_v, S &_s){
	return _s.push(_v.first, "first").push(_v.second, "second");
}

template <class S>
S& operator&(solid::frame::ObjectUidT &_v, S &_s){
	return _s.push(_v.first, "first").push(_v.second, "second");
}
}

void mapSignals();


struct StoreRequest: solid::Dynamic<StoreRequest, solid::consensus::WriteRequestMessage>{
	StoreRequest(const std::string&, solid::uint32 _pos);
	StoreRequest();
	
	/*virtual*/ void notifyConsensusObjectWithThis();
	/*virtual*/ void notifySenderObjectWithThis();
	/*virtual*/ void notifySenderObjectWithFail();
	
	template <class S>
	S& operator&(S &_s){
		static_cast<solid::consensus::WriteRequestMessage*>(this)->operator&<S>(_s);
		_s.push(v,"value");
		return _s;
	}
	solid::uint32	v;
};

struct FetchRequest: solid::Dynamic<FetchRequest, solid::consensus::WriteRequestMessage>{
	FetchRequest(const std::string&);
    FetchRequest();
	
	/*virtual*/ void notifyConsensusObjectWithThis();
	/*virtual*/ void notifySenderObjectWithThis();
	/*virtual*/ void notifySenderObjectWithFail();
	
	template <class S>
	S& operator&(S &_s){
		return static_cast<solid::consensus::WriteRequestMessage*>(this)->operator&<S>(_s);
	}
};

struct EraseRequest: solid::Dynamic<EraseRequest, solid::consensus::WriteRequestMessage>{
	EraseRequest(const std::string&);
	EraseRequest();
	
	/*virtual*/ void notifyConsensusObjectWithThis();
	/*virtual*/ void notifySenderObjectWithThis();
	/*virtual*/ void notifySenderObjectWithFail();
	
	template <class S>
	S& operator&(S &_s){
		return static_cast<solid::consensus::WriteRequestMessage*>(this)->operator&<S>(_s);
	}
};

#endif
