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

#include "frame/message.hpp"
#include "frame/ipc/ipcconnectionuid.hpp"

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
namespace std{

template <class S>
S& operator&(pair<String, int64> &_v, S &_s){
	return _s.push(_v.first, "first").push(_v.second, "second");
}

template <class S>
S& operator&(ObjectUidT &_v, S &_s){
	return _s.push(_v.first, "first").push(_v.second, "second");
}
}
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
struct RemoteListMessage: Dynamic<RemoteListMessage, DynamicShared<frame::Message> >{
	RemoteListMessage(uint32 _tout = 0, uint16 _sentcnt = 1);
	RemoteListMessage(const NumberType<1>&);
	~RemoteListMessage();
	int execute(
		DynamicPointer<Message> &_rmsgptr,
		uint32 _evs,
		frame::MessageSteward&,
		const MessageUidT &, solid::TimeSpec &_rts
	);
	
	/*virtual*/ void ipcReceive(
		frame::ipc::MessageUid &_rmsguid
	);
	/*virtual*/ uint32 ipcPrepare();
	/*virtual*/ void ipcComplete(int _err);
	
	size_t use();
	size_t release();

	template <class S>
	S& operator&(S &_s){
		_s.pushContainer(ppthlst, "strlst").push(err, "error").push(tout,"timeout");
		_s.push(requid, "requid").push(strpth, "strpth").push(fromv, "from");
		_s.push(ipcstatus, "ipcstatus");
		if(ipcstatus != IpcOnSender || S::IsDeserializer){
			_s.push(msguid.idx, "msguid.idx").push(msguid.uid,"msguid.uid");
		}else{//on sender
			solid::frame::ipc::MessageUid &rmsguid(
				const_cast<solid::frame::ipc::MessageUid &>(solid::frame::ipc::ConnectionContext::the().msgid)
			);
			_s.push(rmsguid.idx, "msguid.idx").push(rmsguid.uid,"msguid.uid");
		}
		return _s;
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
	uint8								ipcstatus;
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

struct FetchMasterMessage: Dynamic<FetchMasterMessage, solid::frame::Message>{
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
	/*virtual*/ uint32 ipcPrepare();
	/*virtual*/ void ipcReceive(
		solid::frame::ipc::MessageUid &_rmsguid
	);
	/*virtual*/ void ipcComplete(int _err);
	
	int execute(
		DynamicPointer<Message> &_rthis_ptr,
		uint32 _evs,
		solid::frame::MessageSteward&,
		const MessageUidT &, solid::TimeSpec &_rts
	);

	int receiveMessage(
		DynamicPointer<Message> &_rsig,
		const ObjectUidT& _from = ObjectUidT(),
		const solid::frame::ipc::ConnectionUid *_conid = NULL
	);
	
	template <class S>
	S& operator&(S &_s){
		_s.push(fname, "filename");
		_s.push(tmpfuid.first, "tmpfileuid_first").push(tmpfuid.second, "tmpfileuid_second");
		return _s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid").push(requid, "requestuid").push(streamsz, "streamsize");
	}
	void print()const;
//data:
	String								fname;
	FetchSlaveMessage					*pmsg;
	ObjectUidT							fromv;
	FileUidT							fuid;
	FileUidT							tmpfuid;
	solid::frame::ipc::ConnectionUid	conid;
	StreamPointer<InputStream>			ins;
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
struct FetchSlaveMessage: Dynamic<FetchSlaveMessage, solid::frame::Message>{
	FetchSlaveMessage();
	~FetchSlaveMessage();
	void ipcReceive(
		solid::frame::ipc::MessageUid &_rmsguid
	);
	int sent(const solid::frame::ipc::ConnectionUid &);
	//int execute(concept::Connection &);
	int execute(
		DynamicPointer<Message> &_rthis_ptr,
		uint32 _evs,
		solid::frame::MessageSteward&,
		const MessageUidT &, solid::TimeSpec &_rts
	);
	uint32 ipcPrepare();
	
	template <class S>
	S& operator&(S &_s){
		_s.template pushReinit<FetchSlaveMessage, 0>(this, 0, "reinit");
		_s.push(tov.first, "toobjectid").push(tov.second, "toobjectuid");
		//_s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid");
		_s.push(streamsz, "streamsize").push(requid, "requestuid");
		_s.push(filesz, "filesize").push(msguid.first, "msguid_first").push(msguid.second, "msguid_second");
		_s.push(fuid.first,"fileuid_first").push(fuid.second, "fileuid_second");
		serialized = true;
		return _s;
	}
	
	template <class S, uint32 I>
	int serializationReinit(S &_rs, const uint64 &_rv){
		if(_rv == 1){
			clearOutputStream();
			return OK;
		}
		if(S::IsSerializer){
			InputStream *ps = ins.get();
			//cassert(ps != NULL);
			_rs.pop();
			_rs.pushStream(ps, 0, streamsz, "stream");
		}else{
			initOutputStream();
			OutputStream *ps = outs.get();
			_rs.pop();
			_rs.template pushReinit<FetchSlaveMessage, 0>(this, 1, "reinit");
			_rs.pushStream(ps, (uint64)0, (uint64)streamsz, "stream");
		}
		return CONTINUE;
	}
	void initOutputStream();
	void clearOutputStream();
	void print()const;
//data:	
	ObjectUidT							fromv;
	ObjectUidT							tov;
	FileUidT							fuid;
	solid::frame::ipc::ConnectionUid	conid;
	MessageUidT							msguid;
	StreamPointer<InputStream>			ins;
	StreamPointer<OutputStream>			outs;
	int64								filesz;
	int32								streamsz;
	uint32								requid;
	bool								serialized;
};

//---------------------------------------------------------------
// SendStringMessage
//---------------------------------------------------------------
/*
	The signal sent to peer with the text
*/
struct SendStringMessage: Dynamic<SendStringMessage, solid::frame::Message>{
	SendStringMessage(){}
	SendStringMessage(
		const String &_str,
		ulong _toobjid,
		uint32 _toobjuid,
		ulong _fromobjid,
		uint32 _fromobjuid
	):str(_str), tov(_toobjid, _toobjuid), fromv(_fromobjid, _fromobjuid){}
	
	void ipcReceive(
		solid::frame::ipc::MessageUid &_rmsguid
	);
	template <class S>
	S& operator&(S &_s){
		_s.push(str, "string").push(tov.first, "toobjectid").push(tov.second, "toobjectuid");
		return _s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid");
	}
private:
	typedef std::pair<uint32, uint32> ObjPairT;
	String								str;
	ObjPairT							tov;
	ObjPairT							fromv;
	solid::frame::ipc::ConnectionUid	conid;
};

//---------------------------------------------------------------
// SendStreamMessage
//---------------------------------------------------------------
/*
	The signal sent to peer with the stream.
*/
struct SendStreamMessage: Dynamic<SendStreamMessage, solid::frame::Message>{
	SendStreamMessage(){}
	SendStreamMessage(
		StreamPointer<InputOutputStream> &_iosp,
		const String &_str,
		uint32 _myprocid,
		ulong _toobjid,
		uint32 _toobjuid,
		ulong _fromobjid,
		uint32 _fromobjuid
	):iosp(_iosp), dststr(_str), tov(_toobjid, _toobjuid), fromv(_fromobjid, _fromobjuid){}
	~SendStreamMessage(){
	}
	std::pair<uint32, uint32> to()const{return tov;}
	std::pair<uint32, uint32> from()const{return fromv;}
	void ipcReceive(
		solid::frame::ipc::MessageUid &_rmsguid
	);

	int createDeserializationStream(OutputStream *&_rpos, int64 &_rsz, uint64 &_roff, int _id);
	void destroyDeserializationStream(OutputStream *&_rpos, int64 &_rsz, uint64 &_roff, int _id);
	int createSerializationStream(InputStream *&_rpis, int64 &_rsz, uint64 &_roff, int _id);
	void destroySerializationStream(InputStream *&_rpis, int64 &_rsz, uint64 &_roff, int _id);
	
	template <class S>
	S& operator&(S &_s){
		_s.push(dststr, "dststr");
		_s.push(tov.first, "toobjectid").push(tov.second, "toobjectuid");
		return _s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid");
	}
private:
	typedef std::pair<uint32, uint32> 	ObjPairT;
	typedef std::pair<uint32, uint32> 	FileUidT;

	StreamPointer<InputOutputStream>	iosp;
	String								dststr;
	ObjPairT							tov;
	ObjPairT							fromv;
	solid::frame::ipc::ConnectionUid	conid;
};

}//namespace alpha
}//namespace concept

#endif
