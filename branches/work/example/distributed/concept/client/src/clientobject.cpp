#include "example/distributed/concept/client/clientobject.hpp"
#include "example/distributed/concept/core/manager.hpp"
#include "foundation/common.hpp"
#include "system/thread.hpp"
#include "utility/binaryseeker.hpp"
#include "system/socketaddress.hpp"
#include <cstdio>


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
		AddrInfo ai(s.c_str(), s.c_str() + pos + 1, 0, AddrInfo::Inet4, AddrInfo::Stream);
		if(!ai.empty()){
			addrvec.push_back(SocketAddress(ai.begin()));
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
		SocketAddress &ra(*it);
		char				host[SocketAddress::MaxSockHostSz];
		char				port[SocketAddress::MaxSockServSz];
		ra.name(
			host,
			SocketAddress::MaxSockHostSz,
			port,
			SocketAddress::MaxSockServSz,
			SocketAddress::NumericService | SocketAddress::NumericHost
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