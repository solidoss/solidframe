#ifndef DISTRIBUTED_CONCEPT_SIGNALREQUEST_IDENTIFIER_HPP
#define DISTRIBUTED_CONCEPT_SIGNALREQUEST_IDENTIFIER_HPP

#include "foundation/ipc/ipcconnectionuid.hpp"
#include "foundation/common.hpp"
#include <ostream>

namespace distributed{
namespace consensus{

struct RequestId{
	RequestId():requid(-1){}
	
	template <class S>
	S& operator&(S &_s){
		_s.push(requid, "opp").push(requid, "reqid");
		_s.push(senderuid.first, "senderuid_first");
		_s.push(senderuid.second, "senderuid_second");
		_s.pushBinary(sockaddr.addr(), SocketAddress4::Capacity, "sockaddr");
		return _s;
	}
	
	bool operator<(const RequestId &_rcsi)const;
	bool operator==(const RequestId &_rcsi)const;
	size_t hash()const;
	bool senderEqual(const RequestId &_rcsi)const;
	bool senderLess(const RequestId &_rcsi)const;
	size_t senderHash()const;
	
	uint32 						requid;
	foundation::ObjectUidT		senderuid;
	SocketAddress4				sockaddr;
};

std::ostream &operator<<(std::ostream& _ros, const RequestId &_rreqid);

}//namespace consensus
}//namespace distributed

#endif
