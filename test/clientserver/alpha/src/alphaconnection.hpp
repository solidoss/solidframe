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

#include "core/tstring.hpp"
#include "core/connection.hpp"
#include "core/common.hpp"

#include "alphareader.hpp"
#include "alphawriter.hpp"

#include "clientserver/core/commandableobject.hpp"

class SocketAddress;

namespace clientserver{
class Visitor;
namespace tcp{
class Channel;
}
}

namespace test{
class Visitor;
class Server;
namespace alpha{

class Service;
class Command;

//! Alpha connection implementing the alpha protocol resembling somehow the IMAP protocol
/*!
	It uses a reader and a writer to implement a state machine for the 
	protocol communication. 
*/
class Connection: public clientserver::CommandableObject<test::Connection>{
public:
	typedef clientserver::CommandableObject<test::Connection> BaseTp;
	typedef Service	ServiceTp;
	static void initStatic(Server &_rs);
	Connection(clientserver::tcp::Channel *_pch, SocketAddress *_paddr = NULL);
	~Connection();
	//! The implementation of the protocol's state machine
	/*!
		The method will be called within a clientserver::SelectPool by an
		clientserver::tcp::ConnectionSelector.
	*/
	int execute(ulong _sig, TimeSpec &_tout);
	//! Dummy inmplementation
	int execute();
	//! Dummy inmplementation
	int accept(clientserver::Visitor &);
	
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
	
	//int sendStatusResponse(cmd::Responder &_rr, int _opt);
	//! Receive a stream as a response
	/*!
		Filters unexpected responses and forwards the stream to the current alpha command
	*/
	/*virtual*/ int receiveIStream(
		StreamPtr<IStream> &,
		const FileUidTp&,
		const RequestUidTp &_requid,
		int			_which,
		const ObjectUidTp &_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
	/*virtual*/ int receiveOStream(
		StreamPtr<OStream> &,
		const FileUidTp&,
		const RequestUidTp &_requid,
		int			_which,
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
	/*virtual*/ int receiveIOStream(
		StreamPtr<IOStream> &,
		const FileUidTp&,
		const RequestUidTp &_requid,
		int			_which,
		const ObjectUidTp &_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
	/*virtual*/ int receiveString(
		const String &_str, 
		const RequestUidTp &_requid,
		int			_which,
		const ObjectUidTp &_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
	/*virtual*/ int receiveNumber(
		const int64 &_no, 
		const RequestUidTp &_requid,
		int			_which,
		const ObjectUidTp &_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
	/*virtual*/ int receiveData(
		void *_pdata,
		int	_datasz,
		const RequestUidTp &_requid,
		int			_which = 0,
		const ObjectUidTp&_from = ObjectUidTp(),
		const clientserver::ipc::ConnectorUid *_conid = NULL
	);
	/*virtual*/ int receiveError(
		int _errid, 
		const RequestUidTp &_requid,
		const ObjectUidTp &_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
private:
	void prepareReader();
private:
	enum {
		Init,
		ParsePrepare,
		ParseTout,
		Parse,
		ExecutePrepare,
		ExecuteTout,
		Execute,
		IdleExecute,
		Connect,
		ConnectTout
	};
	protocol::Logger	logger;
	Writer				wtr;
	Reader				rdr;
	Command				*pcmd;
	SocketAddress		*paddr;
	uint32				reqid;
};

}//namespace alpha
}//namespace test

#endif
