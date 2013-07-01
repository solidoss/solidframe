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
#include "frame/aio/aiosingleobject.hpp"

#include "core/tstring.hpp"
#include "core/common.hpp"

#include "alphareader.hpp"
#include "alphawriter.hpp"

#include "system/socketaddress.hpp"

using solid::uint32;


namespace concept{

class Manager;

//signals
struct InputStreamMessage;
struct OutputStreamMessage;
struct InputOutputStreamMessage;
struct StreamErrorMessage;


namespace alpha{

class Service;
class Command;

//messages:
struct RemoteListMessage;
struct FetchSlaveMessage;
struct SendStringMessage;
struct SendStreamMessage;

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
	typedef solid::DynamicHandler<void, Connection>	DynamicHandlerT;
	//typedef DynamicHandler<void, Connection, DynamicDefaultPointerStore, void>	DynamicHandlerT;
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
	
	//! The implementation of the protocol's state machine
	/*!
		The method will be called within a solid::frame::SelectPool by an
		solid::frame::aio::Selector.
	*/
	int execute(ulong _sig, solid::TimeSpec &_tout);
	
	void state(int _st){
		st = _st;
	}
	int state()const{
		return st;
	}
	
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
	void dynamicHandle(solid::DynamicPointer<> &_dp);
	void dynamicHandle(solid::DynamicPointer<RemoteListMessage> &_rmsgptr);
	void dynamicHandle(solid::DynamicPointer<FetchSlaveMessage> &_rmsgptr);
	void dynamicHandle(solid::DynamicPointer<SendStringMessage> &_rmsgptr);
	void dynamicHandle(solid::DynamicPointer<SendStreamMessage> &_rmsgptr);
	void dynamicHandle(solid::DynamicPointer<InputStreamMessage> &_rmsgptr);
	void dynamicHandle(solid::DynamicPointer<OutputStreamMessage> &_rmsgptr);
	void dynamicHandle(solid::DynamicPointer<InputOutputStreamMessage> &_rmsgptr);
	void dynamicHandle(solid::DynamicPointer<StreamErrorMessage> &_rmsgptr);
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
	solid::ResolveData			ai;
	solid::ResolveIterator		aiit;
	uint32						reqid;
	DynamicHandlerT				dh;
	int							st;
};

}//namespace alpha
}//namespace concept

#endif
