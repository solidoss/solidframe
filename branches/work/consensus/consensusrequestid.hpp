// consensus/consensusrequestid.hpp
//
// Copyright (c) 2011, 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_CONSENSUS_REQUESTID_HPP
#define SOLID_CONSENSUS_REQUESTID_HPP

#include "frame/ipc/ipcconnectionuid.hpp"
#include "frame/common.hpp"
#include <ostream>

namespace solid{
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
	RequestId(const uint32 _requid, frame::ObjectUidT const &_objuid):requid(_requid), senderuid(_objuid){}
	
	template <class S>
	S& serialize(S &_s, frame::ipc::ConnectionContext const &/*_rctx*/){
		_s.push(requid, "opp").push(requid, "reqid");
		_s.push(senderuid.first, "senderuid_first");
		_s.push(senderuid.second, "senderuid_second");
		const SocketAddressInet4 &rsa = sockaddr;
		_s.pushBinary((void*)rsa.sockAddr(), SocketAddressInet4::Capacity, "sockaddr");
		return _s;
	}
	
	bool operator<(const RequestId &_rcsi)const;
	bool operator==(const RequestId &_rcsi)const;
	size_t hash()const;
	bool senderEqual(const RequestId &_rcsi)const;
	bool senderLess(const RequestId &_rcsi)const;
	size_t senderHash()const;
	
	uint32 						requid;
	frame::ObjectUidT			senderuid;
	SocketAddressInet4			sockaddr;
};

std::ostream &operator<<(std::ostream& _ros, const RequestId &_rreqid);

}//namespace consensus
}//namespace solid

#endif
