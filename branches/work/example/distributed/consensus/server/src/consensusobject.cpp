#include <deque>
#include <queue>
#include <vector>
#include <algorithm>

#include "system/common.hpp"
#include "system/exception.hpp"

#ifdef HAVE_CPP11
#include <unordered_map>
#include <unordered_set>
#else
#include <map>
#endif

#include "system/timespec.hpp"

#include "algorithm/serialization/binary.hpp"
#include "algorithm/serialization/idtypemap.hpp"


#include "foundation/object.hpp"
#include "foundation/manager.hpp"
#include "foundation/signal.hpp"
#include "foundation/ipc/ipcservice.hpp"

#include "utility/stack.hpp"
#include "utility/queue.hpp"

#include "example/distributed/consensus/core/consensusrequest.hpp"
#include "example/distributed/consensus/server/consensusobject.hpp"
#include "timerqueue.hpp"
#include "consensussignals.hpp"

namespace fdt=foundation;
using namespace std;

namespace consensus{


/*static*/ Parameters& Parameters::the(Parameters *_p){
	static Parameters &r(*_p);
	return r;
}

Parameters::Parameters(){
	the(this);
}


//========================================================
struct RequestStub{
	enum{
		InitState = 0,
		WaitProposeState,
		WaitProposeAcceptState,
		AcceptWaitRequestState,
		AcceptState,
		AcceptPendingState,
	};
	enum{
		SignaledEvent = 1,
		TimeoutEvent = 2,
		
	};
	enum{
		HaveRequestFlag = 1,
	};
	RequestStub(): evs(0), flags(0), proposeid(-1), acceptid(-1), timerid(0), state(InitState), recvpropacc(0){}
	RequestStub(
		DynamicPointer<consensus::RequestSignal> &_rsig
	):sig(_rsig), proposeid(-1), acceptid(-1), timerid(0), state(InitState), recvpropacc(0){}
	
	bool hasRequest()const{
		return flags & HaveRequestFlag;
	}
	
	DynamicPointer<consensus::RequestSignal>	sig;
	uint16										evs;
	uint16										flags;
	uint32										proposeid;
	uint32										acceptid;
	uint16										timerid;
	uint8										state;
	uint8										recvpropacc;//received accepts for propose
};

typedef std::deque<RequestStub>		RequestStubVectorT;

struct Object::Data{
	enum{
		ProposeOperation = 1,
		ProposeAcceptOperation,
		AcceptOperation,
	};
	struct ReqCmpEqual{
		bool operator()(const consensus::RequestId* const & _req1, const consensus::RequestId* const & _req2)const;
	};
	struct ReqCmpLess{
		bool operator()(const consensus::RequestId* const & _req1, const consensus::RequestId* const & _req2)const;
	};
	struct ReqHash{
		size_t operator()(const consensus::RequestId* const & _req1)const;
	};
	struct SenderCmpEqual{
		bool operator()(const consensus::RequestId & _req1, const consensus::RequestId& _req2)const;
	};
	struct SenderCmpLess{
		bool operator()(const consensus::RequestId& _req1, const consensus::RequestId& _req2)const;
	};
	struct SenderHash{
		size_t operator()(const consensus::RequestId& _req1)const;
	};
	
#ifdef HAVE_CPP11
	typedef std::unordered_map<const consensus::RequestId*, size_t, ReqHash, ReqCmpEqual>		RequestStubMapT;
	typedef std::unordered_set<consensus::RequestId, SenderHash, SenderCmpEqual>				SenderSetT;
#else
	typedef std::map<const consensus::RequestId*, size_t, ReqCmpLess>							RequestStubMapT;
	typedef std::set<consensus::RequestId, SenderCmpLess>										SenderSetT;
#endif
	typedef Stack<size_t>																		SizeTStackT;
	typedef Queue<size_t>																		SizeTQueueT;

//methods:
	Data();
	~Data();
	
	bool insertRequestStub(DynamicPointer<RequestSignal> &_rsig, size_t &_ridx);
	void eraseRequestStub(size_t _idx);
	RequestStub& requestStub(size_t _idx);
	const RequestStub& requestStub(size_t _idx)const;
	bool checkAlreadyReceived(DynamicPointer<RequestSignal> &_rsig);
	bool isCoordinator()const;
	bool canSendAcceptOnly()const;
	void coordinatorId(int8 _coordid);
	
	
//data:	
	DynamicExecuterT		exe;
	uint32					proposeid;
		
	uint32					acceptid;
	
	int8					coordinatorid;
	int8					distancefromcoordinator;
	uint8					acceptpendingcnt;//the number of stubs in waitrequest state
	uint32					continuous_accepted_proposes;
	TimeSpec				lastaccepttime;
	
	RequestStubMapT			reqmap;
	RequestStubVectorT		reqvec;
	SizeTQueueT				reqq;
	SizeTStackT				freeposstk;
	SenderSetT				senderset;
	TimerQueue				timerq;
};

inline bool Object::Data::ReqCmpEqual::operator()(
	const consensus::RequestId* const & _req1,
	const consensus::RequestId* const & _req2
)const{
	return *_req1 == *_req2;
}
inline bool Object::Data::ReqCmpLess::operator()(
	const consensus::RequestId* const & _req1,
	const consensus::RequestId* const & _req2
)const{
	return *_req1 < *_req2;
}
inline size_t Object::Data::ReqHash::operator()(
		const consensus::RequestId* const & _req1
)const{
	return _req1->hash();
}
inline bool Object::Data::SenderCmpEqual::operator()(
	const consensus::RequestId & _req1,
	const consensus::RequestId& _req2
)const{
	return _req1.senderEqual(_req2);
}
inline bool Object::Data::SenderCmpLess::operator()(
	const consensus::RequestId& _req1,
	const consensus::RequestId& _req2
)const{
	return _req1.senderLess(_req2);
}
inline size_t Object::Data::SenderHash::operator()(
	const consensus::RequestId& _req1
)const{
	return _req1.senderHash();
}


Object::Data::Data():proposeid(0), acceptid(0), coordinatorid(-2), acceptpendingcnt(0){
	if(Parameters::the().idx){
		coordinatorid = 0;
	}else{
		coordinatorid = -1;
	}
}
Object::Data::~Data(){
	
}
void Object::Data::coordinatorId(int8 _coordid){
	coordinatorid = _coordid;
	if(_coordid != (int8)-1){
		int8 maxsz(Parameters::the().addrvec.size());
		distancefromcoordinator = circular_distance(static_cast<int8>(Parameters::the().idx), coordinatorid, maxsz);
	}else{
		distancefromcoordinator = -1;
	}
}
bool Object::Data::insertRequestStub(DynamicPointer<RequestSignal> &_rsig, size_t &_ridx){
	auto	it(reqmap.find(&_rsig->id));
	if(it != reqmap.end()){
		_ridx = it->second;
		reqmap.erase(it);
		RequestStub &rr(requestStub(_ridx));
		cassert(rr.flags & RequestStub::HaveRequestFlag);
		reqmap[&rr.sig->id] = _ridx;
		return false;
	}
	if(freeposstk.size()){
		_ridx = freeposstk.top();
		freeposstk.pop();
		RequestStub &rcr(requestStub(_ridx));
		rcr.sig = _rsig;
	}else{
		_ridx = reqvec.size();
		reqvec.push_back(RequestStub(_rsig));
	}
	reqmap[&requestStub(_ridx).sig->id] = _ridx;
	return true;
}
void Object::Data::eraseRequestStub(size_t _idx){
	RequestStub &rcr(requestStub(_idx));
	cassert(rcr.sig.ptr());
	rcr.sig.clear();
	rcr.state = 0;
	rcr.evs = 0;
	++rcr.timerid;
	freeposstk.push(_idx);
}
inline RequestStub& Object::Data::requestStub(size_t _idx){
	cassert(_idx < reqvec.size());
	return reqvec[_idx];
}
inline const RequestStub& Object::Data::requestStub(size_t _idx)const{
	cassert(_idx < reqvec.size());
	return reqvec[_idx];
}
inline bool Object::Data::isCoordinator()const{
	return coordinatorid == -1;
}
bool Object::Data::checkAlreadyReceived(DynamicPointer<RequestSignal> &_rsig){
	SenderSetT::iterator it(senderset.find(_rsig->id));
	if(it != senderset.end()){
		if(overflow_safe_less(_rsig->id.requid, it->requid)){
			return true;
		}
		senderset.erase(it);
		senderset.insert(_rsig->id);
	}else{
		senderset.insert(_rsig->id);
	}
	return false;
}
bool Object::Data::canSendAcceptOnly()const{
	TimeSpec ct(fdt::Object::currentTime());
	ct -= lastaccepttime;
	return (continuous_accepted_proposes >= 5) && ct.seconds() < 60;
}
//========================================================
//Runntime data
struct Object::RunData{
	enum{
		OperationCapacity = 32
	};
	struct OperationStub{
		size_t		reqidx;
		uint8		operation;
		uint32		proposeid;
		uint32		acceptid;
	};
	RunData(
		ulong _sig,
		TimeSpec &_rts,
		int8 _coordinatorid
	):signals(_sig), rtimepos(_rts), coordinatorid(_coordinatorid), opcnt(0){}
	
	ulong			signals;
	TimeSpec		&rtimepos;
	int8			coordinatorid;
	size_t			opcnt;
	OperationStub	ops[OperationCapacity];
};
//========================================================
namespace{
static const DynamicRegisterer<Object>	dre;
}

/*static*/ void Object::dynamicRegister(){
	DynamicExecuterT::registerDynamic<RequestSignal, Object>();
	DynamicExecuterT::registerDynamic<OperationSignal<1>, Object>();
	DynamicExecuterT::registerDynamic<OperationSignal<2>, Object>();
	DynamicExecuterT::registerDynamic<OperationSignal<4>, Object>();
	DynamicExecuterT::registerDynamic<OperationSignal<8>, Object>();
	DynamicExecuterT::registerDynamic<OperationSignal<16>, Object>();
	DynamicExecuterT::registerDynamic<OperationSignal<32>, Object>();
	//TODO: add here the other consensus Signals
}
/*static*/ void Object::registerSignals(){
	typedef serialization::TypeMapper					TypeMapper;
	typedef serialization::bin::Serializer				BinSerializer;
	typedef serialization::bin::Deserializer			BinDeserializer;

	TypeMapper::map<OperationSignal<1>, BinSerializer, BinDeserializer>();
	TypeMapper::map<OperationSignal<2>, BinSerializer, BinDeserializer>();
	TypeMapper::map<OperationSignal<4>, BinSerializer, BinDeserializer>();
	TypeMapper::map<OperationSignal<8>, BinSerializer, BinDeserializer>();
	TypeMapper::map<OperationSignal<16>, BinSerializer, BinDeserializer>();
	TypeMapper::map<OperationSignal<32>, BinSerializer, BinDeserializer>();
}
Object::Object():d(*(new Data)){
	
}
//---------------------------------------------------------
Object::~Object(){
	delete &d;
}
//---------------------------------------------------------
void Object::dynamicExecute(DynamicPointer<> &_dp){
	
}
//---------------------------------------------------------
void Object::dynamicExecute(DynamicPointer<RequestSignal> &_rsig){
	if(d.checkAlreadyReceived(_rsig)) return;
	size_t idx;
	if(d.insertRequestStub(_rsig, idx)){
		
	}else{
		//updating existing request stub
	}
	RequestStub &rreq(d.requestStub(idx));
	rreq.flags |= RequestStub::HaveRequestFlag;
	if(!(rreq.evs & RequestStub::SignaledEvent)){
		rreq.evs |= RequestStub::SignaledEvent;
		d.reqq.push(d.timerq.frontIndex());
	}
}
void Object::dynamicExecute(DynamicPointer<OperationSignal<1> > &_rsig){
	doExecuteOperation(_rsig->replicaidx, _rsig->op);
}
void Object::dynamicExecute(DynamicPointer<OperationSignal<2> > &_rsig){
	doExecuteOperation(_rsig->replicaidx, _rsig->op[0]);
	doExecuteOperation(_rsig->replicaidx, _rsig->op[1]);
}
void Object::dynamicExecute(DynamicPointer<OperationSignal<4> > &_rsig){
	for(size_t i(0); i < _rsig->opsz; ++i){
		doExecuteOperation(_rsig->replicaidx, _rsig->op[i]);
	}
}
void Object::dynamicExecute(DynamicPointer<OperationSignal<8> > &_rsig){
	for(size_t i(0); i < _rsig->opsz; ++i){
		doExecuteOperation(_rsig->replicaidx, _rsig->op[i]);
	}
}
void Object::dynamicExecute(DynamicPointer<OperationSignal<16> > &_rsig){
	for(size_t i(0); i < _rsig->opsz; ++i){
		doExecuteOperation(_rsig->replicaidx, _rsig->op[i]);
	}
}
void Object::dynamicExecute(DynamicPointer<OperationSignal<32> > &_rsig){
	for(size_t i(0); i < _rsig->opsz; ++i){
		doExecuteOperation(_rsig->replicaidx, _rsig->op[i]);
	}
}
void Object::doExecuteOperation(uint8 _replicaidx, OperationStub &_rop){
	auto	it(d.reqmap.find(&_rop.reqid));
	size_t	reqidx;
	if(it != d.reqmap.end()){
		reqidx = it->second;
	}else{
		DynamicPointer<RequestSignal> reqsig(new RequestSignal(_rop.reqid));
		d.insertRequestStub(reqsig, reqidx);
	}
	switch(_rop.operation){
		case Data::ProposeOperation:
			doExecuteProposeOperation(reqidx, _rop);
			break;
		case Data::ProposeAcceptOperation:
			doExecuteProposeAcceptOperation(reqidx, _rop);
			break;
		case Data::AcceptOperation:
			doExecuteAcceptOperation(reqidx, _rop);
			break;
		default:
			THROW_EXCEPTION_EX("Unknown operation ", (int)_rop.operation);
	}
	
}
void Object::doExecuteProposeOperation(size_t _reqidx, OperationStub &_rop){
	
}
void Object::doExecuteProposeAcceptOperation(size_t _reqidx, OperationStub &_rop){
	
}
void Object::doExecuteAcceptOperation(size_t _reqidx, OperationStub &_rop){
	
}
//---------------------------------------------------------
/*virtual*/ bool Object::signal(DynamicPointer<foundation::Signal> &_sig){
	if(this->state() < 0){
		_sig.clear();
		return false;//no reason to raise the pool thread!!
	}
	d.exe.push(DynamicPointer<>(_sig));
	return fdt::Object::signal(fdt::S_SIG | fdt::S_RAISE);
}
//---------------------------------------------------------
int Object::execute(ulong _sig, TimeSpec &_tout){
	foundation::Manager &rm(fdt::m());
	
	if(signaled()){//we've received a signal
		ulong sm(0);
		{
			Mutex::Locker	lock(rm.mutex(*this));
			sm = grabSignalMask(0);//grab all bits of the signal mask
			if(sm & fdt::S_KILL) return BAD;
			if(sm & fdt::S_SIG){//we have signals
				d.exe.prepareExecute();
			}
		}
		if(sm & fdt::S_SIG){//we've grabed signals, execute them
			while(d.exe.hasCurrent()){
				d.exe.executeCurrent(*this);
				d.exe.next();
			}
		}
	}
	RunData	rd(_sig, _tout, d.coordinatorid);
	switch(state()){
		case Init:		return doInit(rd);
		case Run:		return doRun(rd);
		case Update:	return doUpdate(rd);
	}
	return NOK;
}
//---------------------------------------------------------
bool Object::isCoordinator()const{
	return d.isCoordinator();
}
uint32 Object::acceptId()const{
	return d.acceptid;
}
uint32 Object::proposeId()const{
	return d.proposeid;
}
//---------------------------------------------------------
int Object::doInit(RunData &_rd){
	state(Run);
	return OK;
}
//---------------------------------------------------------
int Object::doRun(RunData &_rd){
	//first we scan for timeout:
	while(d.timerq.hitted(_rd.rtimepos)){
		RequestStub &rreq(d.requestStub(d.timerq.frontIndex()));
		
		if(rreq.timerid == d.timerq.frontUid()){
			rreq.evs |= RequestStub::TimeoutEvent;
			if(!(rreq.evs & RequestStub::SignaledEvent)){
				rreq.evs |= RequestStub::SignaledEvent;
				d.reqq.push(d.timerq.frontIndex());
			}
		}
		
		d.timerq.pop();
	}
	
	//next we process all request
	size_t cnt(d.reqq.size());
	while(cnt--){
		size_t pos(d.reqq.front());
		d.reqq.pop();
		doProcessRequest(_rd, pos);
	}
	if(d.reqq.size()) return OK;
	
	//set the timer value and exit
	if(d.timerq.size()){
		if(d.timerq.hitted(_rd.rtimepos)){
			return OK;
		}
		_rd.rtimepos = d.timerq.frontTime();
	}
	
	return NOK;
}
//---------------------------------------------------------
int Object::doUpdate(RunData &_rd){
	state(Run);
	return OK;
}

void Object::doProcessRequest(RunData &_rd, size_t _pos){
	RequestStub &rreq(d.requestStub(_pos));
	uint32 events = rreq.evs;
	rreq.evs = 0;
	switch(rreq.state){
		case RequestStub::InitState:
			if(d.isCoordinator()){
				if(d.canSendAcceptOnly()){
					doSendAccept(_rd, _pos);
					if(!(rreq.evs & RequestStub::SignaledEvent)){
						rreq.evs |= RequestStub::SignaledEvent;
						d.reqq.push(_pos);
					}
					rreq.state = RequestStub::AcceptState;
				}else{
					doSendPropose(_rd, _pos);
					rreq.state = RequestStub::WaitProposeAcceptState;
					TimeSpec ts(fdt::Object::currentTime());
					ts += 3000;//ms
					d.timerq.push(ts, _pos, rreq.timerid);
					rreq.recvpropacc = 1;//one is the propose_accept from current coordinator
				}
			}else{
				rreq.state = RequestStub::WaitProposeState;
				TimeSpec ts(fdt::Object::currentTime());
				ts += d.distancefromcoordinator * 500;//ms
				d.timerq.push(ts, _pos, rreq.timerid);
			}
			break;
		case RequestStub::WaitProposeState:
			if(rreq.evs & RequestStub::TimeoutEvent){
				//TODO: enter Object in update state
				break;
			}
			break;
		case RequestStub::WaitProposeAcceptState:
			if(rreq.evs & RequestStub::TimeoutEvent){
				//TODO: enter Object in update state
				break;
			}
			break;
		case RequestStub::AcceptWaitRequestState:
			if(rreq.evs & RequestStub::TimeoutEvent){
				//TODO: enter Object in update state
				break;
			}
			if(!(rreq.flags & RequestStub::HaveRequestFlag)){
				break;
			}
			if(overflow_safe_less(rreq.acceptid, d.acceptid + 1)){
				doEraseRequest(_rd, _pos);
				break;
			}
			if(d.acceptid + 1 != rreq.acceptid){
				rreq.state = RequestStub::AcceptPendingState;
				break;
			}
			doAcceptRequest(_rd, _pos);
			doEraseRequest(_rd, _pos);
			doScanPendingRequests(_rd);
			break;
		case RequestStub::AcceptState:
			if(!(rreq.flags & RequestStub::HaveRequestFlag)){
				rreq.state = RequestStub::AcceptWaitRequestState;
				TimeSpec ts(fdt::Object::currentTime());
				ts.add(30);//30 secs
				d.timerq.push(ts, _pos, rreq.timerid);
				break;
			}
			if(overflow_safe_less(rreq.acceptid, d.acceptid + 1)){
				doEraseRequest(_rd, _pos);
				break;
			}
			if(d.acceptid + 1 != rreq.acceptid){
				rreq.state = RequestStub::AcceptPendingState;
				if(d.acceptpendingcnt < 255){
					++d.acceptpendingcnt;
				}
				break;
			}
			doAcceptRequest(_rd, _pos);
			doEraseRequest(_rd, _pos);
			break;
		case RequestStub::AcceptPendingState:
			//the status is changed within doScanPendingRequests
			break;
		default:
			THROW_EXCEPTION_EX("Unknown state ",rreq.state);
	}
}

void Object::doSendAccept(RunData &_rd, size_t _pos){
	
}

void Object::doSendPropose(RunData &_rd, size_t _pos){
	
}
void Object::doFlushOperations(RunData &_rd){
	Signal *ps(NULL);
	OperationStub *pos(NULL);
	
	if(_rd.opcnt == 0){
		return;
	}else if(_rd.opcnt == 1){
		OperationSignal<1>	*po(new OperationSignal<1>);
		RequestStub	&rrs(d.requestStub(_rd.ops[0].reqidx));
		ps = po;
		
		po->replicaidx = Parameters::the().idx;
		
		po->op.operation = _rd.ops[0].operation;
		po->op.acceptid = _rd.ops[0].acceptid;
		po->op.proposeid = _rd.ops[0].proposeid;
		po->op.reqid = rrs.sig->id;
	}else if(_rd.opcnt == 2){
		OperationSignal<2>	*po(new OperationSignal<2>);
		ps = po;
		po->replicaidx = Parameters::the().idx;
		pos = po->op;
	}else if(_rd.opcnt <= 4){
		OperationSignal<4>	*po(new OperationSignal<4>);
		ps = po;
		po->replicaidx = Parameters::the().idx;
		pos = po->op;
	}else if(_rd.opcnt <= 8){
		OperationSignal<8>	*po(new OperationSignal<8>);
		ps = po;
		po->replicaidx = Parameters::the().idx;
		pos = po->op;
	}else if(_rd.opcnt <= 16){
		OperationSignal<16>	*po(new OperationSignal<16>);
		ps = po;
		po->replicaidx = Parameters::the().idx;
		pos = po->op;
	}else if(_rd.opcnt <= 32){
		OperationSignal<32>	*po(new OperationSignal<32>);
		ps = po;
		po->replicaidx = Parameters::the().idx;
		pos = po->op;
	}else{
		THROW_EXCEPTION_EX("invalid opcnt ",_rd.opcnt);
	}
	if(pos){
		for(uint i(0); i < _rd.opcnt; ++i){
			RequestStub	&rrs(d.requestStub(_rd.ops[i].reqidx));
			//po->oparr.push_back(OperationStub());
			
			pos[i].operation = _rd.ops[i].operation;
			pos[i].acceptid = _rd.ops[i].acceptid;
			pos[i].proposeid = _rd.ops[i].proposeid;
			pos[i].reqid = rrs.sig->id;
		}
	}
	
	if(_rd.coordinatorid != -1){
		DynamicPointer<foundation::Signal>		sigptr(ps);
		//reply to coordinator
		foundation::ipc::Service::the().sendSignal(sigptr, Parameters::the().addrvec[_rd.coordinatorid]);
	}else{
		DynamicSharedPointer<Signal>	sharedsigptr(ps);
		//broadcast to replicas
		for(uint i(0); i < Parameters::the().addrvec.size(); ++i){
			if(i != Parameters::the().idx){
				DynamicPointer<foundation::Signal>		sigptr(sharedsigptr);
				foundation::ipc::Service::the().sendSignal(sigptr, Parameters::the().addrvec[i]);
			}
		}
	}
}
/*
	Scans all the requests for acceptpending requests, and pushes em onto d.reqq
*/
struct RequestStubAcceptCmp{
	const RequestStubVectorT &rreqvec;
	
	RequestStubAcceptCmp(const RequestStubVectorT &_rreqvec):rreqvec(_rreqvec){}
	
	bool operator()(const size_t &_ridx1, const size_t &_ridx2)const{
		return overflow_safe_less(rreqvec[_ridx1].acceptid, rreqvec[_ridx2].acceptid);
	}
};
void Object::doScanPendingRequests(RunData &_rd){
	if(d.acceptpendingcnt != 255){
		size_t	posarr[256];
		size_t	idx(0);
		size_t	cnt(d.acceptpendingcnt);
		for(auto it(d.reqvec.begin()); cnt && it != d.reqvec.end(); ++it){
			RequestStub	&rreq(*it);
			if(rreq.state == RequestStub::AcceptPendingState){
				--cnt;
				posarr[idx] = it - d.reqvec.begin();
				++idx;
			}
		}
		RequestStubAcceptCmp cmp(d.reqvec);
		std::sort(posarr, posarr + idx, cmp);
		
		uint32 crtacceptid(d.acceptid);
		for(size_t i(0); i < idx; ++i){
			RequestStub	&rreq(d.reqvec[posarr[i]]);
			++crtacceptid;
			if(crtacceptid == rreq.acceptid){
				if(!(rreq.evs & RequestStub::SignaledEvent)){
					rreq.evs |= RequestStub::SignaledEvent;
					d.reqq.push(posarr[i]);
				}
				rreq.state = RequestStub::AcceptState;
			}else{
				d.acceptpendingcnt = idx - i;
				break;
			}
		}
	}else{
		std::deque<size_t>	posvec;
		for(auto it(d.reqvec.begin()); it != d.reqvec.end(); ++it){
			RequestStub	&rreq(*it);
			if(rreq.state == RequestStub::AcceptPendingState){
				posvec.push_back(it - d.reqvec.begin());
			}
		}
		
		RequestStubAcceptCmp cmp(d.reqvec);
		std::sort(posvec.begin(), posvec.end(), cmp);
		
		uint32 crtacceptid(d.acceptid);
		for(auto it(posvec.begin()); it != posvec.end(); ++it){
			RequestStub	&rreq(d.reqvec[*it]);
			++crtacceptid;
			if(crtacceptid == rreq.acceptid){
				if(!(rreq.evs & RequestStub::SignaledEvent)){
					rreq.evs |= RequestStub::SignaledEvent;
					d.reqq.push(*it);
				}
				rreq.state = RequestStub::AcceptState;
			}else{
				size_t sz(it - posvec.begin());
				if(sz >= 255){
					d.acceptpendingcnt = 255;
				}else{
					d.acceptpendingcnt = static_cast<uint8>(sz);
				}
				break;
			}
		}
	}
	d.acceptpendingcnt = 0;
}
void Object::doAcceptRequest(RunData &_rd, size_t _pos){
	RequestStub &rreq(d.requestStub(_pos));
	cassert(rreq.flags & RequestStub::HaveRequestFlag);
	cassert(rreq.acceptid == d.acceptid + 1);
	++d.acceptid;
	this->doAccept(rreq.sig);
}
void Object::doEraseRequest(RunData &_rd, size_t _pos){
	
}
//========================================================
}//namespace consensus
