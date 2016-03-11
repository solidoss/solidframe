#include "example/consensus/client/clientobject.hpp"
#include "example/consensus/core/consensusmanager.hpp"
#include "example/consensus/core/consensusrequests.hpp"

#include "frame/common.hpp"
#include "frame/ipc/ipcservice.hpp"

#include "utility/binaryseeker.hpp"

#include "system/socketaddress.hpp"
#include "system/thread.hpp"
#include "system/debug.hpp"
#include <cstdio>
#include <cstring>

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>


using namespace std;
using namespace solid;
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
	string		hoststr;
	string		servstr;
	for(AddressVectorT::iterator it(addrvec.begin()); it != addrvec.end(); ++it){
		SocketAddressInet4 &ra(*it);
		synchronous_resolve(
			hoststr,
			servstr,
			ra,
			ReverseResolveInfo::Numeric
		);
		_ros<<hoststr<<':'<<servstr<<' ';
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

/*static*/ ClientObject::DynamicMapperT ClientObject::dm;

/*static*/ void ClientObject::dynamicRegister(){
	dm.insert<StoreRequest, ClientObject>();
	dm.insert<FetchRequest, ClientObject>();
	dm.insert<EraseRequest, ClientObject>();
	//DynamicHandlerT::registerDynamic<InsertSignal, ClientObject>();
}
//------------------------------------------------------------
ClientObject::ClientObject(
	const ClientParams &_rcp,
	solid::frame::ipc::Service &_ripcsvc
):params(_rcp), ripcsvc(_ripcsvc), crtreqid(1),crtreqpos(0), crtpos(0),waitresponsecount(0){
	state(Execute);
}
//------------------------------------------------------------
ClientObject::~ClientObject(){
	idbg("");
}
//------------------------------------------------------------
void ClientObject::execute(ExecuteContext &_rexectx){
	frame::Manager &rm(frame::Manager::specific());
	
	if(notified()){//we've received a signal
		solid::ulong							sm(0);
		DynamicHandler<DynamicMapperT>	dh(dm);
		{
			Locker<Mutex>	lock(rm.mutex(*this));
			sm = grabSignalMask(0);//grab all bits of the signal mask
			if(sm & frame::S_KILL){
				idbg("die");
				kill(getpid(), SIGTERM);
				_rexectx.close();
				return;
			}
			if(sm & frame::S_SIG){//we have signals
				dh.init(dv.begin(), dv.end());
				dv.clear();
			}
		}
		if(sm & frame::S_SIG){//we've grabed signals, execute them
			for(size_t i = 0; i < dh.size(); ++i){
				dh.handle(*this, i);
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
				idbg("sending a storemessage");
				const string	&s(getString(rr.u.u32s.u32_1, crtpos));
				uint32			sid(sendMessage(new StoreRequest(s, crtpos)));
				expectStore(sid, s, crtpos, 1/*params.addrvec.size()*/);
				break;
			}
			case 'p':
				if(state() == Execute){
					nexttimepos = _rexectx.currentTime();
					nexttimepos.add(rr.u.u32s.u32_1);
					idbg("wait "<<rr.u.u32s.u32_1<<" "<<nexttimepos.seconds()<<':'<<nexttimepos.nanoSeconds());
					state(Wait);
					_rexectx.waitUntil(nexttimepos);
					return;
				}else{
					if(nexttimepos <= _rexectx.currentTime()){
						idbg("done wait "<<" "<<nexttimepos.seconds()<<':'<<nexttimepos.nanoSeconds());
						state(Execute);
						break;
					}else{
						idbg("still wait "<<" "<<nexttimepos.seconds()<<':'<<nexttimepos.nanoSeconds());
						idbg("still wait "<<" "<<_rexectx.currentTime().seconds()<<':'<<_rexectx.currentTime().nanoSeconds());
						_rexectx.waitUntil(nexttimepos);
						return;
					}
				}
			case 'f':{
				const string	&s(params.strvec[rr.u.u32s.u32_1]);
				uint32			sid(sendMessage(new FetchRequest(s)));
				expectFetch(sid, s, params.addrvec.size());
				break;
			}	
			case 'e':{
				const string	&s(params.strvec[rr.u.u32s.u32_1]);
				uint32			sid(sendMessage(new EraseRequest(s)));
				expectErase(sid, s, params.addrvec.size());
				break;
			}
			case 'E':{
				uint32	sid(sendMessage(new EraseRequest));
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
			return;
		}
		_rexectx.reschedule();
		return;
	}else if(waitresponsecount){
		if(_rexectx.eventMask() & frame::EventTimeout){
			kill(getpid(), SIGTERM);
		}else{
			idbg("waiting for "<<waitresponsecount<<" responses");
			_rexectx.waitFor(TimeSpec(1*60));
		}
	}else{
		kill(getpid(), SIGTERM);
	}
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
		BinarySeekerResultT rv = reqbs(reqidvec.begin(), reqidvec.end(), rq);
		reqidvec.insert(reqidvec.begin() + rv.first, std::pair<uint32, int>(rq, _pos));
	}
	return rq;
}
//------------------------------------------------------------
bool ClientObject::isRequestIdExpected(uint32 _v, int &_rpos)const{
	BinarySeekerResultT rv = reqbs(reqidvec.begin(), reqidvec.end(), _v);
	if(rv.second){
		_rpos = reqidvec[rv.first].second;
		return true;
	}else return false;
}
//------------------------------------------------------------
void ClientObject::deleteRequestId(uint32 _v){
	if(_v == 0) return;
	BinarySeekerResultT rv = reqbs(reqidvec.begin(), reqidvec.end(), _v);
	reqidvec.erase(reqidvec.begin() + rv.first);
}
//------------------------------------------------------------
uint32 ClientObject::sendMessage(consensus::WriteRequestMessage *_pmsg){
	DynamicSharedPointer<solid::consensus::WriteRequestMessage>	reqptr(_pmsg);
	//sigptr->requestId(newRequestId(-1));
	reqptr->consensusExpectCount(params.addrvec.size());
	reqptr->consensusRequestId(consensus::RequestId(newRequestId(-1), solid::frame::Manager::specific().id(*this)));
	
	for(ClientParams::AddressVectorT::iterator it(params.addrvec.begin()); it != params.addrvec.end(); ++it){
		DynamicPointer<frame::ipc::Message>	msgptr(reqptr);
		ripcsvc.sendMessage(msgptr, SocketAddressStub(*it));
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
/*virtual*/ bool ClientObject::notify(DynamicPointer<frame::Message> &_rmsgptr){
	if(this->state() < 0){
		_rmsgptr.clear();
		return false;//no reason to raise the pool thread!!
	}
	dv.push_back(DynamicPointer<>(_rmsgptr));
	return Object::notify(frame::S_SIG | frame::S_RAISE);
}
//------------------------------------------------------------
void ClientObject::dynamicHandle(DynamicPointer<> &_dp){
	idbg("received unknown dynamic object");
}
//------------------------------------------------------------
void ClientObject::dynamicHandle(DynamicPointer<ClientMessage> &_rmsgptr){
	idbg("received ClientMessage response");
}
//------------------------------------------------------------
void ClientObject::dynamicHandle(DynamicPointer<StoreRequest> &_rmsgptr){
	--waitresponsecount;
	idbg("received StoreRequest response with value "<<_rmsgptr->v<<" waitresponsecount = "<<waitresponsecount);
}
//------------------------------------------------------------
void ClientObject::dynamicHandle(DynamicPointer<FetchRequest> &_rmsgptr){
	idbg("received FetchRequest response");
}
//------------------------------------------------------------
void ClientObject::dynamicHandle(DynamicPointer<EraseRequest> &_rmsgptr){
	idbg("received EraseRequest response");
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
