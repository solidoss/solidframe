/* Declarations file alphasignalss.hpp
	
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

#ifndef ALPHA_SIGNALS_HPP
#define ALPHA_SIGNALS_HPP

#include "foundation/signal.hpp"
#include "foundation/ipc/ipcconnectionuid.hpp"

#include "utility/dynamicpointer.hpp"

#include "alphacommands.hpp"

namespace foundation{
class SignalExecuter;
namespace ipc{
struct ConnectionUid;
}
}


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

struct SignalTypeIds{
	static const SignalTypeIds& the(const SignalTypeIds *_pids = NULL);
	uint32 fetchmastercommand;
	uint32 fetchslavecommand;
	uint32 fetchslaveresponse;
	uint32 remotelistcommand;
	uint32 remotelistresponse;
};

//---------------------------------------------------------------
// RemoteListSignal
//---------------------------------------------------------------
struct RemoteListSignal: Dynamic<RemoteListSignal, DynamicShared<foundation::Signal> >{
	RemoteListSignal(uint32 _tout = 0, uint16 _sentcnt = 1);
	RemoteListSignal(const NumberType<1>&);
	~RemoteListSignal();
	int execute(
		DynamicPointer<Signal> &_rthis_ptr,
		uint32 _evs,
		foundation::SignalExecuter&,
		const SignalUidT &, TimeSpec &_rts
	);
	
	void ipcReceived(
		foundation::ipc::SignalUid &_rsiguid
	);
	uint32 ipcPrepare();
	void ipcFail(int _err);
	void ipcSuccess();
	void use();
	int release();

	template <class S>
	S& operator&(S &_s){
		_s.pushContainer(ppthlst, "strlst").push(err, "error").push(tout,"timeout");
		_s.push(requid, "requid").push(strpth, "strpth").push(fromv, "from");
		_s.push(ipcstatus, "ipcstatus");
		if(ipcstatus != IpcOnSender || S::IsDeserializer){
			_s.push(siguid.idx, "siguid.idx").push(siguid.uid,"siguid.uid");
		}else{//on sender
			foundation::ipc::SignalUid &rsiguid(
				const_cast<foundation::ipc::SignalUid &>(foundation::ipc::ConnectionContext::the().signaluid)
			);
			_s.push(rsiguid.idx, "siguid.idx").push(rsiguid.uid,"siguid.uid");
		}
		return _s;
	}
//data:
	RemoteList::PathListT			*ppthlst;
	String							strpth;
	int32							err;
	uint32							tout;
	foundation::ipc::ConnectionUid	conid;
	foundation::ipc::SignalUid		siguid;
	uint32							requid;
	ObjectUidT						fromv;
	uint8							success;
	uint8							ipcstatus;
};

struct FetchSlaveSignal;


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
	FetchMasterSignal():psig(NULL), fromv(0xffffffff, 0xffffffff), state(NotReceived), streamsz(0), filesz(0), filepos(0), requid(0){
	}
	~FetchMasterSignal();
	uint32 ipcPrepare();
	void ipcReceived(
		foundation::ipc::SignalUid &_rsiguid
	);
	void ipcFail(int _err);
	
	int execute(
		DynamicPointer<Signal> &_rthis_ptr,
		uint32 _evs,
		foundation::SignalExecuter&,
		const SignalUidT &, TimeSpec &_rts
	);

	int receiveSignal(
		DynamicPointer<Signal> &_rsig,
		const ObjectUidT& _from = ObjectUidT(),
		const foundation::ipc::ConnectionUid *_conid = NULL
	);
	
	template <class S>
	S& operator&(S &_s){
		_s.push(fname, "filename");
		_s.push(tmpfuid.first, "tmpfileuid_first").push(tmpfuid.second, "tmpfileuid_second");
		return _s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid").push(requid, "requestuid").push(streamsz, "streamsize");
	}
	void print()const;
//data:
	String							fname;
	FetchSlaveSignal				*psig;
	ObjectUidT						fromv;
	FileUidT						fuid;
	FileUidT						tmpfuid;
	foundation::ipc::ConnectionUid	conid;
	StreamPointer<InputStream>			ins;
	int32							state;
	uint32							streamsz;
	int64							filesz;
	int64							filepos;
	uint32							requid;
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
	FetchSlaveSignal();
	~FetchSlaveSignal();
	void ipcReceived(
		foundation::ipc::SignalUid &_rsiguid
	);
	int sent(const foundation::ipc::ConnectionUid &);
	//int execute(concept::Connection &);
	int execute(
		DynamicPointer<Signal> &_rthis_ptr,
		uint32 _evs,
		foundation::SignalExecuter&,
		const SignalUidT &, TimeSpec &_rts
	);
	uint32 ipcPrepare();
	
	template <class S>
	S& operator&(S &_s){
		_s.template pushReinit<FetchSlaveSignal, 0>(this, 0, "reinit");
		_s.push(tov.first, "toobjectid").push(tov.second, "toobjectuid");
		//_s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid");
		_s.push(streamsz, "streamsize").push(requid, "requestuid");
		_s.push(filesz, "filesize").push(siguid.first, "signaluid_first").push(siguid.second, "signaluid_second");
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
			_rs.template pushReinit<FetchSlaveSignal, 0>(this, 1, "reinit");
			_rs.pushStream(ps, (uint64)0, (uint64)streamsz, "stream");
		}
		return CONTINUE;
	}
	void initOutputStream();
	void clearOutputStream();
	void print()const;
//data:	
	ObjectUidT						fromv;
	ObjectUidT						tov;
	FileUidT						fuid;
	foundation::ipc::ConnectionUid	conid;
	SignalUidT						siguid;
	StreamPointer<InputStream>		ins;
	StreamPointer<OutputStream>		outs;
	int64							filesz;
	int32							streamsz;
	uint32							requid;
	bool							serialized;
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
	
	void ipcReceived(
		foundation::ipc::SignalUid &_rsiguid
	);
	template <class S>
	S& operator&(S &_s){
		_s.push(str, "string").push(tov.first, "toobjectid").push(tov.second, "toobjectuid");
		return _s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid");
	}
private:
	typedef std::pair<uint32, uint32> ObjPairT;
	String						str;
	ObjPairT					tov;
	ObjPairT					fromv;
	foundation::ipc::ConnectionUid		conid;
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
		StreamPointer<InputOutputStream> &_iosp,
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
	void ipcReceived(
		foundation::ipc::SignalUid &_rsiguid
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

	StreamPointer<InputOutputStream>		iosp;
	String						dststr;
	ObjPairT					tov;
	ObjPairT					fromv;
	foundation::ipc::ConnectionUid		conid;
};

}//namespace alpha
}//namespace concept

#endif
