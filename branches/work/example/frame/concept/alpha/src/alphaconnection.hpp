// alphaconnection.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef ALPHACONNECTION_HPP
#define ALPHACONNECTION_HPP

#include "utility/dynamictype.hpp"
#include "frame/aio/aiosingleobject.hpp"

#include "core/tstring.hpp"
#include "core/common.hpp"

#include "alphareader.hpp"
#include "alphawriter.hpp"

#include "system/socketaddress.hpp"

using solid::uint32;


namespace concept{

class  Manager;
struct FilePointerMessage;

namespace alpha{

class Service;
class Command;

//messages:
struct RemoteListMessage;
struct FetchSlaveMessage;

class Logger: public solid::protocol::text::Logger{
protected:
	virtual void doInFlush(const char*, unsigned);
	virtual void doOutFlush(const char*, unsigned);
};

//! Alpha connection implementing the alpha protocol resembling somehow the IMAP protocol
/*!
	It uses a reader and a writer to implement a state machine for the 
	protocol communication. 
*/
class Connection: public solid::Dynamic<Connection, solid::frame::aio::SingleObject>{
	typedef solid::DynamicMapper<void, Connection>	DynamicMapperT;
public:
#ifdef UDEBUG
	typedef std::vector<Connection*> ConnectionsVectorT;
	static ConnectionsVectorT& connections(); 
#endif
	static void initStatic(Manager &_rm);
	static void dynamicRegister();
	static Connection& the(){
		return static_cast<Connection&>(Object::specific());
	}
	
	Connection(solid::ResolveData &_rai);
	Connection(const solid::SocketDevice &_rsd);
	
	~Connection();
	
	/*virtual*/ bool notify(solid::DynamicPointer<solid::frame::Message> &_rmsgptr);
	
	void state(int _st){
		st = _st;
	}
	int state()const{
		return st;
	}
	
	//! creator method for new commands
	Command* create(const solid::String& _name, Reader &_rr);
	
	Reader& reader(){return rdr;}
	Writer& writer(){return wtr;}
	//! Get the current request id.
	uint32 requestId()const{return reqid;}
	//! Generate a new request id.
	/*!
		Every request has an associated id.
		We want to prevent receiving unexpected responses.
		
		The commands that are not responses - like those received in idle state, come with
		the request id equal to zero so this must be skiped.
	*/
	uint32 newRequestId(){
		if(++reqid) return reqid;
		return (reqid = 1);
	}
	void dynamicHandle(solid::DynamicPointer<> &_dp);
	void dynamicHandle(solid::DynamicPointer<RemoteListMessage> &_rmsgptr);
	void dynamicHandle(solid::DynamicPointer<FetchSlaveMessage> &_rmsgptr);
	void dynamicHandle(solid::DynamicPointer<FilePointerMessage> &_rmsgptr);
private:
	/*virtual*/ void execute(ExecuteContext &_rexectx);
	void prepareReader();
private:
	enum {
		Init,
		Banner,
		ParsePrepare,
		Parse,
		ExecutePrepare,
		IdleExecute,
		Execute,
		Connect,
		ConnectWait,
		ParseTout,
		ExecuteTout,
		ExecuteIOTout,
		
	};
	typedef std::vector<solid::DynamicPointer<> >	DynamicPointerVectorT;
	
	static DynamicMapperT		dm;
	
	Logger						logger;
	Writer						wtr;
	Reader						rdr;
	Command						*pcmd;
	solid::ResolveData			ai;
	solid::ResolveIterator		aiit;
	uint32						reqid;
	int							st;
	DynamicPointerVectorT		dv;
};

}//namespace alpha
}//namespace concept

#endif
