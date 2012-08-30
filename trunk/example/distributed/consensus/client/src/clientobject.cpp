#include "example/distributed/consensus/client/clientobject.hpp"
#include "example/distributed/consensus/core/consensusmanager.hpp"
#include "example/distributed/consensus/core/consensusrequests.hpp"

#include "foundation/common.hpp"
#include "foundation/ipc/ipcservice.hpp"

#include "utility/binaryseeker.hpp"

#include "system/socketaddress.hpp"
#include "system/thread.hpp"
#include "system/debug.hpp"
#include <cstdio>
#include <cstring>


namespace fdt=foundation;
using namespace std;
//------------------------------------------------------------
ClientParams::ClientParams(
	const ClientParams &_rcp
):cnt(_rcp.cnt), addrvec(_rcp.addrvec), reqvec(_rcp.reqvec), strvec(_rcp.strvec){
	
}
//------------------------------------------------------------
bool ClientParams::init(){
	//fill the address vector
	for(StringVectorT::const_iterator it(addrstrvec.begin()); it != addrstrvec.end(); ++it){
		string s(*it);
		size_t pos = s.find_last_of(':');
		if(pos == string::npos){
			err = "An address must be of host:port or host:service";
			return false;
		}
		s[pos] = 0;
		ResolveData rd = synchronous_resolve(
			s.c_str(), s.c_str() + pos + 1, 0,
			SocketInfo::Inet4, SocketInfo::Datagram
		);
		if(!rd.empty()){
			addrvec.push_back(SocketAddressInet4(rd.begin()));
		}else{
			err = "No such address [";
			err += s.c_str();
			err += ':';
			err += s.c_str() + pos + 1;
			err += ']';
			return false;
		}
	}
	//fill the request vector
	if(!parseRequests()) return false;
	//fill the string vector
	for(UInt32VectorT::const_iterator it(strszvec.begin()); it != strszvec.end(); ++it){
		char buf[16];
		sprintf(buf, "%8.8x", (uint32)(it - strszvec.begin()));
		char c('a' + ((uint32)(it - strszvec.begin()) % ((int)'z' - (int)'a') ));
		strvec.push_back("");
		strvec.back() = buf;
		for(uint32 i(0); i < *it; ++i){
			strvec.back() += c;
		}
	}
	return true;
}
//------------------------------------------------------------
const char* parseUInt32(const char* _p, uint32 &_ru){
	char *pend;
	_ru = strtoul(_p, &pend, 10);
	if(pend == _p) return NULL;
	return pend;
	
}
bool ClientParams::parseRequests(){
	const char *p(seqstr.c_str());
	
	while(p && *p){
		switch(*p){
			case 'i'://insert(uint32)
				reqvec.push_back(Request(*p));
				++p;
				p = parseUInt32(p, reqvec.back().u.u32s.u32_1);
				if(!p){
					err = "Error parsing uint32 value";
					return false;
				}
				break;
			case 'p'://pause(uint32)
				reqvec.push_back(Request(*p));
				++p;
				p = parseUInt32(p, reqvec.back().u.u32s.u32_1);
				if(!p){
					err = "Error parsing uint32 value";
					return false;
				}
				break;
			case 'f'://fetch(uint32)
				reqvec.push_back(Request(*p));
				++p;
				p = parseUInt32(p, reqvec.back().u.u32s.u32_1);
				if(!p){
					err = "Error parsing uint32 value";
					return false;
				}
				break;
			case 'e'://erase(uint32)
				reqvec.push_back(Request(*p));
				++p;
				p = parseUInt32(p, reqvec.back().u.u32s.u32_1);
				if(!p){
					err = "Error parsing uint32 value";
					return false;
				}
				break;
			case 'E'://erase all
				reqvec.push_back(Request(*p));
				++p;
			case ' ':
			case '\t':
				++p;
				break;
			default:
				err = "Unsupported opperation: ";
				err += *p;
				return false;
		}
	}
	return true;
}
//------------------------------------------------------------
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
	
	_ros<<"Parsed parameters:"<<endl;
	_ros<<"Addresses: ";
	for(AddressVectorT::iterator it(addrvec.begin()); it != addrvec.end(); ++it){
		SocketAddressInet4 &ra(*it);
		char				host[SocketInfo::HostStringCapacity];
		char				port[SocketInfo::ServiceStringCapacity];
		ra.toString(
			host,
			SocketInfo::HostStringCapacity,
			port,
			SocketInfo::ServiceStringCapacity,
			SocketInfo::NumericService | SocketInfo::NumericHost
		);
		_ros<<host<<':'<<port<<' ';
	}
	_ros<<endl;
	_ros<<"Requests: ";
	for(RequestVectorT::iterator it(reqvec.begin()); it != reqvec.end(); ++it){
		Request &rr(*it);
		_ros<<(char)rr.opp<<'('<<rr.u.u32s.u32_1<<')'<<' ';
	}
	_ros<<endl;
	_ros<<"Strings: "<<endl;
	for(StringVectorT::const_iterator it(strvec.begin()); it != strvec.end(); ++it){
		_ros<<'['<<*it<<']'<<endl;
	}
	_ros<<endl;
}
//------------------------------------------------------------
namespace{
static const DynamicRegisterer<ClientObject>	dre;
}
/*static*/ void ClientObject::dynamicRegister(){
	DynamicExecuterT::registerDynamic<StoreRequest, ClientObject>();
	DynamicExecuterT::registerDynamic<FetchRequest, ClientObject>();
	DynamicExecuterT::registerDynamic<EraseRequest, ClientObject>();
	//DynamicExecuterT::registerDynamic<InsertSignal, ClientObject>();
}
//------------------------------------------------------------
ClientObject::ClientObject(
	const ClientParams &_rcp
):params(_rcp), crtreqid(1),crtreqpos(0), crtpos(0),waitresponsecount(0){
	state(Execute);
}
//------------------------------------------------------------
ClientObject::~ClientObject(){
	idbg("");
}
//------------------------------------------------------------
int ClientObject::execute(ulong _sig, TimeSpec &_tout){
	foundation::Manager &rm(m());
	
	if(signaled()){//we've received a signal
		ulong sm(0);
		{
			Locker<Mutex>	lock(rm.mutex(*this));
			sm = grabSignalMask(0);//grab all bits of the signal mask
			if(sm & fdt::S_KILL){
				idbg("die");
				m().signalStop();
				return BAD;
			}
			if(sm & fdt::S_SIG){//we have signals
				exe.prepareExecute(this);
			}
		}
		if(sm & fdt::S_SIG){//we've grabed signals, execute them
			while(exe.hasCurrent(this)){
				exe.executeCurrent(this);
				exe.next(this);
			}
		}
		//now we determine if we return with NOK or we continue
		//if(!_sig) return NOK;
	}
	idbg("ping");
	if(crtpos < params.cnt){
		ClientParams::Request &rr(params.reqvec[crtreqpos]);
		idbg("opp = "<<rr.opp);
		switch(rr.opp){
			case 'i':{
				idbg("sending a storesingal");
				const string	&s(getString(rr.u.u32s.u32_1, crtpos));
				uint32			sid(sendSignal(new StoreRequest(s, crtpos)));
				expectStore(sid, s, crtpos, params.addrvec.size());
				break;
			}
			case 'p':
				if(state() == Execute){
					_tout += rr.u.u32s.u32_1;
					nexttimepos = _tout;
					idbg("wait "<<rr.u.u32s.u32_1<<" "<<_tout.seconds()<<':'<<_tout.nanoSeconds());
					state(Wait);
					return NOK;
				}else{
					if(nexttimepos <= _tout){
						idbg("done wait "<<" "<<nexttimepos.seconds()<<':'<<nexttimepos.nanoSeconds());
						state(Execute);
						break;
					}else{
						idbg("still wait "<<" "<<nexttimepos.seconds()<<':'<<nexttimepos.nanoSeconds());
						idbg("still wait "<<" "<<_tout.seconds()<<':'<<_tout.nanoSeconds());
						_tout = nexttimepos;
						return NOK;
					}
				}
			case 'f':{
				const string	&s(params.strvec[rr.u.u32s.u32_1]);
				uint32			sid(sendSignal(new FetchRequest(s)));
				expectFetch(sid, s, params.addrvec.size());
				break;
			}	
			case 'e':{
				const string	&s(params.strvec[rr.u.u32s.u32_1]);
				uint32			sid(sendSignal(new EraseRequest(s)));
				expectErase(sid, s, params.addrvec.size());
				break;
			}
			case 'E':{
				uint32	sid(sendSignal(new EraseRequest));
				expectErase(sid, params.addrvec.size());
				break;
			}

			default:
				wdbg("skip unknown request");
				break;
		}
		++crtreqpos;
		if(crtreqpos >= params.reqvec.size()){
			crtreqpos = 0;
			++crtpos;
			return OK;
		}
		return OK;
	}else if(waitresponsecount){
		if(_sig & fdt::TIMEOUT){
			m().signalStop();
		}else{
			idbg("waiting for "<<waitresponsecount<<" responses");
			_tout.add(1 * 60);
		}
	}else{
		m().signalStop();
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
	if(reqidvec.size() && rq > reqidvec.back().first){
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
uint32 ClientObject::sendSignal(distributed::consensus::WriteRequestSignal *_psig){
	DynamicSharedPointer<distributed::consensus::WriteRequestSignal>	sigptr(_psig);
	//sigptr->requestId(newRequestId(-1));
	sigptr->waitresponse = true;
	sigptr->id.requid = newRequestId(-1);
	sigptr->id.senderuid = this->uid();
	for(ClientParams::AddressVectorT::iterator it(params.addrvec.begin()); it != params.addrvec.end(); ++it){
		DynamicPointer<foundation::Signal>	sp(sigptr);
		foundation::ipc::Service::the().sendSignal(sp, SocketAddressStub(*it));
	}
	return 0;
}
//------------------------------------------------------------
const string& ClientObject::getString(uint32 _pos, uint32 _crtpos){
	static const string df("");
	if(_pos < params.strvec.size()){
		string &rs(params.strvec[_pos]);
		char buf[16];
		sprintf(buf, "%8.8x", _crtpos);
		memcpy((void*)rs.data(), buf, 8);
		return rs;
	}else{
		return df;
	}
}
//------------------------------------------------------------
/*virtual*/ bool ClientObject::signal(DynamicPointer<foundation::Signal> &_sig){
	if(this->state() < 0){
		_sig.clear();
		return false;//no reason to raise the pool thread!!
	}
	exe.push(this, DynamicPointer<>(_sig));
	return Object::signal(fdt::S_SIG | fdt::S_RAISE);
}
//------------------------------------------------------------
void ClientObject::dynamicExecute(DynamicPointer<> &_dp){
	idbg("received unknown dynamic object");
}
//------------------------------------------------------------
void ClientObject::dynamicExecute(DynamicPointer<ClientSignal> &_rsig){
	idbg("received ClientSignal response");
}
//------------------------------------------------------------
void ClientObject::dynamicExecute(DynamicPointer<StoreRequest> &_rsig){
	--waitresponsecount;
	idbg("received StoreSignal response with value "<<_rsig->v<<" waitresponsecount = "<<waitresponsecount);
}
//------------------------------------------------------------
void ClientObject::dynamicExecute(DynamicPointer<FetchRequest> &_rsig){
	idbg("received FetchSignal response");
}
//------------------------------------------------------------
void ClientObject::dynamicExecute(DynamicPointer<EraseRequest> &_rsig){
	idbg("received EraseSignal response");
}
//------------------------------------------------------------
void ClientObject::expectStore(uint32 _rid, const string &_rs, uint32 _v, uint32 _cnt){
	waitresponsecount += (_cnt);
}
//------------------------------------------------------------
void ClientObject::expectFetch(uint32 _rid, const string &_rs, uint32 _cnt){
}
//------------------------------------------------------------
void ClientObject::expectErase(uint32 _rid, const string &_rs, uint32 _cnt){
	
}
//------------------------------------------------------------
void ClientObject::expectErase(uint32 _rid, uint32 _cnt){
	
}
//------------------------------------------------------------
