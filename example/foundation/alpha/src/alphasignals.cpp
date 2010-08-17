#include "utility/istream.hpp"
#include "utility/ostream.hpp"
#include "utility/iostream.hpp"
#include "utility/dynamicpointer.hpp"

#include "system/timespec.hpp"
#include "system/filedevice.hpp"
#include "system/mutex.hpp"

#include "foundation/signalexecuter.hpp"
#include "foundation/requestuid.hpp"
#include "foundation/file/filemanager.hpp"
#include "foundation/ipc/ipcservice.hpp"

#include "core/common.hpp"
#include "core/manager.hpp"
#include "core/signals.hpp"


#include "alphasignals.hpp"
#include "system/debug.hpp"

namespace fdt=foundation;

namespace concept{

namespace alpha{

//-----------------------------------------------------------------------------------
// RemoteListSignal
//-----------------------------------------------------------------------------------
RemoteListSignal::RemoteListSignal(uint32 _tout, uint16 _sentcnt): ppthlst(NULL),err(-1),tout(_tout), sentcnt(_sentcnt){
	idbg(""<<(void*)this);
}
RemoteListSignal::~RemoteListSignal(){
	idbg(""<<(void*)this);
	if(!ppthlst && !sentcnt){
		idbg("failed receiving response "<<sentcnt);
		Manager::the().signalObject(fromv.first, fromv.second, fdt::S_KILL | fdt::S_RAISE);
	}
	delete ppthlst;
}
void RemoteListSignal::use(){
	DynamicShared<fdt::Signal>::use();
	idbg(""<<(void*)this<<" usecount = "<<usecount);
}
int RemoteListSignal::release(){
	int rv = DynamicShared<fdt::Signal>::release();
	idbg(""<<(void*)this<<" usecount = "<<usecount);
	return rv;
}
int RemoteListSignal::ipcPrepare(const foundation::ipc::SignalUid &_rsiguid){
	idbg(""<<(void*)this<<" siguid = "<<_rsiguid.idx<<' '<<_rsiguid.uid);
	if(!ppthlst){//on sender
		//only on sender we hold the signal uid
		//to use it when we get back - see ipcReceived
		siguid = _rsiguid;
		sentcnt = -sentcnt;
		return NOK;
	}else return OK;// on peer
}
int RemoteListSignal::ipcReceived(fdt::ipc::SignalUid &_rsiguid, const fdt::ipc::ConnectionUid &_rconid){
	DynamicPointer<fdt::Signal> psig(this);
	conid = _rconid;
	if(!ppthlst){//on peer
		idbg("Received RemoteListSignal on peer");
		//print();
		ObjectUidT	ttov;
		Manager::the().readSignalExecuterUid(ttov);
		Manager::the().signalObject(ttov.first, ttov.second, psig);
	}else{//on sender
		idbg("Received RemoteListSignal back on sender");
		_rsiguid = siguid;
		Manager::the().signalObject(fromv.first, fromv.second, psig);
	}
	return false;
}
void RemoteListSignal::ipcFail(int _err){
	if(!ppthlst){
		idbg("failed on sender "<<sentcnt<<" "<<(void*)this);
		Mutex::Locker lock(mutex());
		++sentcnt;
	}else{
		idbg("failed on peer");
	}
}
int RemoteListSignal::execute(
	DynamicPointer<Signal> &_rthis_ptr,
	uint32 _evs,
	fdt::SignalExecuter&,
	const SignalUidT &,
	TimeSpec &_rts
){
	if(tout){
		idbg("sleep for "<<tout<<" mseconds");
		_rts += tout;
		tout = 0;
		return NOK;
	}
	idbg("done sleeping");
	
	fs::directory_iterator			it,end;
	fs::path						pth(strpth.c_str(), fs::native);
	
	ppthlst = new RemoteList::PathListT;
	strpth.clear();
	
	if(!exists( pth ) || !is_directory(pth)){
		err = -1;
		if(Manager::the().ipc().sendSignal(conid, _rthis_ptr)){
			idbg("connector was destroyed");
		}
		return BAD;
	}
	
	try{
		it = fs::directory_iterator(pth);
	}catch ( const std::exception & ex ){
		idbg("dir_iterator exception :"<<ex.what());
		err = -1;
		strpth = ex.what();
		if(Manager::the().ipc().sendSignal(conid, _rthis_ptr)){
			idbg("connector was destroyed");
		}
		return BAD;
	}
	
	while(it != end){
		ppthlst->push_back(std::pair<String, int64>(it->string(), -1));
		if(is_directory(*it)){
		}else{
			ppthlst->back().second = FileDevice::size(it->string().c_str());
		}
		++it;
	}
	err = 0;
	//Thread::sleep(1000 * 20);
	if(Manager::the().ipc().sendSignal(conid, _rthis_ptr)){
		idbg("connector was destroyed "<<conid.id<<' '<<conid.sessionidx<<' '<<conid.sessionuid);
	}else{
		idbg("signal sent "<<conid.id<<' '<<conid.sessionidx<<' '<<conid.sessionuid);
	}
	return BAD;
}

//-----------------------------------------------------------------------------------
// FetchMasterSignal
//-----------------------------------------------------------------------------------

FetchMasterSignal::~FetchMasterSignal(){
	delete psig;
	idbg((void*)this<<"");
}

void FetchMasterSignal::ipcFail(int _err){
	idbg((void*)this<<"");
	Manager::the().signalObject(fromv.first, fromv.second, fdt::S_RAISE | fdt::S_KILL);
}
void FetchMasterSignal::print()const{
	idbg((void*)this<<" FetchMasterSignal:");
	idbg("state = "<<state<<" streamsz = "<<streamsz<<" requid = "<<requid<<" fname = "<<fname);
	idbg("fromv.first = "<<fromv.first<<" fromv.second = "<<fromv.second);
	idbg("fuid.first = "<<fuid.first<<" fuid.second = "<<fuid.second);
	idbg("tmpfuid.first = "<<tmpfuid.first<<" tmpfuid.second = "<<tmpfuid.second);
}

int FetchMasterSignal::ipcReceived(fdt::ipc::SignalUid &_rsiguid, const fdt::ipc::ConnectionUid &_rconid){
	DynamicPointer<fdt::Signal> sig(this);
	conid = _rconid;
	state = Received;
	ObjectUidT	tov;
	idbg("received master signal");
	print();
	Manager::the().readSignalExecuterUid(tov);
	Manager::the().signalObject(tov.first, tov.second, sig);
	return OK;//release the ptr not clear
}
/*
	The state machine running on peer
*/
int FetchMasterSignal::execute(
	DynamicPointer<Signal> &_rthis_ptr,
	uint32 _evs,
	fdt::SignalExecuter& _rce,
	const SignalUidT &_siguid,
	TimeSpec &_rts
){
	Manager &rm(Manager::the());
	cassert(!(_evs & fdt::TIMEOUT));
	switch(state){
		case Received:{
			idbg((void*)this<<" try to open file "<<fname<<" _siguid = "<<_siguid.first<<","<<_siguid.second);
			//try to get a stream for the file:
			foundation::RequestUid reqid(_rce.id(), rm.uid(_rce), _siguid.first, _siguid.second);
			switch(rm.fileManager().stream(ins, fuid, reqid, fname.c_str())){
				case BAD://ouch
					state = SendError;
					idbg("open failed");
					return OK;
				case OK:
					idbg("open succeded");
					state = SendFirstStream;
					return OK;
				case NOK:
					idbg("open wait");
					return NOK;//wait the stream - no timeout
			}
		}break;
		case SendFirstStream:{
			idbg((void*)this<<" send first stream");
			FetchSlaveSignal			*psig(new FetchSlaveSignal);
			DynamicPointer<fdt::Signal>	sigptr(psig);
			
			this->filesz = ins->size();
			psig->tov = fromv;
			psig->filesz = this->filesz;
			psig->streamsz = this->streamsz;
			if(psig->streamsz > psig->filesz){
				psig->streamsz = psig->filesz;
			}
			psig->siguid = _siguid;
			psig->requid = requid;
			psig->fuid = tmpfuid;
			idbg("filesz = "<<this->filesz<<" inpos = "<<filepos);
			this->filesz -= psig->streamsz;
			this->filepos += psig->streamsz;
			fdt::RequestUid reqid(_rce.id(), rm.uid(_rce), _siguid.first, _siguid.second);
			rm.fileManager().stream(psig->ins, fuid, requid, fdt::file::Manager::NoWait);
			psig = NULL;
			if(rm.ipc().sendSignal(conid, sigptr) || !this->filesz){
				idbg("connector was destroyed or filesz = "<<this->filesz);
				return BAD;
			}
			idbg("wait for streams");
			//TODO: put here timeout! - wait for signals
			//_rts.add(30);
			return NOK;
		}
		case SendNextStream:{
			idbg((void*)this<<" send next stream");
			DynamicPointer<fdt::Signal> sigptr(psig);
			psig->tov = fromv;
			psig->filesz = this->filesz;
			//psig->sz = FetchChunkSize;
			if(psig->streamsz > this->filesz){
				psig->streamsz = this->filesz;
			}
			psig->siguid = _siguid;
			//psig->fuid = tmpfuid;
			idbg("filesz = "<<this->filesz<<" filepos = "<<this->filepos);
			this->filesz -= psig->streamsz;
			fdt::RequestUid reqid(_rce.id(), rm.uid(_rce), _siguid.first, _siguid.second);
			rm.fileManager().stream(psig->ins, fuid, requid, fdt::file::Manager::NoWait);
			psig->ins->seek(this->filepos);
			this->filepos += psig->streamsz;
			cassert(psig->ins);
			psig = NULL;
			if(rm.ipc().sendSignal(conid, sigptr) || !this->filesz){
				idbg("connector was destroyed or filesz = "<<this->filesz);
				return BAD;
			}
			idbg("wait for streams");
			//TODO: put here timeout! - wait for signals
			//_rts.add(30);
			return NOK;
		}
		case SendError:{
			idbg((void*)this<<" sending error");
			FetchSlaveSignal *psig = new FetchSlaveSignal;
			DynamicPointer<fdt::Signal> sigptr(psig);
			psig->tov = fromv;
			psig->filesz = -1;
			rm.ipc().sendSignal(conid, sigptr);
			return BAD;
		}
	}
	return BAD;
}

int FetchMasterSignal::receiveSignal(
	DynamicPointer<Signal> &_rsig,
	const ObjectUidT& _from,
	const fdt::ipc::ConnectionUid *_conid
){
	if(_rsig->dynamicTypeId() == IStreamSignal::staticTypeId()){
		idbg((void*)this<<" Received stream");
		IStreamSignal &rsig(*static_cast<IStreamSignal*>(_rsig.ptr()));
		ins = rsig.sptr;
		fuid = rsig.fileuid;
		state = SendFirstStream;
	}else /*if(_rsig->dynamicTypeId() == OStreamSignal::staticTypeId()){
		OStreamSignal &rsig(*static_cast<OStreamSignal*>(_rsig.ptr()));
	}else if(_rsig->dynamicTypeId() == IOStreamSignal::staticTypeId()){
		IOStreamSignal &rsig(*static_cast<IOStreamSignal*>(_rsig.ptr()));
	}else */if(_rsig->dynamicTypeId() == StreamErrorSignal::staticTypeId()){
		//StreamErrorSignal &rsig(*static_cast<StreamErrorSignal*>(_rsig.ptr()));
		state = SendError;
	}else if(_rsig->dynamicTypeId() == FetchSlaveSignal::staticTypeId()){
		//FetchSlaveSignal &rsig(*static_cast<FetchSlaveSignal*>(_rsig.ptr()));
		psig = static_cast<FetchSlaveSignal*>(_rsig.release());
		idbg((void*)this<<" Received slavesignal");
		state = SendNextStream;
	}else return NOK;
	idbg("success");
	return OK;//success reschedule command for execution
}

// int FetchMasterSignal::receiveSignal(
// 	DynamicPointer<fdt::Signal> &_rsig,
// 	int			_which,
// 	const ObjectUidT&_from,
// 	const fdt::ipc::ConnectionUid *
// ){
// 	psig = static_cast<FetchSlaveSignal*>(_rsig.release());
// 	idbg("");
// 	state = SendNextStream;
// 	return OK;
// }
// int FetchMasterSignal::receiveIStream(
// 	StreamPointer<IStream> &_rins,
// 	const FileUidT	& _fuid,
// 	int			_which,
// 	const ObjectUidT&,
// 	const fdt::ipc::ConnectionUid *
// ){
// 	idbg("fuid = "<<_fuid.first<<","<<_fuid.second);
// 	ins = _rins;
// 	fuid = _fuid;
// 	state = SendFirstStream;
// 	return OK;
// }
// int FetchMasterSignal::receiveError(
// 	int _errid, 
// 	const ObjectUidT&_from,
// 	const fdt::ipc::ConnectionUid *_conid
// ){
// 	idbg("");
// 	state = SendError;
// 	return OK;
// }

//-----------------------------------------------------------------------------------
// FetchSlaveSignal
//-----------------------------------------------------------------------------------

FetchSlaveSignal::FetchSlaveSignal(): fromv(0xffffffff, 0xffffffff), filesz(-10), streamsz(-1), requid(0){
	idbg(""<<(void*)this);
	serialized = false;
}

FetchSlaveSignal::~FetchSlaveSignal(){
	idbg(""<<(void*)this);
	cassert(serialized);
	print();
// 	if(fromv.first != 0xffffffff){
// 		idbg("unsuccessfull sent");
// 		//signal fromv object to die
// 		Manager::the().signalObject(fromv.first, fromv.second, fdt::S_RAISE | fdt::S_KILL);
// 	}
}
void FetchSlaveSignal::print()const{
	idbg((void*)this<<" FetchSlaveSignal:");
	idbg("filesz = "<<this->filesz<<" streamsz = "<<this->streamsz<<" requid = "<<requid);
	idbg("fuid.first = "<<fuid.first<<" fuid.second = "<<fuid.second);
	idbg("siguid.first = "<<siguid.first<<" siguid.second = "<<siguid.second);
}
int FetchSlaveSignal::sent(const fdt::ipc::ConnectionUid &_rconid){
	idbg((void*)this<<"");
	fromv.first = 0xffffffff;
	return BAD;
}
int FetchSlaveSignal::ipcReceived(fdt::ipc::SignalUid &_rsiguid, const fdt::ipc::ConnectionUid &_rconid){
	DynamicPointer<fdt::Signal> psig(this);
	conid = _rconid;
	if(filesz == -10){
		idbg((void*)this<<" Received FetchSlaveSignal on peer");
		print();
		ObjectUidT	ttov;
		Manager::the().readSignalExecuterUid(ttov);
		Manager::the().signalObject(ttov.first, ttov.second, psig);
	}else{
		idbg((void*)this<<" Received FetchSlaveSignal on sender");
		print();
		Manager::the().signalObject(tov.first, tov.second, psig);
	}
	return OK;
}
// Executed on peer within the signal executer
int FetchSlaveSignal::execute(
	DynamicPointer<Signal> &_rthis_ptr,
	uint32 _evs,
	fdt::SignalExecuter& _rce,
	const SignalUidT &,
	TimeSpec &
){
	idbg((void*)this<<"");
	_rce.sendSignal(_rthis_ptr, siguid);
	return BAD;
}

void FetchSlaveSignal::destroyDeserializationStream(
	const std::pair<OStream *, int64> &_rps, int _id
){
	idbg((void*)this<<" Destroy deserialization <"<<_id<<"> sz "<<_rps.second<<" streamptr "<<(void*)_rps.first);
	if(_rps.second < 0){
		//there was an error
		filesz = -1;
	}
	delete _rps.first;
}

int FetchSlaveSignal::createDeserializationStream(
	std::pair<OStream *, int64> &_rps, int _id
){
	idbg((void*)this<<" "<<_id<<" "<<filesz<<' '<<fuid.first<<' '<<fuid.second);
	if(_id) return NOK;
	if(filesz <= 0) return NOK;
	
	StreamPointer<OStream>		sp;
	fdt::RequestUid				requid;
	Manager::the().fileManager().stream(sp, fuid, requid, fdt::file::Manager::Forced);
	if(!sp){
		idbg("");
		return BAD;
	}
	
	idbg((void*)this<<" Create deserialization <"<<_id<<"> sz "<<_rps.second<<" streamptr "<<(void*)sp.ptr());
	
	cassert(sp);
	_rps.first = sp.release();
	_rps.second = streamsz;
	return OK;
}

void FetchSlaveSignal::destroySerializationStream(
	const std::pair<IStream *, int64> &_rps, int _id
){
	idbg((void*)this<<" doing nothing as the stream will be destroied when the signal will be destroyed");
}

int FetchSlaveSignal::createSerializationStream(
	std::pair<IStream *, int64> &_rps, int _id
){
	if(_id || !ins.ptr()) return NOK;
	idbg((void*)this<<" Create serialization <"<<_id<<"> sz "<<_rps.second);
	_rps.first = ins.ptr();
	_rps.second = streamsz;
	return OK;
}

//-----------------------------------------------------------------------------------
// SendStringSignal
//-----------------------------------------------------------------------------------

int SendStringSignal::ipcReceived(fdt::ipc::SignalUid &_rsiguid, const fdt::ipc::ConnectionUid &_rconid){
	DynamicPointer<fdt::Signal> psig(this);
	conid = _rconid;
	Manager::the().signalObject(tov.first, tov.second, psig);
	return false;
}

// int SendStringSignal::execute(concept::Connection &_rcon){
// 	return _rcon.receiveString(str, concept::Connection::RequestUidT(0, 0), 0, fromv, &conid);
// }

//-----------------------------------------------------------------------------------
// SendStreamSignal
//-----------------------------------------------------------------------------------

int SendStreamSignal::ipcReceived(fdt::ipc::SignalUid &_rsiguid, const fdt::ipc::ConnectionUid &_rconid){
	DynamicPointer<fdt::Signal> psig(this);
	conid = _rconid;
	Manager::the().signalObject(tov.first, tov.second, psig);
	return false;
}

void SendStreamSignal::destroyDeserializationStream(
	const std::pair<OStream *, int64> &_rps, int _id
){
	idbg("Destroy deserialization <"<<_id<<"> sz "<<_rps.second);
}
int SendStreamSignal::createDeserializationStream(
	std::pair<OStream *, int64> &_rps, int _id
){
	if(_id) return NOK;
	idbg("Create deserialization <"<<_id<<"> sz "<<_rps.second);
	if(dststr.empty()/* || _rps.second < 0*/) return NOK;
	idbg("File name: "<<this->dststr);
	//TODO:
	int rv = Manager::the().fileManager().stream(this->iosp, this->dststr.c_str(), fdt::file::Manager::NoWait);
	if(rv){
		idbg("Oops, could not open file");
		return BAD;
	}else{
		_rps.first = static_cast<OStream*>(this->iosp.ptr());
	}
	return OK;
}
void SendStreamSignal::destroySerializationStream(
	const std::pair<IStream *, int64> &_rps, int _id
){
	idbg("doing nothing as the stream will be destroied when the signal will be destroyed");
}
int SendStreamSignal::createSerializationStream(
	std::pair<IStream *, int64> &_rps, int _id
){
	if(_id) return NOK;
	idbg("Create serialization <"<<_id<<"> sz "<<_rps.second);
	//The stream is already opened
	_rps.first = static_cast<IStream*>(this->iosp.ptr());
	_rps.second = this->iosp->size();
	return OK;
}

// int SendStreamSignal::execute(concept::Connection &_rcon){
// 	{
// 	StreamPointer<IStream>	isp(static_cast<IStream*>(iosp.release()));
// 	idbg("");
// 	_rcon.receiveIStream(isp, concept::Connection::FileUidT(0,0), concept::Connection::RequestUidT(0, 0), 0, fromv, &conid);
// 	idbg("");
// 	}
// 	return _rcon.receiveString(dststr, concept::Connection::RequestUidT(0, 0), 0, fromv, &conid);
// }

//-----------------------------------------------------------------------------------

}//namespace alpha

}//namespace concept
