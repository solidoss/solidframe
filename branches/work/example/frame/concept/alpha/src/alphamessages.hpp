// alphasignalss.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef ALPHA_SIGNALS_HPP
#define ALPHA_SIGNALS_HPP

#include "frame/ipc/ipcmessage.hpp"
#include "frame/ipc/ipcconnectionuid.hpp"
#include "frame/file/filestream.hpp"

#include "serialization/binary.hpp"

#include "utility/dynamicpointer.hpp"

#include "alphacommands.hpp"

using namespace solid;

namespace solid{
namespace frame{
class MessageSteward;
namespace ipc{
struct ConnectionUid;
}
}
}

using solid::uint32;
using solid::int32;
using solid::uint64;
using solid::int64;
using solid::uint8;

///\cond 0
namespace solid{namespace serialization{namespace binary{

template <class S>
void serialize(S &_s, std::pair<String, int64> &_v){
	_s.push(_v.first, "first").push(_v.second, "second");
}

template <class S>
void serialize(S &_s, ObjectUidT &_v){
	_s.push(_v.first, "first").push(_v.second, "second");
}
/*binary*/}/*serialization*/}/*solid*/}
///\endcond

namespace concept{
namespace alpha{

struct MessageTypeIds{
	static const MessageTypeIds& the(const MessageTypeIds *_pids = NULL);
	uint32 fetchmastercommand;
	uint32 fetchslavecommand;
	uint32 fetchslaveresponse;
	uint32 remotelistcommand;
	uint32 remotelistresponse;
};

//---------------------------------------------------------------
// RemoteListMessage
//---------------------------------------------------------------
struct RemoteListMessage: Dynamic<RemoteListMessage, DynamicShared<frame::ipc::Message> >{
	RemoteListMessage(uint32 _tout = 0, uint16 _sentcnt = 1);
	RemoteListMessage(const NumberType<1>&);
	~RemoteListMessage();

	/*virtual*/ void ipcOnReceive(frame::ipc::ConnectionContext const &_rctx, MessagePointerT &_rmsgptr);
	/*virtual*/ uint32 ipcOnPrepare(frame::ipc::ConnectionContext const &_rctx);
	/*virtual*/ void ipcOnComplete(frame::ipc::ConnectionContext const &_rctx, int _err);
	
	size_t use();
	size_t release();

	template <class S>
	void serialize(S &_s, frame::ipc::ConnectionContext const &_rctx){
		_s.pushContainer(ppthlst, "strlst").push(err, "error").push(tout,"timeout");
		_s.push(requid, "requid").push(strpth, "strpth").push(fromv, "from");
	}
//data:
	RemoteList::PathListT				*ppthlst;
	String								strpth;
	int32								err;
	uint32								tout;
	solid::frame::ipc::ConnectionUid	conid;
	solid::frame::ipc::MessageUid		msguid;
	uint32								requid;
	ObjectUidT							fromv;
	uint8								success;
};

struct FetchSlaveMessage;


//---------------------------------------------------------------
// FetchMasterMessage
//---------------------------------------------------------------
/*
	This request is first sent to a peer's signal executer - where
	# it tries to get a stream,
	# sends a respose (FetchSlaveMessage) with the stream size and at most 1MB from the stream
	# waits for FetchSlaveMessage(s) to give em the rest of stream chunks
	# when the last stream chunk was sent it dies.
*/

struct FetchMasterMessage: Dynamic<FetchMasterMessage, solid::frame::ipc::Message>{
	enum{
		NotReceived,
		Received,
		SendFirstStream,
		SendNextStream,
		SendError,
	};
	FetchMasterMessage():pmsg(NULL), fromv(0xffffffff, 0xffffffff), state(NotReceived), streamsz(0), filesz(0), filepos(0), requid(0){
	}
	~FetchMasterMessage();
	/*virtual*/ void ipcOnReceive(frame::ipc::ConnectionContext const &_rctx, MessagePointerT &_rmsgptr);
	/*virtual*/ void ipcOnComplete(frame::ipc::ConnectionContext const &_rctx, int _err);
	
	template <class S>
	void serialize(S &_s, solid::frame::ipc::ConnectionContext const &_rctx){
		_s.push(fname, "filename");
		_s.push(tmpfuid.first, "tmpfileuid_first").push(tmpfuid.second, "tmpfileuid_second");
		_s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid").push(requid, "requestuid").push(streamsz, "streamsize");
	}
	void print()const;
//data:
	String								fname;
	FetchSlaveMessage					*pmsg;
	ObjectUidT							fromv;
	solid::frame::UidT					fuid;
	solid::frame::UidT					tmpfuid;
	solid::frame::ipc::ConnectionUid	conid;
	solid::frame::file::FilePointerT	fileptr;
	int32								state;
	uint32								streamsz;
	int64								filesz;
	int64								filepos;
	uint32								requid;
};

//---------------------------------------------------------------
// FetchSlaveMessage
//---------------------------------------------------------------
/*
	The signals sent from the alpha connection to the remote FetchMasterMessage
	to request new file chunks, and from FetchMasterMessage to the alpha connection
	as reponse containing the requested file chunk.
*/
struct FetchSlaveMessage: Dynamic<FetchSlaveMessage, solid::frame::ipc::Message>{
	FetchSlaveMessage();
	~FetchSlaveMessage();
	int sent(const solid::frame::ipc::ConnectionUid &);

	/*virtual*/ void ipcOnReceive(frame::ipc::ConnectionContext const &_rctx, MessagePointerT &_rmsgptr);
	
	template <class S>
	void serialize(S &_s, solid::frame::ipc::ConnectionContext const &_rctx){
		_s.template pushReinit<FetchSlaveMessage, 0>(this, 0, "reinit");
		_s.push(tov.first, "toobjectid").push(tov.second, "toobjectuid");
		//_s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid");
		_s.push(streamsz, "streamsize").push(requid, "requestuid");
		_s.push(filesz, "filesize").push(filepos, "filepos").push(msguid.first, "msguid_first").push(msguid.second, "msguid_second");
		_s.push(fuid.first,"fileuid_first").push(fuid.second, "fileuid_second");
		serialized = true;
	}
	
	template <class S, uint32 I>
	serialization::binary::CbkReturnValueE serializationReinit(S &_rs, const uint32 &_rv, solid::frame::ipc::ConnectionContext const &_rctx){
		if(_rv == 1){
			clearOutputStream();
			return serialization::binary::Success;
		}
		_rs.pop();
		if(S::IsSerializer){
			if(!ios.device().empty()){
				_rs.pushStream(static_cast<std::istream*>(&ios), filepos, streamsz, "stream");
			}
		}else{
			if(initOutputStream()){
				_rs.template pushReinit<FetchSlaveMessage, 0>(this, 1, "reinit");
				_rs.pushStream(static_cast<std::ostream*>(&ios), streampos, streamsz, "stream");
			}
		}
		return serialization::binary::Continue;
	}
	bool initOutputStream();
	void clearOutputStream();
	void print()const;
//data:	
	typedef solid::frame::file::FileIOStream<64>		FileIOStreamT;
	ObjectUidT							fromv;
	ObjectUidT							tov;
	solid::frame::UidT					fuid;
	solid::frame::ipc::ConnectionUid	conid;
	solid::frame::UidT					msguid;
	FileIOStreamT						ios;
	int64								filesz;
	int64								filepos;
	int32								streamsz;
	int32								streampos;
	uint32								requid;
	bool								serialized;
};
}//namespace alpha
}//namespace concept

#endif
