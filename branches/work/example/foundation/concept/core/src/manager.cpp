/* Implementation file manager.cpp
	
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

#include <map>
#include <vector>

#include "system/debug.hpp"

#include "utility/iostream.hpp"
#include "utility/dynamictype.hpp"

#include "core/manager.hpp"
#include "core/service.hpp"
#include "core/signals.hpp"
#include "quicklz.h"

#include "foundation/aio/aioselector.hpp"
#include "foundation/aio/aioobject.hpp"

#include "foundation/objectselector.hpp"
#include "foundation/signalexecuter.hpp"
#include "foundation/signal.hpp"
#include "foundation/requestuid.hpp"

#include "foundation/ipc/ipcservice.hpp"
#include "foundation/file/filemanager.hpp"
#include "foundation/file/filemappers.hpp"

#include <iostream>
using namespace std;

//some forward declarations
namespace foundation{

class ObjectSelector;

namespace aio{
class Selector;
}
}//namespace foundation

namespace fdt=foundation;


namespace concept{
//------------------------------------------------------
//		FileManagerController
//------------------------------------------------------

class FileManagerController: public fdt::file::Manager::Controller{
public:
	FileManagerController(){}
protected:
	/*virtual*/ void init(const fdt::file::Manager::InitStub &_ris);
	 
	/*virtual*/ bool release();
	
	/*virtual*/ void sendStream(
		StreamPointer<InputStream> &_sptr,
		const FileUidT &_rfuid,
		const fdt::RequestUid& _rrequid
	);
	/*virtual*/ void sendStream(
		StreamPointer<OutputStream> &_sptr,
		const FileUidT &_rfuid,
		const fdt::RequestUid& _rrequid
	);
	/*virtual*/ void sendStream(
		StreamPointer<InputOutputStream> &_sptr,
		const FileUidT &_rfuid,
		const fdt::RequestUid& _rrequid
	);
	/*virtual*/ void sendError(
		int _error,
		const fdt::RequestUid& _rrequid
	);
};


//------------------------------------------------------
//		IpcServiceController
//------------------------------------------------------
/*
In this example we do:
* accept only one connection from one other concept application
* authenticate in X steps:
	> concept1 -> request auth step 1 -> concept2
	> concept2 -> auth step 2 -> concept1
	> concept1 -> auth step 3 -> concept2
	> concept2 accept authentication -> auth step 4 -> concept1 auth done
*/


struct AuthSignal: Dynamic<AuthSignal, DynamicShared<foundation::Signal> >{
	AuthSignal():authidx(0), authcnt(0){}
	~AuthSignal(){}
	
	template <class S>
	S& operator&(S &_s){
		_s.push(authidx, "authidx").push(authcnt, "authcnt");
		if(S::IsDeserializer){
			_s.push(siguid.idx, "siguid.idx").push(siguid.uid,"siguid.uid");
		}else{//on sender
			foundation::ipc::SignalUid &rsiguid(
				const_cast<foundation::ipc::SignalUid &>(foundation::ipc::ConnectionContext::the().signaluid)
			);
			_s.push(rsiguid.idx, "siguid.idx").push(rsiguid.uid,"siguid.uid");
		}
		_s.push(siguidpeer.idx, "siguidpeer.idx").push(siguidpeer.uid,"siguidpeer.uid");
		return _s;
	}
//data:
	int								authidx;
	int								authcnt;
	foundation::ipc::SignalUid		siguid;
	foundation::ipc::SignalUid		siguidpeer;
};


struct IpcServiceController: foundation::ipc::Controller{
	IpcServiceController():foundation::ipc::Controller(400, AuthenticationFlag), authidx(0){
		
	}
	/*virtual*/ void scheduleTalker(foundation::aio::Object *_po);
	/*virtual*/ bool release();
	/*virtual*/ bool compressBuffer(
		foundation::ipc::BufferContext &_rbc,
		const uint32 _bufsz,
		char* &_rpb,
		uint32 &_bl
	);
	/*virtual*/ bool decompressBuffer(
		foundation::ipc::BufferContext &_rbc,
		char* &_rpb,
		uint32 &_bl
	);
	/*virtual*/ int authenticate(
		DynamicPointer<fdt::Signal> &_sigptr,//the received signal
		fdt::ipc::SignalUid &_rsiguid,
		uint32 &_rflags,
		fdt::ipc::SerializationTypeIdT &_rtid
	);
private:
	qlz_state_compress		qlz_comp_ctx;
	qlz_state_decompress	qlz_decomp_ctx;
	int						authidx;
};


/*virtual*/ bool IpcServiceController::compressBuffer(
	foundation::ipc::BufferContext &_rbc,
	const uint32 _bufsz,
	char* &_rpb,
	uint32 &_bl
){
// 	if(_bufsz < 1024){
// 		return false;
// 	}
	uint32	destcp(0);
	char 	*pdest =  allocateBuffer(_rbc, destcp);
	size_t	len = qlz_compress(_rpb, pdest, _bl, &qlz_comp_ctx);
	_rpb = pdest;
	_bl = len;
	return true;
}

/*virtual*/ bool IpcServiceController::decompressBuffer(
	foundation::ipc::BufferContext &_rbc,
	char* &_rpb,
	uint32 &_bl
){
	uint32	destcp(0);
	char 	*pdest =  allocateBuffer(_rbc, destcp);
	size_t	len = qlz_decompress(_rpb, pdest, &qlz_decomp_ctx);
	_rpb = pdest;
	_bl = len;
	return true;
}

/*virtual*/ int IpcServiceController::authenticate(
	DynamicPointer<fdt::Signal> &_sigptr,//the received signal
	fdt::ipc::SignalUid &_rsiguid,
	uint32 &_rflags,
	fdt::ipc::SerializationTypeIdT &_rtid
){
	if(!_sigptr.get()){
		if(authidx){
			idbg("");
			return BAD;
		}
		//initiate authentication
		_sigptr = new AuthSignal;
		++authidx;
		idbg("authidx = "<<authidx);
		return NOK;
	}
	if(_sigptr->dynamicTypeId() != AuthSignal::staticTypeId()){
		cassert(false);
		return BAD;
	}
	AuthSignal &rsig(static_cast<AuthSignal&>(*_sigptr));
	
	_rsiguid = rsig.siguidpeer;
	
	rsig.siguidpeer = rsig.siguid;
	
	idbg("sig = "<<(void*)_sigptr.get()<<" auth("<<rsig.authidx<<','<<rsig.authcnt<<") authidx = "<<this->authidx);
	
	if(rsig.authidx == 0){
		if(this->authidx == 2){
			idbg("");
			return BAD;
		}
		++this->authidx;
		rsig.authidx = this->authidx;
	}
	
	++rsig.authcnt;
	
	if(rsig.authidx == 2 && rsig.authcnt >= 3){
		idbg("");
		return BAD;
	}
	
	
	if(rsig.authcnt == 4){
		idbg("");
		return OK;
	}
	if(rsig.authcnt == 5){
		_sigptr.clear();
		idbg("");
		return OK;
	}
	idbg("");
	return NOK;
}


//------------------------------------------------------
//		Manager::Data
//------------------------------------------------------

namespace{
	FileManagerController	fmctrl;
	IpcServiceController	ipcctrl;
}

struct Manager::Data{
	Data():filemanageridx(1), readsigexeidx(2), writesigexeidx(3), ipcidx(4){}
	const IndexT			filemanageridx;
	const IndexT			readsigexeidx;
	const IndexT			writesigexeidx;
	const IndexT			ipcidx;
};

//--------------------------------------------------------------------------
Manager::Manager():foundation::Manager(16), d(*(new Data())){
	//NOTE: Use the following line instead of ThisGuard if you only have one Manager per process, else use the ThisGuard for any function
	// that may be called from a thread that has access to other managers.
	//this->prepareThread();
	ThisGuard	tg(this);
	
	registerScheduler(new SchedulerT(*this));
	registerScheduler(new AioSchedulerT(*this));
	registerScheduler(new AioSchedulerT(*this));
	
	registerObject<SchedulerT>(new fdt::file::Manager(&fmctrl), 0, d.filemanageridx);
	registerObject<SchedulerT>(new fdt::SignalExecuter, 0, d.readsigexeidx);
	registerObject<SchedulerT>(new fdt::SignalExecuter, 0, d.writesigexeidx);
	
	registerService<SchedulerT>(new foundation::ipc::Service(&ipcctrl), 0, d.ipcidx);
	
	fdt::ipc::Service::the().typeMapper().insert<AuthSignal>();
}

Manager::~Manager(){
	delete &d;
}

ObjectUidT Manager::readSignalExecuterUid()const{
	return object(d.readsigexeidx).uid();
}
	
ObjectUidT Manager::writeSignalExecuterUid()const{
	return object(d.writesigexeidx).uid();
}

foundation::ipc::Service&	Manager::ipc()const{
	return static_cast<foundation::ipc::Service&>(foundation::Manager::service(d.ipcidx));
}
foundation::file::Manager&	Manager::fileManager()const{
	return static_cast<foundation::file::Manager&>(foundation::Manager::object(d.filemanageridx));
}
void Manager::signalService(const IndexT &_ridx, DynamicPointer<foundation::Signal> &_rsig){
	signal(_rsig, serviceUid(_ridx));
}

//------------------------------------------------------
//		IpcServiceController
//------------------------------------------------------
void IpcServiceController::scheduleTalker(foundation::aio::Object *_po){
	idbg("");
	foundation::ObjectPointer<foundation::aio::Object> op(_po);
	AioSchedulerT::schedule(op, 1);
}
bool IpcServiceController::release(){
	return false;
}
//------------------------------------------------------
//		FileManagerController
//------------------------------------------------------
/*virtual*/ void FileManagerController::init(const fdt::file::Manager::InitStub &_ris){
	_ris.registerMapper(new fdt::file::NameMapper(10, 0));
	_ris.registerMapper(new fdt::file::TempMapper(1024ULL * 1024 * 1024, "/tmp"));
	_ris.registerMapper(new fdt::file::MemoryMapper(1024ULL * 1024 * 100));
}
/*virtual*/ bool FileManagerController::release(){
	return false;
}
void FileManagerController::sendStream(
	StreamPointer<InputStream> &_sptr,
	const FileUidT &_rfuid,
	const fdt::RequestUid& _rrequid
){
	RequestUidT	ru(_rrequid.reqidx, _rrequid.requid);
	DynamicPointer<fdt::Signal>	cp(new InputStreamSignal(_sptr, _rfuid, ru));
	Manager::the().signal(cp, _rrequid.objidx, _rrequid.objuid);
}

void FileManagerController::sendStream(
	StreamPointer<OutputStream> &_sptr,
	const FileUidT &_rfuid,
	const fdt::RequestUid& _rrequid
){
	RequestUidT	ru(_rrequid.reqidx, _rrequid.requid);
	DynamicPointer<fdt::Signal>	cp(new OutputStreamSignal(_sptr, _rfuid, ru));
	Manager::the().signal(cp, _rrequid.objidx, _rrequid.objuid);
}
void FileManagerController::sendStream(
	StreamPointer<InputOutputStream> &_sptr,
	const FileUidT &_rfuid,
	const fdt::RequestUid& _rrequid
){
	RequestUidT	ru(_rrequid.reqidx, _rrequid.requid);
	DynamicPointer<fdt::Signal>	cp(new InputOutputStreamSignal(_sptr, _rfuid, ru));
	Manager::the().signal(cp, _rrequid.objidx, _rrequid.objuid);
}
void FileManagerController::sendError(
	int _error,
	const fdt::RequestUid& _rrequid
){
	RequestUidT	ru(_rrequid.reqidx, _rrequid.requid);
	DynamicPointer<fdt::Signal>	cp(new StreamErrorSignal(_error, ru));
	Manager::the().signal(cp, _rrequid.objidx, _rrequid.objuid);
}



}//namespace concept

