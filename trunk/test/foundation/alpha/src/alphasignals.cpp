#include "utility/istream.hpp"
#include "utility/ostream.hpp"
#include "utility/iostream.hpp"

#include "foundation/signalpointer.hpp"
#include "foundation/ipc/ipcservice.hpp"

#include "system/timespec.hpp"
#include "system/filedevice.hpp"

#include "foundation/signalexecuter.hpp"
#include "foundation/filemanager.hpp"
#include "foundation/requestuid.hpp"

#include "core/common.hpp"
#include "core/manager.hpp"
#include "core/signals.hpp"


#include "alphasignals.hpp"
#include "system/debug.hpp"

namespace fdt=foundation;

namespace test{

namespace alpha{

//-----------------------------------------------------------------------------------
// RemoteListSignal
//-----------------------------------------------------------------------------------

int RemoteListSignal::ipcReceived(fdt::ipc::SignalUid &_rsiguid, const fdt::ipc::ConnectorUid &_rconid){
	fdt::SignalPointer<fdt::Signal> psig(this);
	conid = _rconid;
	if(!ppthlst){
		idbg("Received RemoteListSignal on peer");
		//print();
		ObjectUidTp	ttov;
		Manager::the().readSignalExecuterUid(ttov);
		Manager::the().signalObject(ttov.first, ttov.second, psig);
	}else{
		idbg("Received RemoteListSignal back on sender");
		_rsiguid = siguid;
		Manager::the().signalObject(fromv.first, fromv.second, psig);
	}
	return false;
}
void RemoteListSignal::ipcFail(int _err){
	if(!ppthlst){
		idbg("failed receiving response");
		Manager::the().signalObject(fromv.first, fromv.second, fdt::S_KILL | fdt::S_RAISE);
	}else{
		idbg("failed on peer");
	}
}
int RemoteListSignal::execute(uint32 _evs, fdt::SignalExecuter&, const SignalUidTp &, TimeSpec &_rts){
	if(tout){
		idbg("sleep for "<<tout<<" mseconds");
		_rts += tout;
		tout = 0;
		return NOK;
	}
	idbg("done sleeping");
	fs::directory_iterator	it,end;
	fs::path				pth(strpth.c_str(), fs::native);
	fdt::SignalPointer<fdt::Signal> psig(this);
	ppthlst = new RemoteList::PathListTp;
	strpth.clear();
	if(!exists( pth ) || !is_directory(pth)){
		err = -1;
		if(Manager::the().ipc().sendSignal(conid, psig)){
			idbg("connector was destroyed");
			return BAD;
		}
		return fdt::LEAVE;
	}
	try{
	it = fs::directory_iterator(pth);
	}catch ( const std::exception & ex ){
		idbg("dir_iterator exception :"<<ex.what());
		err = -1;
		strpth = ex.what();
		if(Manager::the().ipc().sendSignal(conid, psig)){
			idbg("connector was destroyed");
			return BAD;
		}
		return fdt::LEAVE;
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
	if(Manager::the().ipc().sendSignal(conid, psig)){
		idbg("connector was destroyed "<<conid.tkrid<<' '<<conid.procid<<' '<<conid.procuid);
		return BAD;
	}else{
		idbg("signal sent "<<conid.tkrid<<' '<<conid.procid<<' '<<conid.procuid);
	}
	return fdt::LEAVE;
}

//-----------------------------------------------------------------------------------
// FetchMasterSignal
//-----------------------------------------------------------------------------------

FetchMasterSignal::~FetchMasterSignal(){
	delete psig;
	idbg("");
}

void FetchMasterSignal::ipcFail(int _err){
	idbg("");
	Manager::the().signalObject(fromv.first, fromv.second, fdt::S_RAISE | fdt::S_KILL);
}
void FetchMasterSignal::print()const{
	idbg("FetchMasterSignal:");
	idbg("state = "<<state<<" insz = "<<insz<<" requid = "<<requid<<" fname = "<<fname);
	idbg("fromv.first = "<<fromv.first<<" fromv.second = "<<fromv.second);
	idbg("fuid.first = "<<fuid.first<<" fuid.second = "<<fuid.second);
	idbg("tmpfuid.first = "<<tmpfuid.first<<" tmpfuid.second = "<<tmpfuid.second);
}

int FetchMasterSignal::ipcReceived(fdt::ipc::SignalUid &_rsiguid, const fdt::ipc::ConnectorUid &_rconid){
	fdt::SignalPointer<fdt::Signal> sig(this);
	conid = _rconid;
	state = Received;
	ObjectUidTp	tov;
	idbg("received master signal");
	print();
	Manager::the().readSignalExecuterUid(tov);
	Manager::the().signalObject(tov.first, tov.second, sig);
	return OK;//release the ptr not clear
}
/*
	The state machine running on peer
*/
int FetchMasterSignal::execute(uint32 _evs, fdt::SignalExecuter& _rce, const SignalUidTp &_siguid, TimeSpec &_rts){
	Manager &rm(Manager::the());
	cassert(!(_evs & fdt::TIMEOUT));
	switch(state){
		case Received:{
			idbg("try to open file "<<fname<<" _siguid = "<<_siguid.first<<","<<_siguid.second);
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
			idbg("send first stream");
			FetchSlaveSignal	*psig = new FetchSlaveSignal;
			fdt::SignalPointer<fdt::Signal> sigptr(psig);
			insz = ins->size();
			psig->tov = fromv;
			psig->insz = insz;
			psig->sz = FetchChunkSize;
			if(psig->sz > psig->insz){
				psig->sz = psig->insz;
			}
			psig->siguid = _siguid;
			psig->requid = requid;
			psig->fuid = tmpfuid;
			idbg("insz = "<<insz<<" inpos = "<<inpos);
			insz -= psig->sz;
			inpos += psig->sz;
			fdt::RequestUid reqid(_rce.id(), rm.uid(_rce), _siguid.first, _siguid.second);
			rm.fileManager().stream(psig->ins, fuid, requid, fdt::FileManager::NoWait);
			psig = NULL;
			if(rm.ipc().sendSignal(conid, sigptr) || !insz){
				idbg("connector was destroyed or insz "<<insz);
				return BAD;
			}
			idbg("wait for streams");
			//TODO: put here timeout! - wait for signals
			//_rts.add(30);
			return NOK;
		}
		case SendNextStream:{
			idbg("send next stream");
			fdt::SignalPointer<fdt::Signal> sigptr(psig);
			psig->tov = fromv;
			psig->sz = FetchChunkSize;
			if(psig->sz > insz){
				psig->sz = insz;
			}
			psig->siguid = _siguid;
			psig->fuid = tmpfuid;
			idbg("insz = "<<insz<<" inpos = "<<inpos);
			insz -= psig->sz;
			fdt::RequestUid reqid(_rce.id(), rm.uid(_rce), _siguid.first, _siguid.second);
			rm.fileManager().stream(psig->ins, fuid, requid, fdt::FileManager::NoWait);
			psig->ins->seek(inpos);
			inpos += psig->sz;
			cassert(psig->ins);
			psig = NULL;
			if(rm.ipc().sendSignal(conid, sigptr) || !insz){
				idbg("connector was destroyed or insz "<<insz);
				return BAD;
			}
			idbg("wait for streams");
			//TODO: put here timeout! - wait for signals
			//_rts.add(30);
			return NOK;
		}
		case SendError:{
			idbg("sending error");
			FetchSlaveSignal *psig = new FetchSlaveSignal;
			fdt::SignalPointer<fdt::Signal> sigptr(psig);
			psig->tov = fromv;
			psig->sz = -2;
			rm.ipc().sendSignal(conid, sigptr);
			return BAD;
		}
	}
	return BAD;
}

int FetchMasterSignal::receiveSignal(
	fdt::SignalPointer<Signal> &_rsig,
	const ObjectUidTp& _from,
	const fdt::ipc::ConnectorUid *_conid
){
	if(_rsig->dynamicTypeId() == IStreamSignal::staticTypeId()){
		IStreamSignal &rsig(*static_cast<IStreamSignal*>(_rsig.ptr()));
		ins = rsig.sptr;
		fuid = rsig.fileuid;
		state = SendFirstStream;
	}else /*if(_rsig->dynamicTypeId() == OStreamSignal::staticTypeId()){
		OStreamSignal &rsig(*static_cast<OStreamSignal*>(_rsig.ptr()));
	}else if(_rsig->dynamicTypeId() == IOStreamSignal::staticTypeId()){
		IOStreamSignal &rsig(*static_cast<IOStreamSignal*>(_rsig.ptr()));
	}else */if(_rsig->dynamicTypeId() == StreamErrorSignal::staticTypeId()){
		StreamErrorSignal &rsig(*static_cast<StreamErrorSignal*>(_rsig.ptr()));
		state = SendError;
	}else if(_rsig->dynamicTypeId() == FetchSlaveSignal::staticTypeId()){
		//FetchSlaveSignal &rsig(*static_cast<FetchSlaveSignal*>(_rsig.ptr()));
		psig = static_cast<FetchSlaveSignal*>(_rsig.release());
		state = SendNextStream;
	}else return NOK;
	return OK;//success reschedule command for execution
}

// int FetchMasterSignal::receiveSignal(
// 	fdt::SignalPointer<fdt::Signal> &_rsig,
// 	int			_which,
// 	const ObjectUidTp&_from,
// 	const fdt::ipc::ConnectorUid *
// ){
// 	psig = static_cast<FetchSlaveSignal*>(_rsig.release());
// 	idbg("");
// 	state = SendNextStream;
// 	return OK;
// }
// int FetchMasterSignal::receiveIStream(
// 	StreamPtr<IStream> &_rins,
// 	const FileUidTp	& _fuid,
// 	int			_which,
// 	const ObjectUidTp&,
// 	const fdt::ipc::ConnectorUid *
// ){
// 	idbg("fuid = "<<_fuid.first<<","<<_fuid.second);
// 	ins = _rins;
// 	fuid = _fuid;
// 	state = SendFirstStream;
// 	return OK;
// }
// int FetchMasterSignal::receiveError(
// 	int _errid, 
// 	const ObjectUidTp&_from,
// 	const fdt::ipc::ConnectorUid *_conid
// ){
// 	idbg("");
// 	state = SendError;
// 	return OK;
// }

//-----------------------------------------------------------------------------------
// FetchSlaveSignal
//-----------------------------------------------------------------------------------

FetchSlaveSignal::~FetchSlaveSignal(){
	idbg("");
// 	if(fromv.first != 0xffffffff){
// 		idbg("unsuccessfull sent");
// 		//signal fromv object to die
// 		Manager::the().signalObject(fromv.first, fromv.second, fdt::S_RAISE | fdt::S_KILL);
// 	}
}
void FetchSlaveSignal::print()const{
	idbg("FetchSlaveSignal:");
	idbg("insz = "<<insz<<" sz = "<<sz<<" requid = "<<requid);
	idbg("fuid.first = "<<fuid.first<<" fuid.second = "<<fuid.second);
	idbg("siguid.first = "<<siguid.first<<" siguid.second = "<<siguid.second);
}
int FetchSlaveSignal::sent(const fdt::ipc::ConnectorUid &_rconid){
	idbg("");
	fromv.first = 0xffffffff;
	return BAD;
}
int FetchSlaveSignal::ipcReceived(fdt::ipc::SignalUid &_rsiguid, const fdt::ipc::ConnectorUid &_rconid){
	fdt::SignalPointer<fdt::Signal> psig(this);
	conid = _rconid;
	if(sz == -10){
		idbg("Received FetchSlaveSignal on peer");
		print();
		ObjectUidTp	ttov;
		Manager::the().readSignalExecuterUid(ttov);
		Manager::the().signalObject(ttov.first, ttov.second, psig);
	}else{
		idbg("Received FetchSlaveSignal on sender");
		print();
		Manager::the().signalObject(tov.first, tov.second, psig);
	}
	return OK;
}
// Executed when received back on the requesting alpha connection
// int FetchSlaveSignal::execute(test::Connection &_rcon){
// 	if(sz >= 0){
// 		idbg("");
// 		_rcon.receiveNumber(insz, RequestUidTp(requid, 0), 0, siguid, &conid);
// 	}else{
// 		idbg("");
// 		_rcon.receiveError(-1, RequestUidTp(requid, 0), siguid, &conid);
// 	}
// 	return NOK;
// }
// Executed on peer within the signal executer
int FetchSlaveSignal::execute(uint32 _evs, fdt::SignalExecuter& _rce, const SignalUidTp &, TimeSpec &){
	idbg("");
	fdt::SignalPointer<fdt::Signal>	cp(this);
	_rce.sendSignal(cp, siguid);
	return fdt::LEAVE;
}

void FetchSlaveSignal::destroyDeserializationStream(
	const std::pair<OStream *, int64> &_rps, int _id
){
	idbg("Destroy deserialization <"<<_id<<"> sz "<<_rps.second<<" streamptr "<<(void*)_rps.first);
	if(_rps.second < 0){
		//there was an error
		sz = -1;
	}
	delete _rps.first;
}

int FetchSlaveSignal::createDeserializationStream(
	std::pair<OStream *, int64> &_rps, int _id
){
	if(_id) return NOK;
	if(sz <= 0) return NOK;
	StreamPtr<OStream>			sp;
	fdt::RequestUid	requid;
	Manager::the().fileManager().stream(sp, fuid, requid, fdt::FileManager::Forced);
	if(!sp) return BAD;
	idbg("Create deserialization <"<<_id<<"> sz "<<_rps.second<<" streamptr "<<(void*)sp.ptr());
	if(insz < 0){//back 1M
		sp->seek(FetchChunkSize);
	}
	cassert(sp);
	_rps.first = sp.release();
	_rps.second = sz;
	return OK;
}

void FetchSlaveSignal::destroySerializationStream(
	const std::pair<IStream *, int64> &_rps, int _id
){
	idbg("doing nothing as the stream will be destroied when the signal will be destroyed");
}
int FetchSlaveSignal::createSerializationStream(
	std::pair<IStream *, int64> &_rps, int _id
){
	if(_id || !ins.ptr()) return NOK;
	idbg("Create serialization <"<<_id<<"> sz "<<_rps.second);
	_rps.first = ins.ptr();
	_rps.second = sz;
	return OK;
}

//-----------------------------------------------------------------------------------
// SendStringSignal
//-----------------------------------------------------------------------------------

int SendStringSignal::ipcReceived(fdt::ipc::SignalUid &_rsiguid, const fdt::ipc::ConnectorUid &_rconid){
	fdt::SignalPointer<fdt::Signal> psig(this);
	conid = _rconid;
	Manager::the().signalObject(tov.first, tov.second, psig);
	return false;
}

// int SendStringSignal::execute(test::Connection &_rcon){
// 	return _rcon.receiveString(str, test::Connection::RequestUidTp(0, 0), 0, fromv, &conid);
// }

//-----------------------------------------------------------------------------------
// SendStreamSignal
//-----------------------------------------------------------------------------------

int SendStreamSignal::ipcReceived(fdt::ipc::SignalUid &_rsiguid, const fdt::ipc::ConnectorUid &_rconid){
	fdt::SignalPointer<fdt::Signal> psig(this);
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
	int rv = Manager::the().fileManager().stream(this->iosp, this->dststr.c_str(), fdt::FileManager::NoWait);
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

// int SendStreamSignal::execute(test::Connection &_rcon){
// 	{
// 	StreamPtr<IStream>	isp(static_cast<IStream*>(iosp.release()));
// 	idbg("");
// 	_rcon.receiveIStream(isp, test::Connection::FileUidTp(0,0), test::Connection::RequestUidTp(0, 0), 0, fromv, &conid);
// 	idbg("");
// 	}
// 	return _rcon.receiveString(dststr, test::Connection::RequestUidTp(0, 0), 0, fromv, &conid);
// }

//-----------------------------------------------------------------------------------

}//namespace alpha

}//namespace test
