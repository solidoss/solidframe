// alphacommands.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef ALPHA_COMMANDS_HPP
#define ALPHA_COMMANDS_HPP

#include <list>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/exception.hpp>
#include <boost/utility.hpp>

#include "utility/queue.hpp"
#include "utility/streampointer.hpp"
#include "utility/istream.hpp"
#include "utility/ostream.hpp"

#include "frame/ipc/ipcconnectionuid.hpp"
#include "frame/file/filestream.hpp"

#include "core/common.hpp"
#include "alphacommand.hpp"

namespace fs = boost::filesystem;
using boost::filesystem::path;
using boost::next;
using boost::prior;

using solid::uint32;
using solid::int32;
using solid::uint16;
using solid::int16;
using solid::uint64;
using solid::int64;

namespace solid{
namespace protocol{
namespace text{
struct Parameter;
}
}
}

namespace concept{
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
	void execute(Connection &_rc);
private:
	void execNoop(Connection &_rc);
	void execLogout(Connection &_rc);
	void execCapability(Connection &_rc);
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
	void execute(Connection &);
private:
	solid::String user,pass;
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
	void execute(Connection &);
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
	int reinitWriter(Writer &, solid::protocol::text::Parameter &);
	int receiveFilePointer(
		FilePointerMessage &_rmsg
	);
	/*virtual*/ int receiveMessage(solid::DynamicPointer<FetchSlaveMessage> &_rmsgptr);
private:
	enum State{
		InitLocal,
		SendLocal,
		InitRemote,
		SendFirstData,
		SendNextData,
		WaitLocalStream,
		WaitTempStream,
		WaitFirstRemoteStream,
		WaitRemoteStream,
		SendRemoteError,
		SendTempError,
		SendError,
		ReturnBad,
		ReturnOk,
		ReturnCrlf,
	};
	void doSendMaster(const solid::frame::UidT &_ruid);
	int doInitLocal();
	int doSendLiteral(Writer &_rw, bool _local);
	int doGetTempStream(uint32 _sz);
	int doSendFirstData(Writer &_rw);
	int doSendNextData(Writer &_rw);
private:
	typedef solid::frame::file::FileIOStream<256>		FileIOStreamT;
	solid::String								strpth;
	solid::String								straddr;
	solid::String								port;
	Connection									&rc;
	int16 										state;
	uint64										litsz;
	solid::protocol::text::Parameter			*pp;
	
	FileIOStreamT								ios;
	
	solid::frame::UidT							mastermsguid;
	uint32										tmpstreamcp;//temp stream capacity
	uint64										streamsz;
	uint32										streamcp;
	solid::DynamicPointer<FetchSlaveMessage>	cachedmsg;
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
	int reinitReader(Reader &, solid::protocol::text::Parameter &);
	void execute(Connection &);
	int receiveFilePointer(FilePointerMessage &_rmsg);
	int reinitWriter(Writer &, solid::protocol::text::Parameter &);
private:
	typedef solid::frame::file::FileOStream<1024>		FileOStreamT;
	solid::String								strpth;//the file path
	FileOStreamT								os;
	Connection									&rc;	
	int 										st;
	uint32										litsz;
	uint64										litsz64;
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
	void execute(Connection &);
	
	int reinitWriter(Writer &, solid::protocol::text::Parameter &);
private:
	solid::String			strpth;
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
	//typedef std::list<std::pair<String,int64> > PathListT;
	struct PathListT: std::list<std::pair<solid::String,int64> >{
		PathListT();
		~PathListT();
	};
	RemoteList();
	~RemoteList();
	void initReader(Reader &);
	void execute(Connection &);
	
	int reinitWriter(Writer &, solid::protocol::text::Parameter &);
	template <int U>
	int reinitReader(Reader &, solid::protocol::text::Parameter &);
	
private:
	/*virtual*/ int receiveMessage(solid::DynamicPointer<RemoteListMessage> &_rmsgptr);
private:
	struct HostAddr{
		solid::String	addr;
		solid::String	port;
		uint32			netid;
	};
	typedef std::vector<HostAddr>				HostAddrVectorT;
	typedef solid::protocol::text::Parameter	ParameterT;
	solid::String				strpth;
	HostAddrVectorT				hostvec;
	uint32						pausems;
	PathListT					*ppthlst;
	PathListT::const_iterator	it;
	int							state;
	ParameterT					*pp;
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
	void execute(Connection &);
	int reinitWriter(Writer &, solid::protocol::text::Parameter &);
	/*virtual*/ int receiveFilePointer(
		FilePointerMessage &_rmsg
	);
	int receiveString(
		const solid::String &_str,
		int			_which,
		const ObjectUidT&_from,
		const solid::frame::ipc::ConnectionUid *_conid
	);
private:
	typedef solid::frame::file::FileIStream<1024>		FileIStreamT;
	typedef solid::frame::file::FilePointerT			FilePointerT;
	enum Type{LocalStringType, PeerStringType, LocalStreamType, PeerStreamType};
	solid::Queue<Type>										typeq;
	solid::Queue<solid::String>								stringq;
	solid::Queue<ObjectUidT>								fromq;
	solid::Queue<solid::frame::ipc::ConnectionUid>			conidq;
	solid::Queue<FilePointerT>								fileptrq;
	Connection												&rc;
	FileIStreamT											is;
	uint64													litsz64;
};

}//namespace alpha
}//namespace concept


#endif
