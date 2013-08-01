// betaservercommand.hpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef BETACLIENTCONNECTION_HPP
#define BETACLIENTCONNECTION_HPP


#include <vector>
#include "utility/queue.hpp"
#include "utility/stack.hpp"
#include "betaconnection.hpp"

using solid::uint16;
using solid::uint32;
using solid::uint64;

namespace concept{

class Manager;

namespace beta{

struct LoginMessage;
struct CancelMessage;

namespace client{

class Command;

class Connection: public solid::Dynamic<Connection, beta::Connection>{
	typedef solid::DynamicMapper<void, Connection>	DynamicMapperT;
public:
	static void initStatic(Manager &_rm);
	static void dynamicRegister();
	
	static Connection& the(){
		return static_cast<Connection&>(solid::frame::Object::specific());
	}
	
	Connection(const solid::ResolveData &_raddrinfo);
	
	~Connection();
	
	/*virtual*/ bool notify(solid::DynamicPointer<solid::frame::Message> &_rmsgptr);
	
	int execute(ulong _sig, solid::TimeSpec &_tout);
	
	uint32 requestId()const{return reqid;}
	uint32 newRequestId(){
		if(++reqid) return reqid;
		return (reqid = 1);
	}
	
	template <class S, uint32 I>
	int serializationReinit(S &_rs, const uint64 &_rv);
	void pushCommand(Command *_pcmd);
	uint32 commandUid(const uint32 _cmdidx)const;
	
	void dynamicHandle(solid::DynamicPointer<> &_dp);
	void dynamicHandle(solid::DynamicPointer<LoginMessage> &_rmsgptr);
	void dynamicHandle(solid::DynamicPointer<CancelMessage> &_rmsgptr);
private:
	bool useEncryption()const;
	bool useCompression()const;
	
	void state(int _st){
		st = _st;
	}
	int state()const{
		return st;
	}
	
	void doPrepareRun();
	/*virtual*/ int	doFillSendBufferData(char *_sendbufpos);
	/*virtual*/ bool doParseBufferData(const char *_pbuf, ulong _len);
	/*virtual*/ int doParseBufferException(const char *_pbuf, ulong _len);
private:
	enum {
		Init,
		ConnectPrepare,
		ConnectNext,
		Connect,
		ConnectWait,
		Run
	};
	
	struct CommandStub{
		CommandStub(Command *_pcmd = NULL, uint32 _uid = 0):pcmd(_pcmd), uid(_uid), sendtype(true){}
		Command *pcmd;
		uint32 	uid;
		bool	sendtype;
	};
	typedef std::vector<solid::DynamicPointer<> >	DynamicPointerVectorT;
	typedef std::vector<CommandStub>				CommandVectorT;
	typedef solid::Queue<uint32>					UInt32QueueT;
	typedef solid::Stack<uint32>					UInt32StackT;
	
	static DynamicMapperT		dm;
	
	solid::ResolveData			addrinfo;
	solid::ResolveIterator		addrit;
	int							st;
	uint32						reqid;
	DynamicPointerVectorT		dv;
	CommandVectorT				cmdvec;
	UInt32QueueT				cmdque;
	UInt32StackT				cmdvecfreestk;
	uint16						crtcmdsendtype;
};

inline uint32 Connection::commandUid(const uint32 _cmdidx)const{
	return cmdvec[_cmdidx].uid;
}


}//namespace client

}//namespace beta

}//namespace concept

#endif
