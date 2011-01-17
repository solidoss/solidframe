#include "example/distributed/concept/client/clientobject.hpp"
#include "example/distributed/concept/core/manager.hpp"
#include "foundation/common.hpp"
#include "system/thread.hpp"
#include "utility/binaryseeker.hpp"


namespace fdt=foundation;
using namespace std;
//------------------------------------------------------------
ClientParams::ClientParams(const ClientParams &_rcp):cnt(_rcp.cnt){
	
}
void ClientParams::print(std::ostream &_ros){
	_ros<<"Client Params:"<<endl;
	_ros<<"seqstr		: "<<seqstr<<endl;
	_ros<<"cnt			: "<<cnt<<endl;
	_ros<<"addrstrvec	: ";
	for(StringVectorT::const_iterator it(addrstrvec.begin()); it != addrstrvec.end(); ++it){
		_ros<<*it<<';';
	}
	_ros<<endl;
	_ros<<"strszvec		: "; 
	for(UInt32VectorT::const_iterator it(strszvec.begin()); it != strszvec.end(); ++it){
		_ros<<*it<<';';
	}
	_ros<<endl;
}
//------------------------------------------------------------
ClientObject::ClientObject(const ClientParams &_rcp):params(_rcp), crtreqid(1){
	
}
//------------------------------------------------------------
ClientObject::~ClientObject(){
	
}
//------------------------------------------------------------
void ClientObject::dynamicExecute(DynamicPointer<> &_dp){
	
}
//------------------------------------------------------------
void ClientObject::dynamicExecute(DynamicPointer<ClientSignal> &_rsig){
	
}
//------------------------------------------------------------
int ClientObject::execute(ulong _sig, TimeSpec &_tout){
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
struct ReqCmp{
	int operator()(const std::pair<uint32, int> &_v1, const uint32 _v2)const{
		//TODO: _v1.first is an id - problems when is near ffffffff
		//see ipc::processconnector.cpp
		if(_v1.first < _v2) return -1;
		if(_v1.first > _v2) return 1;
		return 0;
	}
};

static BinarySeeker<ReqCmp>	reqbs;

uint32 ClientObject::newRequestId(int _pos){
	uint32 rq = crtreqid;
	++crtreqid;
	if(crtreqid == 0) crtreqid = 1;
	if(rq > reqidvec.back().first){
		//push back
		reqidvec.push_back(std::pair<uint32, int>(rq, _pos));
		
	}else{
		int rv = reqbs(reqidvec.begin(), reqidvec.end(), rq);
		cassert(rv < 0);
		rv = -rv - 1;
		reqidvec.insert(reqidvec.begin() + rv, std::pair<uint32, int>(rq, _pos));
	}
	return rq;
}
//------------------------------------------------------------
bool ClientObject::isRequestIdExpected(uint32 _v, int &_rpos)const{
	int rv = reqbs(reqidvec.begin(), reqidvec.end(), _v);
	if(rv >= 0){
		_rpos = reqidvec[rv].second;
		return true;
	}else return false;
}
//------------------------------------------------------------
void ClientObject::deleteRequestId(uint32 _v){
	if(_v == 0) return;
	int rv = reqbs(reqidvec.begin(), reqidvec.end(), _v);
	cassert(rv < 0);
	rv = -rv - 1;
	reqidvec.erase(reqidvec.begin() + rv);
}
//------------------------------------------------------------