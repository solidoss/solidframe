#include "example/distributed/consensus/server/serverobject.hpp"
#include "example/distributed/consensus/core/manager.hpp"
#include "example/distributed/consensus/core/signals.hpp"

#include "foundation/ipc/ipcservice.hpp"

#include "foundation/common.hpp"

#include "system/thread.hpp"
#include "system/debug.hpp"

namespace fdt=foundation;

ServerObject::ServerObject():crtval(1){
	
}
ServerObject::~ServerObject(){
	
}
//------------------------------------------------------------
namespace{
static const DynamicRegisterer<ServerObject>	dre;
}
/*static*/ void ServerObject::dynamicRegister(){
	DynamicExecuterT::registerDynamic<StoreSignal, ServerObject>();
	DynamicExecuterT::registerDynamic<FetchSignal, ServerObject>();
	DynamicExecuterT::registerDynamic<EraseSignal, ServerObject>();
	//DynamicExecuterT::registerDynamic<InsertSignal, ClientObject>();
}

int ServerObject::execute(ulong _sig, TimeSpec &_tout){
	foundation::Manager &rm(m());
	
	if(signaled()){//we've received a signal
		ulong sm(0);
		{
			Mutex::Locker	lock(rm.mutex(*this));
			sm = grabSignalMask(0);//grab all bits of the signal mask
			if(sm & fdt::S_KILL) return BAD;
			if(sm & fdt::S_SIG){//we have signals
				exe.prepareExecute();
			}
		}
		if(sm & fdt::S_SIG){//we've grabed signals, execute them
			while(exe.hasCurrent()){
				exe.executeCurrent(*this);
				exe.next();
			}
		}
		//now we determine if we return with NOK or we continue
		if(!_sig) return NOK;
	}
	
	return NOK;
}

//------------------------------------------------------------
/*virtual*/ bool ServerObject::signal(DynamicPointer<foundation::Signal> &_sig){
	if(this->state() < 0){
		_sig.clear();
		return false;//no reason to raise the pool thread!!
	}
	exe.push(DynamicPointer<>(_sig));
	return Object::signal(fdt::S_SIG | fdt::S_RAISE);
}


void ServerObject::dynamicExecute(DynamicPointer<> &_dp){
	
}


void ServerObject::dynamicExecute(DynamicPointer<StoreSignal> &_rsig){
	idbg("received InsertSignal request");
	const foundation::ipc::ConnectionUid	ipcconid(_rsig->ipcconid);
	
	++crtval;
	_rsig->v = crtval;
	
	DynamicPointer<foundation::Signal>		sigptr(_rsig);
	foundation::ipc::Service::the().sendSignal(sigptr, ipcconid);
}

void ServerObject::dynamicExecute(DynamicPointer<FetchSignal> &_rsig){
	idbg("received FetchSignal request");
	const foundation::ipc::ConnectionUid	ipcconid(_rsig->ipcconid);
	DynamicPointer<foundation::Signal>		sigptr(_rsig);
	
	foundation::ipc::Service::the().sendSignal(sigptr, ipcconid);
}

void ServerObject::dynamicExecute(DynamicPointer<EraseSignal> &_rsig){
	idbg("received EraseSignal request");
	const foundation::ipc::ConnectionUid	ipcconid(_rsig->ipcconid);
	DynamicPointer<foundation::Signal>		sigptr(_rsig);
	
	foundation::ipc::Service::the().sendSignal(sigptr, ipcconid);
}
