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
namespace solid{
namespace frame{
namespace ipc{
class Service;
}//namespace ipc
}//namespace frame
}//namespace solid

void mapSignals(solid::frame::ipc::Service &);


struct StoreRequest: solid::Dynamic<StoreRequest, solid::consensus::WriteRequestMessage>{
	StoreRequest(const std::string&, solid::uint32 _pos);
	StoreRequest();
	
	/*virtual*/ void consensusNotifyServerWithThis();
	/*virtual*/ void consensusNotifyClientWithThis();
	/*virtual*/ void consensusNotifyClientWithFail();
	
	template <class S>
	S& serialize(S &_s, solid::frame::ipc::ConnectionContext const &_rctx){
		static_cast<solid::consensus::WriteRequestMessage*>(this)->serialize<S>(_s, _rctx);
		_s.push(v,"value");
		return _s;
	}
	solid::uint32	v;
};

struct FetchRequest: solid::Dynamic<FetchRequest, solid::consensus::WriteRequestMessage>{
	FetchRequest(const std::string&);
    FetchRequest();
	
	/*virtual*/ void consensusNotifyServerWithThis();
	/*virtual*/ void consensusNotifyClientWithThis();
	/*virtual*/ void consensusNotifyClientWithFail();
	
	template <class S>
	S& serialize(S &_s, solid::frame::ipc::ConnectionContext const &_rctx){
		return static_cast<solid::consensus::WriteRequestMessage*>(this)->serialize<S>(_s, _rctx);
	}
};

struct EraseRequest: solid::Dynamic<EraseRequest, solid::consensus::WriteRequestMessage>{
	EraseRequest(const std::string&);
	EraseRequest();
	
	/*virtual*/ void consensusNotifyServerWithThis();
	/*virtual*/ void consensusNotifyClientWithThis();
	/*virtual*/ void consensusNotifyClientWithFail();
	
	template <class S>
	S& serialize(S &_s, solid::frame::ipc::ConnectionContext const &_rctx){
		return static_cast<solid::consensus::WriteRequestMessage*>(this)->serialize<S>(_s, _rctx);
	}
};

#endif
