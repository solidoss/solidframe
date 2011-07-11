#ifndef CONSENSUSREQUESTS_HPP
#define CONSENSUSREQUESTS_HPP

#include "consensusrequest.hpp"

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


struct StoreRequest: Dynamic<StoreRequest, consensus::RequestSignal>{
	StoreRequest(const std::string&, uint32 _pos);
	StoreRequest();
	
	template <class S>
	S& operator&(S &_s){
		static_cast<consensus::RequestSignal*>(this)->operator&<S>(_s);
		_s.push(v,"value");
		return _s;
	}
	uint32	v;
};

struct FetchRequest: Dynamic<FetchRequest, consensus::RequestSignal>{
	FetchRequest(const std::string&);
    FetchRequest();
	template <class S>
	S& operator&(S &_s){
		return static_cast<consensus::RequestSignal*>(this)->operator&<S>(_s);
	}
};

struct EraseRequest: Dynamic<EraseRequest, consensus::RequestSignal>{
	EraseRequest(const std::string&);
	EraseRequest();
	template <class S>
	S& operator&(S &_s){
		return static_cast<consensus::RequestSignal*>(this)->operator&<S>(_s);
	}
};

#endif
