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

//---------------------------------------------------------------
// RemoteListSignal
//---------------------------------------------------------------
struct RemoteListSignal: Dynamic<RemoteListSignal, DynamicShared<foundation::Signal> >{
	RemoteListSignal(uint32 _tout = 0, uint16 _sentcnt = 1);
	~RemoteListSignal();
	int execute(
		DynamicPointer<Signal> &_rthis_ptr,
		uint32 _evs,
		foundation::SignalExecuter&,
		const SignalUidT &, TimeSpec &_rts
	);
	
	int ipcReceived(foundation::ipc::SignalUid &_rsiguid, const foundation::ipc::ConnectionUid &_rconid);
	int ipcPrepare(const foundation::ipc::SignalUid &_rsiguid);
	void ipcFail(int _err);
	
	void use();
	int release();

	template <class S>
	S& operator&(S &_s){
		_s.pushContainer(ppthlst, "strlst").push(err, "error").push(tout,"timeout");
		_s.push(requid, "requid").push(strpth, "strpth").push(fromv, "from");
		_s.push(siguid.idx, "siguid.idx").push(siguid.uid,"siguid.uid");
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
	int16							sentcnt;
	bool							success_response;
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
	
	int ipcReceived(foundation::ipc::SignalUid &_rsiguid, const foundation::ipc::ConnectionUid &_rconid);
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
	StreamPointer<IStream>			ins;
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
	int ipcReceived(foundation::ipc::SignalUid &_rsiguid, const foundation::ipc::ConnectionUid &_rconid);
	int sent(const foundation::ipc::ConnectionUid &);
	//int execute(concept::Connection &);
	int execute(
		DynamicPointer<Signal> &_rthis_ptr,
		uint32 _evs,
		foundation::SignalExecuter&,
		const SignalUidT &, TimeSpec &_rts
	);
	int createDeserializationStream(std::pair<OStream *, int64> &_rps, int _id);
	void destroyDeserializationStream(const std::pair<OStream *, int64> &_rps, int _id);
	int createSerializationStream(std::pair<IStream *, int64> &_rps, int _id);
	void destroySerializationStream(const std::pair<IStream *, int64> &_rps, int _id);
	
	template <class S>
	S& operator&(S &_s){
		_s.template pushStreammer<FetchSlaveSignal>(this, "FetchStreamResponse::isp");
		_s.push(tov.first, "toobjectid").push(tov.second, "toobjectuid");
		//_s.push(fromv.first, "fromobjectid").push(fromv.second, "fromobjectuid");
		_s.push(streamsz, "streamsize").push(requid, "requestuid");
		_s.push(filesz, "filesize").push(siguid.first, "signaluid_first").push(siguid.second, "signaluid_second");
		_s.push(fuid.first,"fileuid_first").push(fuid.second, "fileuid_second");
		serialized = true;
		return _s;
	}
	void print()const;
//data:	
	ObjectUidT						fromv;
	ObjectUidT						tov;
	FileUidT						fuid;
	foundation::ipc::ConnectionUid	conid;
	SignalUidT						siguid;
	StreamPointer<IStream>			ins;
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
	int ipcReceived(foundation::ipc::SignalUid &_rsiguid, const foundation::ipc::ConnectionUid &_rconid);
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
		StreamPointer<IOStream> &_iosp,
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
	int ipcReceived(foundation::ipc::SignalUid &_rsiguid, const foundation::ipc::ConnectionUid &_rconid);

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
	typedef std::pair<uint32, uint32> 	ObjPairT;
	typedef std::pair<uint32, uint32> 	FileUidT;

	StreamPointer<IOStream>		iosp;
	String						dststr;
	ObjPairT					tov;
	ObjPairT					fromv;
	foundation::ipc::ConnectionUid		conid;
};

}//namespace alpha
}//namespace concept

#endif
