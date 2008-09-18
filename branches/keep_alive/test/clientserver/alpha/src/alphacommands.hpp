/* Declarations file alphacommands.hpp
	
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

#ifndef ALPHA_COMMANDS_HPP
#define ALPHA_COMMANDS_HPP

#include <list>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/exception.hpp>
#include <boost/utility.hpp>

#include "utility/queue.hpp"
#include "utility/streamptr.hpp"
#include "utility/istream.hpp"
#include "utility/ostream.hpp"
#include "core/common.hpp"
#include "alphacommand.hpp"

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
/*!
	Syntax:<br>
	tag SP LOGIN SP astring = nume SP astring = password CRLF<br>
*/
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
/*!
	Syntax:<br>
	tag SP FETCH SP astring = path [SP astring = peer_address [SP number = peer_ipc_base_port]] CRLF<br>
	Example:<br>
	aa fetch "/tmp/00001.txt"<br>
	aa fetch "/tmp/00001.txt" homehost 1222<br>
*/
class Fetch: public Command{
public:
	Fetch(Connection &);
	~Fetch();
	void initReader(Reader &);
	int execute(Connection &);
	//! Writer plugin
	/*!
		Implements the logic for requesting a file from FileManager, 
		eventually wait for it, write the file on socket.<br>
		
		For remote fetch the things are more complicated as we will
		use a local temp file as buffer and:<br>
		- request a temp stream
		- when we've got the temp stream, reques the size and the first 1MB of file
		- when we've received the size and the firs 1MB, request for the next 1MB
			and write the first 1MB on socket
		- and so forth
	*/
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
	int16 				st;
	protocol::Parameter	*pp;
	uint64				litsz64;
	uint64				chunksz;
	uint16				isfirst;
	clientserver::ipc::ConnectorUid conuid;
};
//! Store a file locally
/*!
	Syntax:<br>
	tag SP STORE SP astring = path SP '{' number = literalsize '}' CRLF literal_data CRLF<br>
	
*/
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

//! Lists one level of the requested path
/*!
	Syntax:<br>
	tag SP LIST SP astring = path [SP astring = peer_ipc_address] [SP number=peer_ipc_port]<br>
	
	If the path is a directory, the direct cildren are displayed.
	If the path is a file, the file information is displayed.
*/
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

//! Lists one level of the requested remote path
/*!
	Syntax:<br>
	tag SP REMOTELIST SP astring = path SP astring = peer_ipc_address SP number=peer_ipc_port<br>
	
	If the path is a directory, the direct cildren are displayed.
	If the path is a file, the file information is displayed.
*/
class RemoteList: public Command{
public:
	enum {Wait, SendList, SendError, SendListContinue};
	//typedef std::list<std::pair<String,int64> > PathListTp;
	struct PathListTp: std::list<std::pair<String,int64> >{
		PathListTp();
		~PathListTp();
	};
	RemoteList();
	~RemoteList();
	void initReader(Reader &);
	int execute(Connection &);
	
	int reinitWriter(Writer &, protocol::Parameter &);
	int receiveData(
		void *_pdata,
		int _datasz,
		int			_which, 
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
	int receiveError(
		int _errid, 
		const ObjectUidTp&_from,
		const clientserver::ipc::ConnectorUid *_conid
	);
private:
	String					strpth;
	String					straddr;
	uint32					port;
	PathListTp				*ppthlst;
	PathListTp::const_iterator it;
	int						state;
	protocol::Parameter		*pp;
};


//! Send a string to a remote alpha connection to other test server
/*!
	The peer connection must be in idle!<br>
	Syntax:<br>
	tag SP SENDSTRING SP astring = peer_address SP number = peer_ipc_base_port SP
	number = peer_object_id SP number = peer_object_uid SP astring = data_to_send<br>
	
	Example:<br>
	aa sendstring homehost 1222 1111 2222 "some nice string"
*/
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
	ulong				objid;
	uint32				objuid;
};

//! Send a stream to a remote alpha connection to other test server
/*!
	The peer connection must be in idle!<br>
	Syntax:<br>
	tag SP SENDSTREAM SP astring = peer_address SP number = peer_ipc_base_port SP
	number = peer_object_id SP number = peer_object_uid SP astring = src_local_path SP
	astring = dest_path<br>
	
	Example:<br>
	aa sendsteam homehost 1222 1111 2222 "/data/movie.avi" "/tmp/movie.avi"
*/

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
	ulong				objid;
	uint32				objuid;
};

//! Wait for internal server events
/*!
	Use in combination with sendstream and sendstring.<br>
	Syntax:<br>
	tag SP IDLE CRLF <br>
	...events are displayed ...<br>
	done CRLF<br>
	
	So the command will both wait for "done" from client and for
	events.
*/
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
