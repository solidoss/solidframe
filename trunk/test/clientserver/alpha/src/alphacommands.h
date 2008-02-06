/* Declarations file alphacommands.h
	
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

#ifndef ALPHA_COMMANDS_H
#define ALPHA_COMMANDS_H

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/exception.hpp>
#include <boost/utility.hpp>

#include "utility/queue.h"
#include "utility/streamptr.h"
#include "utility/istream.h"
#include "utility/ostream.h"
#include "core/common.h"
#include "alphacommand.h"

namespace fs = boost::filesystem;
using boost::filesystem::path;
using boost::next;
using boost::prior;


namespace protocol{
struct Parameter;
}

namespace test{
namespace alpha{

class Connection;
class Reader;
class Writer;
//! Basic alpha commands - with no parameters.
class Basic: public Command{
public:
	enum Types{
		Noop, Logout, Capability
	};
	Basic(Types _tp);
	~Basic();
	void initReader(Reader &);
	int execute(Connection &_rc);
private:
	int execNoop(Connection &_rc);
	int execLogout(Connection &_rc);
	int execCapability(Connection &_rc);
private:
	Types tp;
};

//! Alpha Login command
class Login: public Command{
public:
	Login();
	~Login();
	void initReader(Reader &);
	int execute(Connection &);
private:
	String user,pass;
};
//! Alpha (remote) file fetch commad 
class Fetch: public Command{
public:
	Fetch(Connection &);
	~Fetch();
	void initReader(Reader &);
	int execute(Connection &);
	int reinitWriter(Writer &, protocol::Parameter &);
	int receiveIStream(
		StreamPtr<IStream> &_sptr,
		const FileUidTp &_fuid,
		int			_which,
		const ObjectUidTp&,
		const clientserver::ipc::ConnectorUid *
	);
	int receiveError(
		int _errid,
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *
	);
	int receiveNumber(
		const int64 &_no,
		int			_which,
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
private:
	enum {
		InitLocal, SendLocal, InitRemote,
		SendMasterRemote, SendFirstRemote, SendNextRemote, 
		SendError, SendTempError, WaitStreamLocal, WaitStreamRemote, WaitTempRemote,
		WaitFirstRemote, WaitNextRemote, ReturnBad,
	};

	String				strpth;
	String				straddr;
	FileUidTp			fuid;
	uint32				port;
	StreamPtr<IStream>	sp;
	IStreamIterator		it;
	Connection			&rc;
	CommandUidTp		mastercmduid;
	int 				st;
	protocol::Parameter	*pp;
	uint64				litsz64;
	uint64				chunksz;
	clientserver::ipc::ConnectorUid conuid;
};

class Store: public Command{
public:
	enum {Init, SendWait, PrepareReceive};
	Store(Connection &);
	~Store();
	void initReader(Reader &);
	int reinitReader(Reader &, protocol::Parameter &);
	int execute(Connection &);
	int receiveOStream(
		StreamPtr<OStream> &_sptr,
		const FileUidTp &_fuid,
		int			_which,
		const ObjectUidTp&,
		const clientserver::ipc::ConnectorUid *
	);
	int receiveError(
		int _errid,
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *
	);
	int reinitWriter(Writer &, protocol::Parameter &);
private:
	String				strpth;//the file path
	StreamPtr<OStream>	sp;
	OStreamIterator		it;
	Connection			&rc;
	int 				st;
	uint32				litsz;
	uint64				litsz64;
};

class List: public Command{
public:
	enum {Step, Stop};
	List();
	~List();
	void initReader(Reader &);
	int execute(Connection &);
	
	int reinitWriter(Writer &, protocol::Parameter &);
private:
	String					strpth;
	fs::directory_iterator 	it,end;
};

class SendString: public Command{
public:
	SendString();
	~SendString();
	void initReader(Reader &);
	int execute(Connection &);
private:
	String				str;
	String 				addr;
	uint32				port;
	uint32				objid;
	uint32				objuid;
};

class SendStream: public Command{
public:
	SendStream();
	~SendStream();
	void initReader(Reader &);
	int execute(Connection &);
private:
	String				srcstr;
	String				dststr;
	String 				addr;
	uint32				port;
	uint32				objid;
	uint32				objuid;
};

class Idle: public Command{
public:
	Idle(Connection &_rc):rc(_rc){}
	~Idle();
	void initReader(Reader &);
	int execute(Connection &);
	int reinitWriter(Writer &, protocol::Parameter &);
	/*virtual*/ int receiveIStream(
		StreamPtr<IStream> &,
		const FileUidTp&,
		int			_which,
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
// 	virtual int receiveOStream(
// 		StreamPtr<OStream> &,
// 		const FromPairTp&_from,
// 		const ipc::ConnectorUid *_conid
// 	);
// 	virtual int receiveIOStream(
// 		StreamPtr<IOStream> &, 
// 		const FromPairTp&_from,
// 		const ipc::ConnectorUid *_conid
// 	);
	/*virtual*/ int receiveString(
		const String &_str,
		int			_which,
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
private:
	enum Type{LocalStringType, PeerStringType, LocalStreamType, PeerStreamType};
	Queue<Type>					typeq;
	Queue<String>				stringq;
	Queue<ObjectUidTp>			fromq;
	Queue<clientserver::ipc::ConnectorUid>	conidq;
	Queue<StreamPtr<IStream> >	streamq;
	Connection					&rc;
	IStreamIterator				it;
	uint64						litsz64;
};

}
}


#endif
