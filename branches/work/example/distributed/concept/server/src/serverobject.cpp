#include "example/distributed/concept/server/serverobject.hpp"
#include "example/distributed/concept/core/manager.hpp"
#include "example/distributed/concept/core/signals.hpp"

#include "foundation/common.hpp"

#include "system/thread.hpp"
#include "system/debug.hpp"

namespace fdt=foundation;

ServerObject::ServerObject(){
	
}
ServerObject::~ServerObject(){
	
}
//------------------------------------------------------------
namespace{
static const DynamicRegisterer<ServerObject>	dre;
}
/*static*/ void ServerObject::dynamicRegister(){
	DynamicExecuterT::registerDynamic<InsertSignal, ServerObject>();
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

void ServerObject::dynamicExecute(DynamicPointer<> &_dp){
	
}


void ServerObject::dynamicExecute(DynamicPointer<InsertSignal> &_rsig){
	idbg("received InsertSignal request");
}

void ServerObject::dynamicExecute(DynamicPointer<FetchSignal> &_rsig){
	idbg("received FetchSignal request");
}

void ServerObject::dynamicExecute(DynamicPointer<EraseSignal> &_rsig){
	idbg("received EraseSignal request");
}
