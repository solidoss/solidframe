// frame/ipc/ipccontext.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_IPC_CONTEXT_HPP
#define SOLID_FRAME_IPC_IPC_CONTEXT_HPP

#include "system/socketaddress.hpp"

#include "utility/dynamicpointer.hpp"
#include "boost/any.hpp"

#include <ostream>

#include "frame/common.hpp"

namespace solid{
namespace frame{
namespace ipc{

//! A structure to uniquely indetify an IPC connection pool
/*!
	<b>Overview:</b><br>
	
	<b>Usage:</b><br>
	
*/
struct ConnectionPoolUid: UniqueId{
	ConnectionPoolUid(
		const size_t _idx = -1,
		const uint32 _uid = -1
	):UniqueId(_idx, _uid){}
	
};

struct ConnectionUid{
	
	ConnectionUid(
		const ConnectionPoolUid &_rpoolid = ConnectionPoolUid(),
		const ObjectUidT &_rconid = ObjectUidT()
	):poolid(_rpoolid), connectionid(_rconid){}
	
	bool isInvalid()const{
		return isInvalidConnection() || isInvalidPool();
	}
	
	bool isInvalidConnection()const{
		return connectionid.isInvalid();
	}
	
	bool isInvalidPool()const{
		return poolid.isInvalid();
	}
	
	ConnectionPoolUid	poolid;
	ObjectUidT			connectionid;
};


std::ostream& operator<<(std::ostream &_ros, ConnectionUid const &_con_id);

struct RequestUid{
	uint32	index;
	uint32	unique;
	
	RequestUid(
		const uint32 _idx = -1,
		const uint32 _uid = -1
	):index(_idx), unique(_uid){}
};

std::ostream& operator<<(std::ostream &_ros, RequestUid const &_msguid);

struct MessageUid{
	size_t		index;
	uint32		unique;
	
	MessageUid(
		const size_t _idx = -1,
		const uint32 _uid = 0
	):index(_idx), unique(_uid){}
	
	bool isInvalid()const{
		return index == static_cast<size_t>(-1);
	}
	bool isValid()const{
		return !isInvalid();
	}
};

class Service;
class Connection;

struct ConnectionContext{
	Service& service()const{
		return rservice;
	}
	
	ConnectionUid	connectionId()const;
	
	SocketDevice const & device()const;
	
	ulong messageFlags()const{
		return message_flags;
	}
	RequestUid const& requestUid()const{
		return request_uid;
	}
	//! Keep any connection data
	boost::any& any();
private:
	friend class Connection;
	friend class MessageWriter;
	friend struct Message;
	friend class TestEntryway;
	
	Service				&rservice;
	Connection			&rconnection;
	
	ulong				message_flags;
	uint8				message_state;
	RequestUid			request_uid;
	
	
	
	ConnectionContext(
		Service &_rsrv, Connection &_rcon
	):rservice(_rsrv), rconnection(_rcon), message_flags(0), message_state(0){}
	
	ConnectionContext(ConnectionContext const&);
	ConnectionContext& operator=(ConnectionContext const&);
};

}//namespace ipc
}//namespace frame
}//namespace solid
#endif
