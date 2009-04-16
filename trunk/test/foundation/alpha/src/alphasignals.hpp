/* Declarations file alphasignalss.hpp
	
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

#ifndef ALPHA_SIGNALS_HPP
#define ALPHA_SIGNALS_HPP

#include "foundation/signal.hpp"
#include "foundation/signalpointer.hpp"

#include "alphacommands.hpp"

namespace foundation{
class SignalExecuter;
namespace ipc{
struct ConnectorUid;
}
}


///\cond 0
namespace std{

template <class S>
S& operator&(pair<String, int64> &_v, S &_s){
	return _s.push(_v.first, "first").push(_v.second, "second");
}

template <class S>
S& operator&(ObjectUidTp &_v, S &_s){
	return _s.push(_v.first, "first").push(_v.second, "second");
}
}
///\endcond

namespace test{
namespace alpha{

//---------------------------------------------------------------
// RemoteListSignal
//---------------------------------------------------------------
struct RemoteListSignal: Dynamic<RemoteListSignal, foundation::Signal>{
	RemoteListSignal(uint32 _tout = 0): ppthlst(NULL),err(-1),tout(_tout){
		//idbg(""<<(void*)this);
	}
	~RemoteListSignal(){
		//idbg(""<<(void*)this);
		delete ppthlst;
	}
	int execute(uint32 _evs, foundation::SignalExecuter&, const SignalUidTp &, TimeSpec &_rts);
	
	int ipcReceived(foundation::ipc::SignalUid &_rsiguid, const foundation::ipc::ConnectorUid &_rconid);
	int ipcPrepare(const foundation::ipc::SignalUid &_rsiguid){
		siguid = _rsiguid;
		if(!ppthlst){//on sender
			return NOK;
		}else return OK;// on peer
	}
	void ipcFail(int _err);

	template <class S>
	S& operator&(S &_s){
		_s.pushContainer(ppthlst, "strlst").push(err, "error").push(tout,"timeout");
		_s.push(requid, "requid").push(strpth, "strpth").push(fromv, "from");
		_s.push(siguid.idx, "siguid.idx").push(siguid.uid,"siguid.uid");
		return _s;
	}
//data:
	RemoteList::PathListTp			*ppthlst;
	String							strpth;
	int32							err;
	uint32							tout;
	foundation::ipc::ConnectorUid	conid;
	foundation::ipc::SignalUid		siguid;
	uint32							requid;
	ObjectUidTp						fromv;
};

struct FetchSlaveSignal;
enum{
	FetchChunkSize = 1024*1024
};

//---------------------------------------------------------------
// FetchMasterSignal
//---------------------------------------------------------------
/*
	This request is first sent to a peer's signal executer - where
	# it tries to get a stream,
	# sends a respose (FetchSlaveSignal) with the stream size and at most 1MB from the stream
	# waits for FetchSlaveSignal(s) to give em the rest of stream chunks
	# when the last stream chunk was sent it dies.
*/

struct FetchMasterSignal: Dynamic<FetchMasterSignal, foundation::Signal>{
	enum{
		NotReceived,
		Received,
		SendFirstStream,
		SendNextStream,
		SendError,
	};
	FetchMasterSignal():psig(NULL), fromv(0xffffffff, 0xffffffff), state(NotReceived), insz(-1), inpos(0), requid(0){
	}
	~FetchMasterSignal();
	
	int ipcReceived(foundation::ipc::SignalUid &_rsiguid, const foundation::ipc::ConnectorUid &_rconid);
	void ipcFail(int _err);
	
	int execute(uint32 _evs, foundation::SignalExecuter&, const SignalUidTp &, TimeSpec &_rts);

	int receiveSignal(
		foundation::SignalPointer<Signal> &_rsig,
		const ObjectUidTp& _from = ObjectUidTp(),
		const foundation::ipc::ConnectorUid *_conid = NULL
	);
	
	template <class S>
	S& operator&(S &_s){
		_s.push(fname, "filename");
		_s.push(tmpfuid.first, "tmpfileuid_first").push(tmpfuid.second, "tmpfileuid_second");
		return _s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid").push(requid, "requestuid");
	}
	void print()const;
//data:
	String					fname;
	FetchSlaveSignal		*psig;
	ObjectUidTp				fromv;
	FileUidTp				fuid;
	FileUidTp				tmpfuid;
	foundation::ipc::ConnectorUid	conid;
	StreamPtr<IStream>		ins;
	int32					state;
	int64					insz;
	int64					inpos;
	uint32					requid;
};

//---------------------------------------------------------------
// FetchSlaveSignal
//---------------------------------------------------------------
/*
	The signals sent from the alpha connection to the remote FetchMasterSignal
	to request new file chunks, and from FetchMasterSignal to the alpha connection
	as reponse containing the requested file chunk.
*/
struct FetchSlaveSignal: Dynamic<FetchSlaveSignal, foundation::Signal>{
	FetchSlaveSignal(): fromv(0xffffffff, 0xffffffff), insz(-1), sz(-10), requid(0){}
	~FetchSlaveSignal();
	int ipcReceived(foundation::ipc::SignalUid &_rsiguid, const foundation::ipc::ConnectorUid &_rconid);
	int sent(const foundation::ipc::ConnectorUid &);
	//int execute(test::Connection &);
	int execute(uint32 _evs, foundation::SignalExecuter&, const SignalUidTp &, TimeSpec &_rts);
	int createDeserializationStream(std::pair<OStream *, int64> &_rps, int _id);
	void destroyDeserializationStream(const std::pair<OStream *, int64> &_rps, int _id);
	int createSerializationStream(std::pair<IStream *, int64> &_rps, int _id);
	void destroySerializationStream(const std::pair<IStream *, int64> &_rps, int _id);
	
	template <class S>
	S& operator&(S &_s){
		_s.template pushStreammer<FetchSlaveSignal>(this, "FetchStreamResponse::isp");
		_s.push(tov.first, "toobjectid").push(tov.second, "toobjectuid");
		//_s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid");
		_s.push(insz, "inputstreamsize").push(requid, "requestuid");
		_s.push(sz, "inputsize").push(siguid.first, "signaluid_first").push(siguid.second, "signaluid_second");
		_s.push(fuid.first,"fileuid_first").push(fuid.second, "fileuid_second");
		return _s;
	}
	void print()const;
//data:	
	ObjectUidTp					fromv;
	ObjectUidTp					tov;
	FileUidTp					fuid;
	foundation::ipc::ConnectorUid		conid;
	SignalUidTp					siguid;
	StreamPtr<IStream>			ins;
	//if insz >= 0 -> [0->1M) else -> [1M->2M)
	int64						insz;
	int32						sz;
	uint32						requid;
};

//---------------------------------------------------------------
// SendStringSignal
//---------------------------------------------------------------
/*
	The signal sent to peer with the text
*/
struct SendStringSignal: Dynamic<SendStringSignal, foundation::Signal>{
	SendStringSignal(){}
	SendStringSignal(
		const String &_str,
		ulong _toobjid,
		uint32 _toobjuid,
		ulong _fromobjid,
		uint32 _fromobjuid
	):str(_str), tov(_toobjid, _toobjuid), fromv(_fromobjid, _fromobjuid){}
	int ipcReceived(foundation::ipc::SignalUid &_rsiguid, const foundation::ipc::ConnectorUid &_rconid);
	template <class S>
	S& operator&(S &_s){
		_s.push(str, "string").push(tov.first, "toobjectid").push(tov.second, "toobjectuid");
		return _s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid");
	}
private:
	typedef std::pair<uint32, uint32> ObjPairTp;
	String						str;
	ObjPairTp					tov;
	ObjPairTp					fromv;
	foundation::ipc::ConnectorUid		conid;
};

//---------------------------------------------------------------
// SendStreamSignal
//---------------------------------------------------------------
/*
	The signal sent to peer with the stream.
*/
struct SendStreamSignal: Dynamic<SendStreamSignal, foundation::Signal>{
	SendStreamSignal(){}
	SendStreamSignal(
		StreamPtr<IOStream> &_iosp,
		const String &_str,
		uint32 _myprocid,
		ulong _toobjid,
		uint32 _toobjuid,
		ulong _fromobjid,
		uint32 _fromobjuid
	):iosp(_iosp), dststr(_str), tov(_toobjid, _toobjuid), fromv(_fromobjid, _fromobjuid){}
	~SendStreamSignal(){
	}
	std::pair<uint32, uint32> to()const{return tov;}
	std::pair<uint32, uint32> from()const{return fromv;}
	int ipcReceived(foundation::ipc::SignalUid &_rsiguid, const foundation::ipc::ConnectorUid &_rconid);

	int createDeserializationStream(std::pair<OStream *, int64> &_rps, int _id);
	void destroyDeserializationStream(const std::pair<OStream *, int64> &_rps, int _id);
	int createSerializationStream(std::pair<IStream *, int64> &_rps, int _id);
	void destroySerializationStream(const std::pair<IStream *, int64> &_rps, int _id);
	
	template <class S>
	S& operator&(S &_s){
		_s.template pushStreammer<SendStreamSignal>(this, "SendStreamSignal::iosp").push(dststr, "dststr");
		_s.push(tov.first, "toobjectid").push(tov.second, "toobjectuid");
		return _s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid");
	}
private:
	typedef std::pair<uint32, uint32> 	ObjPairTp;
	typedef std::pair<uint32, uint32> 	FileUidTp;

	StreamPtr<IOStream>			iosp;
	String						dststr;
	ObjPairTp					tov;
	ObjPairTp					fromv;
	foundation::ipc::ConnectorUid		conid;
};

}//alpha
}//test

#endif
