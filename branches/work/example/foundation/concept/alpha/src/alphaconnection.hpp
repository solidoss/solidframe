/* Declarations file alphaconnection.hpp
	
	Copyright 2007, 2008 Valentin Palade 
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

#ifndef ALPHACONNECTION_HPP
#define ALPHACONNECTION_HPP

#include "utility/dynamictype.hpp"
#include "foundation/aio/aiosingleobject.hpp"

#include "core/tstring.hpp"
#include "core/common.hpp"

#include "alphareader.hpp"
#include "alphawriter.hpp"

#include "system/socketaddress.hpp"

namespace foundation{

class Visitor;

}

namespace concept{

class Visitor;
class Manager;

//signals
struct InputStreamSignal;
struct OutputStreamSignal;
struct InputOutputStreamSignal;
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
class Connection: public Dynamic<Connection, foundation::aio::SingleObject>{
	typedef DynamicExecuter<void, Connection, foundation::DynamicServicePointerStore, void>	DynamicExecuterT;
	//typedef DynamicExecuter<void, Connection, DynamicDefaultPointerStore, void>	DynamicExecuterT;
public:
	typedef Service	ServiceT;
	
	static void initStatic(Manager &_rm);
	static void dynamicRegister();
	static Connection& the(){
		return static_cast<Connection&>(Object::the());
	}
	
	Connection(SocketAddressInfo &_rai);
	Connection(const SocketDevice &_rsd);
	
	~Connection();
	
	/*virtual*/ bool signal(DynamicPointer<foundation::Signal> &_sig);
	
	//! The implementation of the protocol's state machine
	/*!
		The method will be called within a foundation::SelectPool by an
		foundation::aio::Selector.
	*/
	int execute(ulong _sig, TimeSpec &_tout);
	//! Dummy inmplementation
	int execute();
	//! Dummy inmplementation
	int accept(foundation::Visitor &);
	
	//! creator method for new commands
	Command* create(const String& _name, Reader &_rr);
	
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
	void dynamicExecute(DynamicPointer<> &_dp);
	void dynamicExecute(DynamicPointer<RemoteListSignal> &_rsig);
	void dynamicExecute(DynamicPointer<FetchSlaveSignal> &_rsig);
	void dynamicExecute(DynamicPointer<SendStringSignal> &_rsig);
	void dynamicExecute(DynamicPointer<SendStreamSignal> &_rsig);
	void dynamicExecute(DynamicPointer<InputStreamSignal> &_rsig);
	void dynamicExecute(DynamicPointer<OutputStreamSignal> &_rsig);
	void dynamicExecute(DynamicPointer<InputOutputStreamSignal> &_rsig);
	void dynamicExecute(DynamicPointer<StreamErrorSignal> &_rsig);
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
		ConnectWait,
		ParseTout,
		ExecuteTout,
		ExecuteIOTout,
		
	};
	Logger						logger;
	Writer						wtr;
	Reader						rdr;
	Command						*pcmd;
	SocketAddressInfo			ai;
	SocketAddressInfoIterator	aiit;
	uint32						reqid;
	DynamicExecuterT			dr;
};

}//namespace alpha
}//namespace concept

#endif
