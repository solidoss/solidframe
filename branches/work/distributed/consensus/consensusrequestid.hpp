/* Declarations file consensusrequestid.hpp
	
	Copyright 2011, 2012 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DISTRIBUTED_CONCEPT_SIGNALREQUEST_IDENTIFIER_HPP
#define DISTRIBUTED_CONCEPT_SIGNALREQUEST_IDENTIFIER_HPP

#include "foundation/ipc/ipcconnectionuid.hpp"
#include "foundation/common.hpp"
#include <ostream>

namespace distributed{
namespace consensus{
//! A distributed consensus request identifier
/*!
 * RequestId is a way to uniquely identify requests comming from
 * clients by a distributed::consensus::Object. It contains:<br>
 * - senderuid: the identifier for a foundation::Object within a process<br>
 * - requid: an id incrementaly given by the sending foundation::Object,
 * to every Request<br>
 * - sockaddr: the foundation::ipc base address<br>
 * 
 * \see example/distributed/consensus for a proof-of-concept
 * 
 */
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
