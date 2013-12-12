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
	int reinitWriter(Writer &, solid::protocol::text::Parameter &);
	int receiveInputStream(
		solid::StreamPointer<solid::InputStream> &_sptr,
		const FileUidT &_fuid,
		int			_which,
		const ObjectUidT&,
		const solid::frame::ipc::ConnectionUid *
	);
	int receiveError(
		int _errid,
		const ObjectUidT&_from,
		const solid::frame::ipc::ConnectionUid *
	);
	int receiveNumber(
		const int64 &_no,
		int			_which,
		const ObjectUidT&_from,
		const solid::frame::ipc::ConnectionUid *_conid
	);
private:
	enum State{
		InitLocal,
		SendLocal,
		InitRemote,
		SendFirstData,
		SendNextData,
		WaitLocalStream,
		WaitTempStream,
		WaitRemoteStream,
		SendRemoteError,
		SendTempError,
		SendError,
		ReturnBad,
		ReturnOk,
		ReturnCrlf,
	};
	void doSendMaster(const FileUidT &_fuid);
	void doSendSlave(const FileUidT &_fuid);
	int doInitLocal();
	int doSendLiteral(Writer &_rw, bool _local);
	int doGetTempStream(uint32 _sz);
	int doSendFirstData(Writer &_rw);
	int doSendNextData(Writer &_rw);
private:
	solid::String								strpth;
	solid::String								straddr;
	solid::String								port;
	Connection									&rc;
	int16 										state;
	uint64										litsz;
	solid::protocol::text::Parameter					*pp;
	
	
	solid::StreamPointer<solid::InputStream>	sp_in;
	solid::StreamPointer<solid::InputStream>	sp_out;
	solid::InputStreamIterator					it;
	solid::frame::ipc::ConnectionUid			ipcconuid;
	MessageUidT									mastermsguid;
	uint32										tmpstreamcp;//temp stream capacity
	uint64										streamsz_out;
	uint32										streamsz_in;
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
	int execute(Connection &);
	int receiveOutputStream(
		solid::StreamPointer<solid::OutputStream> &_sptr,
		const FileUidT &_fuid,
		int			_which,
		const ObjectUidT&,
		const solid::frame::ipc::ConnectionUid *
	);
	int receiveError(
		int _errid,
		const ObjectUidT&_from,
		const solid::frame::ipc::ConnectionUid *
	);
	int reinitWriter(Writer &, solid::protocol::text::Parameter &);
private:
	solid::String								strpth;//the file path
	solid::StreamPointer<solid::OutputStream>	sp;
	solid::OutputStreamIterator					it;
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
	int execute(Connection &);
	
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
	int execute(Connection &);
	
	int reinitWriter(Writer &, solid::protocol::text::Parameter &);
	int receiveData(
		void *_pdata,
		int _datasz,
		int			_which, 
		const ObjectUidT&_from,
		const solid::frame::ipc::ConnectionUid *_conid
	);
	int receiveError(
		int _errid, 
		const ObjectUidT&_from,
		const solid::frame::ipc::ConnectionUid *_conid
	);
	template <int U>
	int reinitReader(Reader &, solid::protocol::text::Parameter &);
private:
	struct HostAddr{
		solid::String	addr;
		solid::String	port;
		uint32			netid;
	};
	typedef std::vector<HostAddr>	HostAddrVectorT;
	solid::String				strpth;
	HostAddrVectorT				hostvec;
	uint32						pausems;
	PathListT					*ppthlst;
	PathListT::const_iterator	it;
	int							state;
	solid::protocol::text::Parameter	*pp;
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
	int reinitWriter(Writer &, solid::protocol::text::Parameter &);
	/*virtual*/ int receiveInputStream(
		solid::StreamPointer<solid::InputStream> &,
		const FileUidT&,
		int			_which,
		const ObjectUidT&_from,
		const solid::frame::ipc::ConnectionUid *_conid
	);
// 	virtual int receiveOutputStream(
// 		StreamPointer<OutputStream> &,
// 		const FromPairT&_from,
// 		const ipc::ConnectionUid *_conid
// 	);
// 	virtual int receiveInputOutputStream(
// 		StreamPointer<InputOutputStream> &, 
// 		const FromPairT&_from,
// 		const ipc::ConnectionUid *_conid
// 	);
	/*virtual*/ int receiveString(
		const solid::String &_str,
		int			_which,
		const ObjectUidT&_from,
		const solid::frame::ipc::ConnectionUid *_conid
	);
private:
	enum Type{LocalStringType, PeerStringType, LocalStreamType, PeerStreamType};
	solid::Queue<Type>										typeq;
	solid::Queue<solid::String>								stringq;
	solid::Queue<ObjectUidT>								fromq;
	solid::Queue<solid::frame::ipc::ConnectionUid>			conidq;
	solid::Queue<solid::StreamPointer<solid::InputStream> >	streamq;
	Connection												&rc;
	solid::InputStreamIterator								it;
	uint64													litsz64;
};

}//namespace alpha
}//namespace concept


#endif
