/* Declarations file alphaconnection.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ALPHACONNECTION_HPP
#define ALPHACONNECTION_HPP

#include "core/connection.hpp"
#include "core/tstring.hpp"
#include "core/common.hpp"

#include "alphareader.hpp"
#include "alphawriter.hpp"

#include "foundation/signalableobject.hpp"

class SocketAddress;

namespace foundation{

class Visitor;

namespace tcp{

class Channel;

}
}

namespace test{

class Visitor;
class Manager;

//signals
struct IStreamSignal;
struct OStreamSignal;
struct IOStreamSignal;
struct StreamErrorSignal;


namespace alpha{

class Service;
class Command;

//signals:
struct RemoteListSignal;
struct FetchSlaveSignal;
struct SendStringSignal;
struct SendStreamSignal;

class Logger: public protocol::Logger{
protected:
	virtual void doInFlush(const char*, unsigned);
	virtual void doOutFlush(const char*, unsigned);
};

//! Alpha connection implementing the alpha protocol resembling somehow the IMAP protocol
/*!
	It uses a reader and a writer to implement a state machine for the 
	protocol communication. 
*/
class Connection: public DynamicReceiver<Connection, foundation::SignalableObject<test::Connection> >{
public:
	//typedef foundation::CommandableObject<test::Connection> BaseTp;
	typedef Service	ServiceTp;
	
	static void initStatic(Manager &_rm);
	static void dynamicRegister(DynamicMap &_rdm);
	
	Connection(SocketAddress *_paddr);
	Connection(const SocketDevice &_rsd);
	
	~Connection();
	//! The implementation of the protocol's state machine
	/*!
		The method will be called within a foundation::SelectPool by an
		foundation::tcp::ConnectionSelector.
	*/
	int execute(ulong _sig, TimeSpec &_tout);
	//! Dummy inmplementation
	int execute();
	//! Dummy inmplementation
	int accept(foundation::Visitor &);
	
	//! creator method for new commands
	Command* create(const String& _name);
	
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
	
	int dynamicReceive(RemoteListSignal &_rsig);
	int dynamicReceive(FetchSlaveSignal &_rsig);
	int dynamicReceive(SendStringSignal &_rsig);
	int dynamicReceive(SendStreamSignal &_rsig);
	int dynamicReceive(IStreamSignal &_rsig);
	int dynamicReceive(OStreamSignal &_rsig);
	int dynamicReceive(IOStreamSignal &_rsig);
	int dynamicReceive(StreamErrorSignal &_rsig);
private:
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
		ConnectTout,
		ParseTout,
		ExecuteTout,
		ExecuteIOTout,
		
	};
	Logger				logger;
	Writer				wtr;
	Reader				rdr;
	Command				*pcmd;
	SocketAddress		*paddr;
	uint32				reqid;
};

}//namespace alpha
}//namespace test

#endif
