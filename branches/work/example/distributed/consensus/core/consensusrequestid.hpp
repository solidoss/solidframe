#ifndef DISTRIBUTED_CONCEPT_CORE_SIGNAL_IDENTIFIER_HPP
#define DISTRIBUTED_CONCEPT_CORE_SIGNAL_IDENTIFIER_HPP

#include "foundation/ipc/ipcconnectionuid.hpp"
#include "foundation/common.hpp"

namespace consensus{

struct RequestId{
	RequestId():requid(-1){}
	
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

}//namespace consensus

#endif
