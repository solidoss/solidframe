/* Declarations file alphaconnection.h
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ALPHACONNECTION_H
#define ALPHACONNECTION_H

#include "core/tstring.h"
#include "core/connection.h"

#include "alphareader.h"
#include "alphawriter.h"

#include "clientserver/core/commandableobject.h"

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


class Connection: public clientserver::CommandableObject<test::Connection>{
public:
	typedef clientserver::CommandableObject<test::Connection> BaseTp;
	typedef Service	ServiceTp;
	static void initStatic(Server &_rs);
	Connection(clientserver::tcp::Channel *_pch, SocketAddress *_paddr = NULL);
	~Connection();
	int execute(ulong _sig, TimeSpec &_tout);
	int execute();
	int accept(clientserver::Visitor &);
	
	Command* create(const String& _name);
	
	Reader& reader(){return rdr;}
	Writer& writer(){return wtr;}
	
	
	//int sendStatusResponse(cmd::Responder &_rr, int _opt);
	/*virtual*/ int receiveIStream(
		StreamPtr<IStream> &,
		const FromPairTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
	/*virtual*/ int receiveOStream(
		StreamPtr<OStream> &,
		const FromPairTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
	/*virtual*/ int receiveIOStream(
		StreamPtr<IOStream> &, 
		const FromPairTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
	/*virtual*/ int receiveString(
		const String &_str, 
		const FromPairTp&_from,
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
};

}//namespace alpha
}//namespace test

#endif
