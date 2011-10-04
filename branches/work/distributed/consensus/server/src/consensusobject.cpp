#include <deque>
#include <queue>
#include <vector>
#include <algorithm>

#include "system/common.hpp"
#include "system/exception.hpp"

#undef HAVE_CPP11

#ifdef HAVE_CPP11
#include <unordered_map>
#include <unordered_set>
#else
#include <map>
#include <set>
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

#include "distributed/consensus/consensusrequest.hpp"
#include "distributed/consensus/server/consensusobject.hpp"
#include "timerqueue.hpp"
#include "consensussignals.hpp"

namespace fdt=foundation;
using namespace std;

namespace distributed{
namespace consensus{
namespace server{

/*static*/ const Parameters& Parameters::the(Parameters *_p){
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
		WaitProposeConfirmState,
		WaitAcceptState,
		WaitAcceptConfirmState,
		AcceptWaitRequestState,
		AcceptState,
		AcceptPendingState,
		AcceptRunState,
		EraseState,
	};
	enum{
		SignaledEvent = 1,
		TimeoutEvent = 2,
		
	};
	enum{
		HaveRequestFlag = 1,
		HaveProposeFlag = 2,
		HaveAcceptFlag = 4,
	};
	RequestStub(
	):
		evs(0), flags(0), proposeid(0xffffffff/2), acceptid(-1),
		timerid(0), recvpropconf(0), recvpropdecl(0), st(InitState){}
	RequestStub(
		DynamicPointer<consensus::RequestSignal> &_rsig
	):
		sig(_rsig), evs(0), flags(0), proposeid(-1), acceptid(-1), timerid(0),
		recvpropconf(0), recvpropdecl(0), st(InitState){}
	
	bool hasRequest()const{
		return flags & HaveRequestFlag;
	}
	void reinit(){
		evs = 0;
		flags = 0;
		st = InitState;
		proposeid = -1;
		acceptid = -1;
		recvpropconf = 0;
	}
	void state(uint8 _st);
	uint8 state()const{
		return st;
	}
	bool isValidProposeState()const;
	bool isValidProposeConfirmState()const;
	bool isValidProposeDeclineState()const;
	bool isValidAcceptState()const;
	bool isValidFastAcceptState()const;
	bool isValidAcceptConfirmState()const;
	bool isValidAcceptDeclineState()const;
	
	DynamicPointer<consensus::RequestSignal>	sig;
	uint16										evs;
	uint16										flags;
	uint32										proposeid;
	uint32										acceptid;
	uint16										timerid;
	uint8										recvpropconf;//received accepts for propose
	uint8										recvpropdecl;//received accepts for propose
private:
	uint8										st;
};

void RequestStub::state(uint8 _st){
	//checking the safetyness of state change
	switch(_st){
		//case WaitProposeState:
		//case WaitProposeConfirmState:
		//case WaitAcceptConfirmState:
		//case AcceptWaitRequestState:
		case AcceptState:
			if(
				st == InitState ||
				st == WaitProposeState ||
				st == WaitAcceptConfirmState ||
				st == WaitAcceptState ||
				st == AcceptPendingState
			){
				break;
			}else{
				THROW_EXCEPTION_EX("Invalid state for request ", (int)st);
				break;
			}
		//case AcceptPendingState:
		//case EraseState:
		default:
			break;
	}
	st = _st;
}

inline bool RequestStub::isValidProposeState()const{
	switch(st){
		case InitState:
		case WaitProposeState:
		case WaitProposeConfirmState:
		case WaitAcceptConfirmState:
		case WaitAcceptState:
			return true;
		default:
			return false;
	}
}
inline bool RequestStub::isValidProposeConfirmState()const{
	switch(st){
		case InitState:
		case WaitProposeState:
			return false;
		case WaitProposeConfirmState:
			return true;
		case WaitAcceptConfirmState:
		case WaitAcceptState:
		default:
			return false;
	}
}
inline bool RequestStub::isValidProposeDeclineState()const{
	return isValidProposeConfirmState();
}
inline bool RequestStub::isValidAcceptState()const{
	switch(st){
		case WaitAcceptState:
			return true;
		case AcceptWaitRequestState:
		case AcceptState:
		case AcceptPendingState:
		default:
			return false;
	}
}
inline bool RequestStub::isValidFastAcceptState()const{
	switch(st){
		case InitState:
		case WaitProposeState:
		case WaitProposeConfirmState:
			return true;
		case WaitAcceptConfirmState:
		case WaitAcceptState:
		default:
			return false;
	}
}
inline bool RequestStub::isValidAcceptConfirmState()const{
	switch(st){
		case InitState:
		case WaitProposeState:
		case WaitProposeConfirmState:
			return false;
		case WaitAcceptConfirmState:
			return true;
		case WaitAcceptState:
		default:
			return false;
	}
}
inline bool RequestStub::isValidAcceptDeclineState()const{
	return isValidAcceptConfirmState();
}



typedef std::deque<RequestStub>		RequestStubVectorT;

struct Object::Data{
	enum{
		ProposeOperation = 1,
		ProposeConfirmOperation,
		ProposeDeclineOperation,
		AcceptOperation,
		FastAcceptOperation,
		AcceptConfirmOperation,
		AcceptDeclineOperation,
		
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
	void eraseRequestStub(const size_t _idx);
	RequestStub& requestStub(const size_t _idx);
	RequestStub& safeRequestStub(const consensus::RequestId &_reqid, size_t &_rreqidx);
	RequestStub* requestStubPointer(const consensus::RequestId &_reqid, size_t &_rreqidx);
	const RequestStub& requestStub(const size_t _idx)const;
	bool checkAlreadyReceived(DynamicPointer<RequestSignal> &_rsig);
	bool isCoordinator()const;
	bool canSendFastAccept()const;
	void coordinatorId(int8 _coordid);
	
	
//data:	
	DynamicExecuterT		exe;
	uint32					proposeid;
		
	uint32					acceptid;
	uint32					proposedacceptid;
	uint32					confirmedacceptid;
	
	uint32					continuousacceptedproposes;
	size_t					pendingacceptwaitidx;
	
	int8					coordinatorid;
	int8					distancefromcoordinator;
	uint8					acceptpendingcnt;//the number of stubs in waitrequest state
	bool					isnotjuststarted;
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

/*
NOTE:
	the 0 value for proposeid is only valid for the beginning of the algorithm.
	A replica should only accept a proposeid==0 if its proposeid is 0.
	On overflow, the proposeid should skip the 0 value.
*/
Object::Data::Data():
	proposeid(0), acceptid(0), proposedacceptid(0), confirmedacceptid(0),
	continuousacceptedproposes(0), pendingacceptwaitidx(-1),
	coordinatorid(-2), distancefromcoordinator(-1), acceptpendingcnt(0), isnotjuststarted(false)
{
	if(Parameters::the().idx){
		coordinatorId(0);
	}else{
		coordinatorId(-1);
	}
}
Object::Data::~Data(){
	
}
void Object::Data::coordinatorId(int8 _coordid){
	coordinatorid = _coordid;
	if(_coordid != (int8)-1){
		int8 maxsz(Parameters::the().addrvec.size());
		distancefromcoordinator = circular_distance(static_cast<int8>(Parameters::the().idx), coordinatorid, maxsz);
		idbg("non-coordinator with distance from coordinator: "<<(int)distancefromcoordinator);
	}else{
		distancefromcoordinator = -1;
		idbg("coordinator");
	}
}
bool Object::Data::insertRequestStub(DynamicPointer<RequestSignal> &_rsig, size_t &_ridx){
	RequestStubMapT::const_iterator	it(reqmap.find(&_rsig->id));
	if(it != reqmap.end()){
		_ridx = it->second;
		reqmap.erase(it);
		RequestStub &rreq(requestStub(_ridx));
		if(rreq.flags & RequestStub::HaveRequestFlag){
			edbg("conflict "<<_rsig->id<<" against existing "<<rreq.sig->id<<" idx = "<<_ridx);
		}
		cassert(!(rreq.flags & RequestStub::HaveRequestFlag));
		rreq.sig = _rsig;
		reqmap[&rreq.sig->id] = _ridx;
		return false;
	}
	if(freeposstk.size()){
		_ridx = freeposstk.top();
		freeposstk.pop();
		RequestStub &rreq(requestStub(_ridx));
		rreq.reinit();
		rreq.sig = _rsig;
	}else{
		_ridx = reqvec.size();
		reqvec.push_back(RequestStub(_rsig));
	}
	reqmap[&requestStub(_ridx).sig->id] = _ridx;
	return true;
}
void Object::Data::eraseRequestStub(const size_t _idx){
	RequestStub &rreq(requestStub(_idx));
	//cassert(rreq.sig.ptr());
	if(rreq.sig.ptr()){
		reqmap.erase(&rreq.sig->id);
	}
	rreq.sig.clear();
	rreq.state(RequestStub::InitState);
	++rreq.timerid;
	freeposstk.push(_idx);
}
inline RequestStub& Object::Data::requestStub(const size_t _idx){
	cassert(_idx < reqvec.size());
	return reqvec[_idx];
}
inline const RequestStub& Object::Data::requestStub(const size_t _idx)const{
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
bool Object::Data::canSendFastAccept()const{
	TimeSpec ct(fdt::Object::currentTime());
	ct -= lastaccepttime;
	idbg("continuousacceptedproposes = "<<continuousacceptedproposes<<" ct.sec = "<<ct.seconds());
	return (continuousacceptedproposes >= 5) && ct.seconds() < 60;
}

struct DummyRequestSignal: public RequestSignal{
	DummyRequestSignal(const consensus::RequestId &_reqid):RequestSignal(_reqid){}
	/*virtual*/ void sendThisToConsensusObject(){}
};

RequestStub& Object::Data::safeRequestStub(const consensus::RequestId &_reqid, size_t &_rreqidx){
	RequestStubMapT::const_iterator	it(reqmap.find(&_reqid));
	if(it != reqmap.end()){
		_rreqidx = it->second;
	}else{
		DynamicPointer<RequestSignal> reqsig(new DummyRequestSignal(_reqid));
		insertRequestStub(reqsig, _rreqidx);
	}
	return requestStub(_rreqidx);
}
RequestStub* Object::Data::requestStubPointer(const consensus::RequestId &_reqid, size_t &_rreqidx){
	RequestStubMapT::const_iterator	it(reqmap.find(&_reqid));
	if(it != reqmap.end()){
		_rreqidx = it->second;
		return &requestStub(it->second);
	}else{
		return NULL;
	}
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
	):signals(_sig), rtimepos(_rts), coordinatorid(_coordinatorid), opcnt(0), crtacceptincrement(1){}
	
	bool isOperationsTableFull()const{
		return opcnt == OperationCapacity;
	}
	bool isCoordinator()const{
		return coordinatorid == -1;
	}
	
	ulong			signals;
	TimeSpec		&rtimepos;
	int8			coordinatorid;
	size_t			opcnt;
	size_t			crtacceptincrement;
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
	idbg((void*)this);
}
//---------------------------------------------------------
Object::~Object(){
	delete &d;
	idbg((void*)this);
}
//---------------------------------------------------------
bool Object::isRecoveryState()const{
	return state() >= PrepareRecoveryState && state() <= LastRecoveryState;
}
//---------------------------------------------------------
void Object::dynamicExecute(DynamicPointer<> &_dp, RunData &_rrd){
	
}
//---------------------------------------------------------
void Object::dynamicExecute(DynamicPointer<RequestSignal> &_rsig, RunData &_rrd){
	if(d.checkAlreadyReceived(_rsig)) return;
	size_t	idx;
	bool	alreadyexisted(!d.insertRequestStub(_rsig, idx));
	RequestStub &rreq(d.requestStub(idx));
	
	idbg("adding new request "<<rreq.sig->id<<" on idx = "<<idx<<" existing = "<<alreadyexisted);
	rreq.flags |= RequestStub::HaveRequestFlag;
	
	if(!(rreq.evs & RequestStub::SignaledEvent)){
		rreq.evs |= RequestStub::SignaledEvent;
		d.reqq.push(idx);
	}
}
void Object::dynamicExecute(DynamicPointer<OperationSignal<1> > &_rsig, RunData &_rrd){
	idbg("opcount = 1");
	doExecuteOperation(_rrd, _rsig->replicaidx, _rsig->op);
}
void Object::dynamicExecute(DynamicPointer<OperationSignal<2> > &_rsig, RunData &_rrd){
	idbg("opcount = 2");
	doExecuteOperation(_rrd, _rsig->replicaidx, _rsig->op[0]);
	doExecuteOperation(_rrd, _rsig->replicaidx, _rsig->op[1]);
}
void Object::dynamicExecute(DynamicPointer<OperationSignal<4> > &_rsig, RunData &_rrd){
	idbg("opcount = "<<_rsig->opsz);
	for(size_t i(0); i < _rsig->opsz; ++i){
		doExecuteOperation(_rrd, _rsig->replicaidx, _rsig->op[i]);
	}
}
void Object::dynamicExecute(DynamicPointer<OperationSignal<8> > &_rsig, RunData &_rrd){
	idbg("opcount = "<<_rsig->opsz);
	for(size_t i(0); i < _rsig->opsz; ++i){
		doExecuteOperation(_rrd, _rsig->replicaidx, _rsig->op[i]);
	}
}
void Object::dynamicExecute(DynamicPointer<OperationSignal<16> > &_rsig, RunData &_rrd){
	idbg("opcount = "<<_rsig->opsz);
	for(size_t i(0); i < _rsig->opsz; ++i){
		doExecuteOperation(_rrd, _rsig->replicaidx, _rsig->op[i]);
	}
}
void Object::dynamicExecute(DynamicPointer<OperationSignal<32> > &_rsig, RunData &_rrd){
	idbg("opcount = "<<_rsig->opsz);
	for(size_t i(0); i < _rsig->opsz; ++i){
		doExecuteOperation(_rrd, _rsig->replicaidx, _rsig->op[i]);
	}
}
void Object::doExecuteOperation(RunData &_rd, const uint8 _replicaidx, OperationStub &_rop){
	switch(_rop.operation){
		case Data::ProposeOperation:
			doExecuteProposeOperation(_rd, _replicaidx, _rop);
			break;
		case Data::ProposeConfirmOperation:
			doExecuteProposeConfirmOperation(_rd, _replicaidx, _rop);
			break;
		case Data::ProposeDeclineOperation:
			doExecuteProposeDeclineOperation(_rd, _replicaidx, _rop);
			break;
		case Data::AcceptOperation:
			doExecuteAcceptOperation(_rd, _replicaidx, _rop);
			break;
		case Data::FastAcceptOperation:
			doExecuteFastAcceptOperation(_rd, _replicaidx, _rop);
			break;
		case Data::AcceptConfirmOperation:
			doExecuteAcceptConfirmOperation(_rd, _replicaidx, _rop);
			break;
		case Data::AcceptDeclineOperation:
			doExecuteAcceptDeclineOperation(_rd, _replicaidx, _rop);
			break;
		default:
			THROW_EXCEPTION_EX("Unknown operation ", (int)_rop.operation);
	}
	
}

//on replica
void Object::doExecuteProposeOperation(RunData &_rd, const uint8 _replicaidx, OperationStub &_rop){
	idbg("op = ("<<(int)_rop.operation<<' '<<_rop.proposeid<<' '<<'('<<_rop.reqid<<')'<<" crtproposeid = "<<d.proposeid);
	if(!_rop.proposeid && d.proposeid/* && !isRecoveryState()*/){
		idbg("");
		doSendDeclinePropose(_rd, _replicaidx, _rop);
		return;
	}
	if(overflow_safe_less(_rop.proposeid, d.proposeid)/* && !isRecoveryState()*/){
		idbg("");
		//we cannot accept the propose
		doSendDeclinePropose(_rd, _replicaidx, _rop);
		return;
	}
	size_t	reqidx;
	//we can accept the propose
	d.proposeid = _rop.proposeid;
	d.isnotjuststarted = true;
	
	RequestStub &rreq(d.safeRequestStub(_rop.reqid, reqidx));
	
	if(!rreq.isValidProposeState() && !isRecoveryState()){
		wdbg("Invalid state "<<rreq.state()<<" for reqidx "<<reqidx);
		return;
	}
	
	rreq.proposeid = d.proposeid;
	rreq.flags |= RequestStub::HaveProposeFlag;
	if(!(rreq.flags & RequestStub::HaveAcceptFlag)){
		++d.proposedacceptid;
		rreq.acceptid = d.proposedacceptid;
	}
	rreq.state(RequestStub::WaitAcceptState);
	++rreq.timerid;
	TimeSpec ts(fdt::Object::currentTime());
	ts += 10 * 1000;//ms
	d.timerq.push(ts, reqidx, rreq.timerid);
	
	doSendConfirmPropose(_rd, _replicaidx, reqidx);
}

//on coordinator
void Object::doExecuteProposeConfirmOperation(RunData &_rd, const uint8 _replicaidx, OperationStub &_rop){
	idbg("op = ("<<(int)_rop.operation<<' '<<_rop.proposeid<<' '<<'('<<_rop.reqid<<')');
	if(isRecoveryState()){
		idbg("Recovery state!!!");
		return;
	}
	size_t reqidx;
	RequestStub *preq(d.requestStubPointer(_rop.reqid, reqidx));
	if(!preq){
		idbg("no such request");
		return;
	}
	if(preq->proposeid != _rop.proposeid){
		idbg("req->proposeid("<<preq->proposeid<<") != _rop.proposeid("<<_rop.proposeid<<")");
		return;
	}
	if(!preq->isValidProposeConfirmState()){
		wdbg("Invalid state "<<(int)preq->state()<<" for reqidx "<<reqidx);
		return;
	}
	cassert(preq->flags & RequestStub::HaveAcceptFlag);
	cassert(preq->flags & RequestStub::HaveProposeFlag);
	
	const uint32 tmpaccid = overflow_safe_max(preq->acceptid, _rop.acceptid);
	
// 	if(_rop.acceptid != tmpaccid){
// 		idbg("ignore a propose confirm operation from an outdated replica "<<tmpaccid<<" > "<<_rop.acceptid);
// 		return;
// 	}
	
	preq->acceptid = tmpaccid;
	
	++preq->recvpropconf;
	
	if(preq->recvpropconf == Parameters::the().quorum){
		++d.continuousacceptedproposes;
		preq->state(RequestStub::WaitAcceptConfirmState);
		TimeSpec ts(fdt::Object::currentTime());
		ts += 60 * 1000;//ms
		++preq->timerid;
		d.timerq.push(ts, reqidx, preq->timerid);
		doSendAccept(_rd, reqidx);
	}
	
}

//on coordinator
void Object::doExecuteProposeDeclineOperation(RunData &_rd, const uint8 _replicaidx, OperationStub &_rop){
	idbg("op = ("<<(int)_rop.operation<<' '<<_rop.proposeid<<' '<<'('<<_rop.reqid<<')');
	if(isRecoveryState()){
		idbg("Recovery state!!!");
		return;
	}
	size_t reqidx;
	RequestStub *preq(d.requestStubPointer(_rop.reqid, reqidx));
	if(!preq){
		idbg("no such request");
		return;
	}
	
	if(!preq->isValidProposeDeclineState()){
		wdbg("Invalid state "<<(int)preq->state()<<" for reqidx "<<reqidx);
		return;
	}
	if(
		preq->proposeid == 0 && 
		preq->proposeid != _rop.proposeid &&
		d.acceptid != _rop.acceptid
	){
		idbg("req->proposeid("<<preq->proposeid<<") != _rop.proposeid("<<_rop.proposeid<<")");
		doEnterRecoveryState();
		return;
	}
	++preq->recvpropdecl;
	if(preq->recvpropdecl == Parameters::the().quorum){
		++preq->timerid;
		preq->state(RequestStub::InitState);
		d.continuousacceptedproposes = 0;
		if(!(preq->evs & RequestStub::SignaledEvent)){
			preq->evs |= RequestStub::SignaledEvent;
			d.reqq.push(reqidx);
		}
	}
	return;
}

//on replica
void Object::doExecuteAcceptOperation(RunData &_rd, const uint8 _replicaidx, OperationStub &_rop){
	idbg("op = ("<<(int)_rop.operation<<' '<<_rop.proposeid<<' '<<'('<<_rop.reqid<<')');
	size_t reqidx;
	RequestStub *preq(d.requestStubPointer(_rop.reqid, reqidx));
	if(!preq){
		idbg("no such request");
		if(!isRecoveryState()){
			doSendDeclineAccept(_rd, _replicaidx, _rop);
		}
		return;
	}
	if(preq->proposeid != _rop.proposeid && !isRecoveryState()){
		idbg("req->proposeid("<<preq->proposeid<<") != _rop.proposeid("<<_rop.proposeid<<")");
		doSendDeclineAccept(_rd, _replicaidx, _rop);
		return;
	}
	
	if(!preq->isValidAcceptState() && !isRecoveryState()){
		wdbg("Invalid state "<<(int)preq->state()<<" for reqidx "<<reqidx);
		return;
	}
	
	preq->acceptid = _rop.acceptid;
	preq->flags |= RequestStub::HaveAcceptFlag;
	
	preq->state(RequestStub::AcceptState);
	
	if(!(preq->evs & RequestStub::SignaledEvent)){
		preq->evs |= RequestStub::SignaledEvent;
		d.reqq.push(reqidx);
	}
	
	doSendConfirmAccept(_rd, _replicaidx, reqidx);
}

//on replica
void Object::doExecuteFastAcceptOperation(RunData &_rd, const uint8 _replicaidx, OperationStub &_rop){
	idbg("op = ("<<(int)_rop.operation<<' '<<_rop.proposeid<<' '<<'('<<_rop.reqid<<')');
	if(!isRecoveryState() && !overflow_safe_less(d.proposeid, _rop.proposeid)){
		//we cannot accept the propose
		doSendDeclineAccept(_rd, _replicaidx, _rop);
		return;
	}
	size_t	reqidx;
	//we can accept the propose
	d.proposeid = _rop.proposeid;
	
	RequestStub &rreq(d.safeRequestStub(_rop.reqid, reqidx));
	
	if(!rreq.isValidFastAcceptState() && !isRecoveryState()){
		wdbg("Invalid state "<<(int)rreq.state()<<" for reqidx "<<reqidx);
		return;
	}
	
	rreq.proposeid = _rop.proposeid;
	rreq.acceptid = _rop.acceptid;
	d.proposedacceptid = overflow_safe_max(d.proposedacceptid, _rop.acceptid);
	rreq.flags |= (RequestStub::HaveProposeFlag | RequestStub::HaveAcceptFlag);
	
	rreq.state(RequestStub::AcceptState);
	
	if(!(rreq.evs & RequestStub::SignaledEvent)){
		rreq.evs |= RequestStub::SignaledEvent;
		d.reqq.push(reqidx);
	}
	doSendConfirmAccept(_rd, _replicaidx, reqidx);
}

//on coordinator
void Object::doExecuteAcceptConfirmOperation(RunData &_rd, const uint8 _replicaidx, OperationStub &_rop){
	idbg("op = ("<<(int)_rop.operation<<' '<<_rop.proposeid<<' '<<'('<<_rop.reqid<<')');
	if(isRecoveryState()){
		idbg("Recovery state!!!");
		return;
	}
	size_t reqidx;
	RequestStub *preq(d.requestStubPointer(_rop.reqid, reqidx));
	if(!preq){
		idbg("no such request");
		return;
	}
	if(preq->proposeid != _rop.proposeid || preq->acceptid != _rop.acceptid){
		idbg("req->proposeid("<<preq->proposeid<<") != _rop.proposeid("<<_rop.proposeid<<")");
		return;
	}
	if(preq->acceptid != _rop.acceptid){
		idbg("req->acceptid("<<preq->acceptid<<") != _rop.acceptid("<<_rop.acceptid<<")");
		return;
	}
	if(!preq->isValidAcceptConfirmState()){
		wdbg("Invalid state "<<(int)preq->state()<<" for reqidx "<<reqidx);
		return;
	}
	
	preq->state(RequestStub::AcceptState);
	
	if(!(preq->evs & RequestStub::SignaledEvent)){
		preq->evs |= RequestStub::SignaledEvent;
		d.reqq.push(reqidx);
	}
}
//on coordinator
void Object::doExecuteAcceptDeclineOperation(RunData &_rd, const uint8 _replicaidx, OperationStub &_rop){
	idbg("op = ("<<(int)_rop.operation<<' '<<_rop.proposeid<<' '<<'('<<_rop.reqid<<')');
	if(isRecoveryState()){
		idbg("Recovery state!!!");
		return;
	}
	size_t reqidx;
	RequestStub *preq(d.requestStubPointer(_rop.reqid, reqidx));
	if(!preq){
		idbg("no such request");
		return;
	}
	if(preq->proposeid != _rop.proposeid || preq->acceptid != _rop.acceptid){
		idbg("req->proposeid("<<preq->proposeid<<") != _rop.proposeid("<<_rop.proposeid<<")");
		return;
	}
	if(preq->acceptid != _rop.acceptid){
		idbg("req->acceptid("<<preq->acceptid<<") != _rop.acceptid("<<_rop.acceptid<<")");
		return;
	}
	if(!preq->isValidAcceptConfirmState()){
		wdbg("Invalid state "<<preq->state()<<" for reqidx "<<reqidx);
		return;
	}
	//do nothing for now
	return;
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
/*virtual*/ void Object::init(){
}
//---------------------------------------------------------
/*virtual*/ void Object::prepareRun(){
}
//---------------------------------------------------------
/*virtual*/ void Object::prepareRecovery(){
}
//---------------------------------------------------------
int Object::execute(ulong _sig, TimeSpec &_tout){
	foundation::Manager &rm(fdt::m());
	
	RunData	rd(_sig, _tout, d.coordinatorid);
	
	if(signaled()){//we've received a signal
		ulong sm(0);
		if(state() != InitState && state() != PrepareRunState && state() != PrepareRecoveryState){
			Mutex::Locker	lock(rm.mutex(*this));
			sm = grabSignalMask(0);//grab all bits of the signal mask
			if(sm & fdt::S_KILL) return BAD;
			if(sm & fdt::S_SIG){//we have signals
				d.exe.prepareExecute();
			}
		}
		if(sm & fdt::S_SIG){//we've grabed signals, execute them
			while(d.exe.hasCurrent()){
				d.exe.executeCurrent(*this, rd);
				d.exe.next();
			}
		}
	}

	switch(state()){
		case InitState:				return doInit(rd);
		case PrepareRunState:		return doPrepareRun(rd);
		case RunState:				return doRun(rd);
		case PrepareRecoveryState:	return doPrepareRecovery(rd);
		default:
			if(state() >= FirstRecoveryState && state() <= LastRecoveryState){
				return doRecovery(rd);
			}
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
	this->init();
	state(PrepareRunState);
	return OK;
}
//---------------------------------------------------------
int Object::doPrepareRun(RunData &_rd){
	this->prepareRun();
	state(RunState);
	return OK;
}
//---------------------------------------------------------
int Object::doRun(RunData &_rd){
	idbg("");
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
	
	//next we process all requests
	size_t cnt(d.reqq.size());
	while(cnt--){
		size_t pos(d.reqq.front());
		d.reqq.pop();
		doProcessRequest(_rd, pos);
	}
	
	doFlushOperations(_rd);
	
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
int Object::doPrepareRecovery(RunData &_rd){
	//erase all requests in erase state
	for(RequestStubVectorT::const_iterator it(d.reqvec.begin()); it != d.reqvec.end(); ++it){
		const RequestStub &rreq(*it);
		if(rreq.state() == RequestStub::EraseState){
			d.eraseRequestStub(it - d.reqvec.begin());
		}
	}
	prepareRecovery();
	state(FirstRecoveryState);
	return OK;
}
//---------------------------------------------------------
int Object::doRecovery(RunData &_rd){
	idbg("");
	return recovery();
}
//---------------------------------------------------------
void Object::doProcessRequest(RunData &_rd, const size_t _reqidx){
	RequestStub &rreq(d.requestStub(_reqidx));
	uint32 events = rreq.evs;
	rreq.evs = 0;
	switch(rreq.state()){
		case RequestStub::InitState://any
			if(d.isCoordinator()){
				if(d.canSendFastAccept()){
					idbg("InitState coordinator - send fast accept for "<<rreq.sig->id);
					doSendFastAccept(_rd, _reqidx);
					rreq.state(RequestStub::WaitAcceptConfirmState);
				}else{
					idbg("InitState coordinator - send propose for "<<rreq.sig->id);
					doSendPropose(_rd, _reqidx);
					rreq.state(RequestStub::WaitProposeConfirmState);
					TimeSpec ts(fdt::Object::currentTime());
					ts += 60 * 1000;//ms
					d.timerq.push(ts, _reqidx, rreq.timerid);
					rreq.recvpropconf = 1;//one is the propose_accept from current coordinator
				}
			}else{
				idbg("InitState non-coordinator - wait propose/accept for "<<rreq.sig->id)
				rreq.state(RequestStub::WaitProposeState);
				TimeSpec ts(fdt::Object::currentTime());
				ts += d.distancefromcoordinator * 1000;//ms
				d.timerq.push(ts, _reqidx, rreq.timerid);
			}
			break;
		case RequestStub::WaitProposeState://on replica
			idbg("WaitProposeState for "<<rreq.sig->id);
			if(events & RequestStub::TimeoutEvent){
				idbg("WaitProposeState - timeout for "<<rreq.sig->id);
				doSendPropose(_rd, _reqidx);
				rreq.state(RequestStub::WaitProposeConfirmState);
				TimeSpec ts(fdt::Object::currentTime());
				ts += 60 * 1000;//ms
				d.timerq.push(ts, _reqidx, rreq.timerid);
				rreq.recvpropconf = 1;//one is the propose_accept from current coordinator
				break;
			}
			break;
		case RequestStub::WaitProposeConfirmState://on coordinator
			idbg("WaitProposeAcceptState for "<<rreq.sig->id);
			if(events & RequestStub::TimeoutEvent){
				idbg("WaitProposeAcceptState - timeout: erase request "<<rreq.sig->id);
				doEraseRequest(_rd, _reqidx);
				break;
			}
			break;
		case RequestStub::WaitAcceptState://on replica
			if(events & RequestStub::TimeoutEvent){
				idbg("WaitAcceptState - timeout "<<rreq.sig->id);
				rreq.state(RequestStub::WaitProposeState);
				TimeSpec ts(fdt::Object::currentTime());
				ts += d.distancefromcoordinator * 1000;//ms
				++rreq.timerid;
				d.timerq.push(ts, _reqidx, rreq.timerid);
				break;
			}
			break;
		case RequestStub::WaitAcceptConfirmState://on coordinator
			idbg("WaitProposeAcceptConfirmState "<<rreq.sig->id);
			if(events & RequestStub::TimeoutEvent){
				idbg("WaitProposeAcceptConfirmState - timeout: erase request "<<rreq.sig->id);
				doEraseRequest(_rd, _reqidx);
				break;
			}
			break;
		case RequestStub::AcceptWaitRequestState://any
			idbg("AcceptWaitRequestState "<<rreq.sig->id);
			if(events & RequestStub::TimeoutEvent){
				idbg("AcceptWaitRequestState - timeout: enter recovery state "<<rreq.sig->id);
				doEnterRecoveryState();
				break;
			}
			if(!(rreq.flags & RequestStub::HaveRequestFlag)){
				break;
			}
		case RequestStub::AcceptState://any
			idbg("AcceptState "<<_reqidx<<" "<<rreq.sig->id<<" rreq.acceptid = "<<rreq.acceptid<<" d.acceptid = "<<d.acceptid<<" acceptpendingcnt = "<<(int)d.acceptpendingcnt);
			++rreq.timerid;
			//for recovery reasons, we do this check before
			//haverequestflag check
			if(overflow_safe_less(rreq.acceptid, d.acceptid + 1)){
				doEraseRequest(_rd, _reqidx);
				break;
			}
			if(!(rreq.flags & RequestStub::HaveRequestFlag)){
				idbg("norequestflag "<<_reqidx<<" acceptpendingcnt "<<(int)d.acceptpendingcnt);
				rreq.state(RequestStub::AcceptWaitRequestState);
				TimeSpec ts(fdt::Object::currentTime());
				ts.add(60);//60 secs
				d.timerq.push(ts, _reqidx, rreq.timerid);
				if(d.acceptpendingcnt < 255){
					++d.acceptpendingcnt;
				}
				break;
			}
			
			if(d.acceptid + _rd.crtacceptincrement != rreq.acceptid){
				idbg("enterpending "<<_reqidx);
				rreq.state(RequestStub::AcceptPendingState);
				
				if(d.acceptpendingcnt < 255){
					++d.acceptpendingcnt;
				}
				if(d.acceptpendingcnt == 1){
					cassert(d.pendingacceptwaitidx == -1);
					d.pendingacceptwaitidx = _reqidx;
					TimeSpec ts(fdt::Object::currentTime());
					ts.add(2*60);//2 mins
					d.timerq.push(ts, _reqidx, rreq.timerid);
					if(d.acceptpendingcnt < 255){
						++d.acceptpendingcnt;
					}
				}
				break;
			}
			d.lastaccepttime = fdt::Object::currentTime();
			++_rd.crtacceptincrement;
			//we cannot do erase or Accept here, we must wait for the
			//send operations to be flushed away
			rreq.state(RequestStub::AcceptRunState);
			
			if(!(rreq.evs & RequestStub::SignaledEvent)){
				rreq.evs |= RequestStub::SignaledEvent;
				d.reqq.push(_reqidx);
			}
			
			break;
		case RequestStub::AcceptPendingState:
			idbg("AcceptPendingState "<<rreq.sig->id);
			if(events & RequestStub::TimeoutEvent){
				idbg("AcceptPendingState - timeout: enter recovery state "<<rreq.sig->id);
				doEnterRecoveryState();
				break;
			}
			//the status is changed within doScanPendingRequests
			break;
		case RequestStub::AcceptRunState:
			doAcceptRequest(_rd, _reqidx);
			if(d.acceptpendingcnt){
				if(d.pendingacceptwaitidx == _reqidx){
					d.pendingacceptwaitidx = -1;
				}
				doScanPendingRequests(_rd);
			}
		case RequestStub::EraseState:
			doEraseRequest(_rd, _reqidx);
			break;
		default:
			THROW_EXCEPTION_EX("Unknown state ",rreq.state());
	}
}

void Object::doSendAccept(RunData &_rd, const size_t _reqidx){
	if(isRecoveryState()){
		idbg("recovery state: no sends");
		return;
	}
	if(!_rd.isCoordinator()){
		idbg("");
		doFlushOperations(_rd);
		_rd.coordinatorid = -1;
	}
	
	d.coordinatorId(-1);
	
	RequestStub &rreq(d.requestStub(_reqidx));
	
	idbg(""<<_reqidx<<" rreq.proposeid = "<<rreq.proposeid<<" rreq.acceptid = "<<rreq.acceptid<<" "<<rreq.sig->id);
	
	_rd.ops[_rd.opcnt].operation = Data::AcceptOperation;
	_rd.ops[_rd.opcnt].acceptid = rreq.acceptid;
	_rd.ops[_rd.opcnt].proposeid = rreq.proposeid;
	_rd.ops[_rd.opcnt].reqidx = _reqidx;
	
	++_rd.opcnt;
	
	if(_rd.isOperationsTableFull()){
		idbg("");
		doFlushOperations(_rd);
	}
}

void Object::doSendFastAccept(RunData &_rd, const size_t _reqidx){
	if(isRecoveryState()){
		idbg("recovery state: no sends");
		return;
	}
	if(!_rd.isCoordinator()){
		doFlushOperations(_rd);
		_rd.coordinatorid = -1;
	}
	RequestStub &rreq(d.requestStub(_reqidx));
	
	
	
	++d.proposeid;
	if(!d.proposeid) d.proposeid = 1;
	++d.proposedacceptid;
	
	rreq.acceptid = d.proposedacceptid;
	rreq.proposeid = d.proposeid;
	rreq.flags |= (RequestStub::HaveProposeFlag | RequestStub::HaveAcceptFlag);
	idbg(""<<_reqidx<<" rreq.proposeid = "<<rreq.proposeid<<" rreq.acceptid = "<<rreq.acceptid<<" "<<rreq.sig->id);
	
	_rd.ops[_rd.opcnt].operation = Data::FastAcceptOperation;
	_rd.ops[_rd.opcnt].acceptid = rreq.acceptid;
	_rd.ops[_rd.opcnt].proposeid = rreq.proposeid;
	_rd.ops[_rd.opcnt].reqidx = _reqidx;
	
	++_rd.opcnt;
	
	if(_rd.isOperationsTableFull()){
		doFlushOperations(_rd);
	}
}


void Object::doSendPropose(RunData &_rd, const size_t _reqidx){
	idbg(""<<_reqidx);
	if(isRecoveryState()){
		idbg("recovery state: no sends");
		return;
	}
	if(!_rd.isCoordinator()){
		doFlushOperations(_rd);
		_rd.coordinatorid = -1;
	}
	
	RequestStub &rreq(d.requestStub(_reqidx));
	
	if(d.isnotjuststarted){
		++d.proposeid;
		if(!d.proposeid) d.proposeid = 1;
	}else{
		d.isnotjuststarted = true;
	}
	++d.proposedacceptid;
	
	rreq.acceptid = d.proposedacceptid;
	rreq.proposeid = d.proposeid;
	rreq.flags |= (RequestStub::HaveProposeFlag | RequestStub::HaveAcceptFlag);
	idbg("sendpropose: proposeid = "<<rreq.proposeid<<" acceptid = "<<rreq.acceptid<<" "<<rreq.sig->id);
	
	_rd.ops[_rd.opcnt].operation = Data::ProposeOperation;
	_rd.ops[_rd.opcnt].acceptid = rreq.acceptid;
	_rd.ops[_rd.opcnt].proposeid = rreq.proposeid;
	_rd.ops[_rd.opcnt].reqidx = _reqidx;
	
	++_rd.opcnt;
	
	if(_rd.isOperationsTableFull()){
		doFlushOperations(_rd);
	}
}
void Object::doSendConfirmPropose(RunData &_rd, const uint8 _replicaidx, const size_t _reqidx){
	if(isRecoveryState()){
		idbg("recovery state: no sends");
		return;
	}
	if(_rd.coordinatorid != _replicaidx){
		doFlushOperations(_rd);
		_rd.coordinatorid = _replicaidx;
	}
	
	RequestStub &rreq(d.requestStub(_reqidx));
	idbg(""<<_reqidx<<" "<<(int)_replicaidx<<" "<<rreq.sig->id);
	
	
	//rreq.state = 
	_rd.ops[_rd.opcnt].operation = Data::ProposeConfirmOperation;
	_rd.ops[_rd.opcnt].acceptid = rreq.acceptid;
	_rd.ops[_rd.opcnt].proposeid = rreq.proposeid;
	_rd.ops[_rd.opcnt].reqidx = _reqidx;
	
	++_rd.opcnt;
	
	if(_rd.isOperationsTableFull()){
		doFlushOperations(_rd);
	}
}

void Object::doSendDeclinePropose(RunData &_rd, const uint8 _replicaidx, const OperationStub &_rop){
	idbg(""<<(int)_replicaidx<<" "<<_rop.reqid);
	if(isRecoveryState()){
		idbg("recovery state: no sends");
		return;
	}
	OperationSignal<1>	*po(new OperationSignal<1>);
	
	po->replicaidx = Parameters::the().idx;
	
	po->op.operation = Data::ProposeDeclineOperation;
	po->op.proposeid = d.proposeid;
	po->op.acceptid = d.acceptid;
	po->op.reqid = _rop.reqid;
	DynamicPointer<foundation::Signal>		sigptr(po);
	
	foundation::ipc::Service::the().sendSignal(sigptr, Parameters::the().addrvec[_replicaidx]);
	
}

void Object::doSendConfirmAccept(RunData &_rd, const uint8 _replicaidx, const size_t _reqidx){
	if(isRecoveryState()){
		idbg("recovery state: no sends");
		return;
	}
	if(_rd.coordinatorid != _replicaidx){
		doFlushOperations(_rd);
		_rd.coordinatorid = _replicaidx;
	}
	
	d.coordinatorId(_replicaidx);
	
	RequestStub &rreq(d.requestStub(_reqidx));
	
	idbg(""<<(int)_replicaidx<<" "<<_reqidx<<" "<<rreq.sig->id);
	
	_rd.ops[_rd.opcnt].operation = Data::AcceptConfirmOperation;
	_rd.ops[_rd.opcnt].acceptid = rreq.acceptid;
	_rd.ops[_rd.opcnt].proposeid = rreq.proposeid;
	_rd.ops[_rd.opcnt].reqidx = _reqidx;
	
	++_rd.opcnt;
	
	if(_rd.isOperationsTableFull()){
		doFlushOperations(_rd);
	}
}
void Object::doSendDeclineAccept(RunData &_rd, const uint8 _replicaidx, const OperationStub &_rop){
	idbg(""<<(int)_replicaidx<<" "<<_rop.reqid);
	if(isRecoveryState()){
		idbg("recovery state: no sends");
		return;
	}
	OperationSignal<1>	*po(new OperationSignal<1>);
	
	po->replicaidx = Parameters::the().idx;
	
	po->op.operation = Data::AcceptDeclineOperation;
	po->op.proposeid = d.proposeid;
	po->op.acceptid = d.acceptid;
	po->op.reqid = _rop.reqid;
	DynamicPointer<foundation::Signal>		sigptr(po);
	
	foundation::ipc::Service::the().sendSignal(sigptr, Parameters::the().addrvec[_replicaidx]);
}
void Object::doFlushOperations(RunData &_rd){
	idbg("");
	Signal *ps(NULL);
	OperationStub *pos(NULL);
	const size_t opcnt = _rd.opcnt;
	_rd.opcnt = 0;
	if(opcnt == 0){
		return;
	}else if(opcnt == 1){
		OperationSignal<1>	*po(new OperationSignal<1>);
		RequestStub	&rreq(d.requestStub(_rd.ops[0].reqidx));
		ps = po;
		
		po->replicaidx = Parameters::the().idx;
		
		po->op.operation = _rd.ops[0].operation;
		po->op.acceptid = _rd.ops[0].acceptid;
		po->op.proposeid = _rd.ops[0].proposeid;
		po->op.reqid = rreq.sig->id;
	}else if(opcnt == 2){
		OperationSignal<2>	*po(new OperationSignal<2>);
		ps = po;
		po->replicaidx = Parameters::the().idx;
		pos = po->op;
	}else if(opcnt <= 4){
		OperationSignal<4>	*po(new OperationSignal<4>);
		ps = po;
		po->replicaidx = Parameters::the().idx;
		po->opsz = opcnt;
		pos = po->op;
	}else if(opcnt <= 8){
		OperationSignal<8>	*po(new OperationSignal<8>);
		ps = po;
		po->replicaidx = Parameters::the().idx;
		po->opsz = opcnt;
		pos = po->op;
	}else if(opcnt <= 16){
		OperationSignal<16>	*po(new OperationSignal<16>);
		ps = po;
		po->replicaidx = Parameters::the().idx;
		po->opsz = opcnt;
		pos = po->op;
	}else if(opcnt <= 32){
		OperationSignal<32>	*po(new OperationSignal<32>);
		ps = po;
		po->replicaidx = Parameters::the().idx;
		po->opsz = opcnt;
		pos = po->op;
	}else{
		THROW_EXCEPTION_EX("invalid opcnt ",opcnt);
	}
	if(pos){
		for(size_t i(0); i < opcnt; ++i){
			RequestStub	&rreq(d.requestStub(_rd.ops[i].reqidx));
			//po->oparr.push_back(OperationStub());
			
			pos[i].operation = _rd.ops[i].operation;
			pos[i].acceptid = _rd.ops[i].acceptid;
			pos[i].proposeid = _rd.ops[i].proposeid;
			pos[i].reqid = rreq.sig->id;
			idbg("pos["<<i<<"].operation = "<<(int)pos[i].operation<<" proposeid = "<<pos[i].proposeid);
		}
	}
	
	if(_rd.isCoordinator()){
		DynamicSharedPointer<Signal>	sharedsigptr(ps);
		idbg("broadcast to other replicas");
		//broadcast to replicas
		for(uint i(0); i < Parameters::the().addrvec.size(); ++i){
			if(i != Parameters::the().idx){
				DynamicPointer<foundation::Signal>		sigptr(sharedsigptr);
				foundation::ipc::Service::the().sendSignal(sigptr, Parameters::the().addrvec[i]);
			}
		}
	}else{
		idbg("send to "<<(int)_rd.coordinatorid);
		DynamicPointer<foundation::Signal>		sigptr(ps);
		//reply to coordinator
		foundation::ipc::Service::the().sendSignal(sigptr, Parameters::the().addrvec[_rd.coordinatorid]);
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
	idbg(""<<(int)d.acceptpendingcnt);
	const size_t accpendcnt = d.acceptpendingcnt;
	d.acceptpendingcnt = 0;
	size_t	cnt(0);
	if(accpendcnt != 255){
		size_t	posarr[256];
		size_t	idx(0);
		
		
		for(RequestStubVectorT::const_iterator it(d.reqvec.begin()); it != d.reqvec.end(); ++it){
			const RequestStub	&rreq(*it);
			if(
				rreq.state() == RequestStub::AcceptPendingState
			){
				posarr[idx] = it - d.reqvec.begin();
				++idx;
			}else if(rreq.state() == RequestStub::AcceptWaitRequestState){
				++cnt;
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
				rreq.state(RequestStub::AcceptState);
				if(d.pendingacceptwaitidx == posarr[i]){
					d.pendingacceptwaitidx = -1;
				}
			}else{
				const size_t	tmp = cnt + idx - i;
				d.acceptpendingcnt = tmp <= 255 ? tmp : 255;
				if(tmp != cnt && d.pendingacceptwaitidx == -1){
					//we have at least one pending request wich is not in waitrequest state
					RequestStub	&rreq(d.requestStub(posarr[i]));
					TimeSpec	ts(fdt::Object::currentTime());
					ts.add(2*60);//2 mins
					d.timerq.push(ts, posarr[i], rreq.timerid);
					d.pendingacceptwaitidx = posarr[i];
				}
				break;
			}
		}
		idbg("d.acceptpendingcnt = "<<(int)d.acceptpendingcnt<<" d.pendingacceptwaitidx = "<<d.pendingacceptwaitidx);
	}else{
		std::deque<size_t>	posvec;
		for(RequestStubVectorT::const_iterator it(d.reqvec.begin()); it != d.reqvec.end(); ++it){
			const RequestStub	&rreq(*it);
			if(rreq.state() == RequestStub::AcceptPendingState){
				posvec.push_back(it - d.reqvec.begin());
			}else if(rreq.state() == RequestStub::AcceptWaitRequestState){
				++cnt;
			}
		}
		
		RequestStubAcceptCmp cmp(d.reqvec);
		std::sort(posvec.begin(), posvec.end(), cmp);
		
		uint32 crtacceptid(d.acceptid);
		
		for(std::deque<size_t>::const_iterator it(posvec.begin()); it != posvec.end(); ++it){
			RequestStub	&rreq(d.reqvec[*it]);
			++crtacceptid;
			if(crtacceptid == rreq.acceptid){
				if(!(rreq.evs & RequestStub::SignaledEvent)){
					rreq.evs |= RequestStub::SignaledEvent;
					d.reqq.push(*it);
				}
				if(d.pendingacceptwaitidx == *it){
					d.pendingacceptwaitidx = -1;
				}
				rreq.state(RequestStub::AcceptState);
			}else{
				size_t sz(it - posvec.begin());
				size_t tmp = cnt + posvec.size() - sz;
				d.acceptpendingcnt = tmp <= 255 ? tmp : 255;;
				if(tmp != cnt && d.pendingacceptwaitidx == -1){
					//we have at least one pending request wich is not in waitrequest state
					RequestStub	&rreq(d.requestStub(*it));
					TimeSpec	ts(fdt::Object::currentTime());
					ts.add(2*60);//2 mins
					d.timerq.push(ts, *it, rreq.timerid);
					d.pendingacceptwaitidx = *it;
				}
				break;
			}
		}
	}
}
void Object::doAcceptRequest(RunData &_rd, const size_t _reqidx){
	idbg(""<<_reqidx);
	RequestStub &rreq(d.requestStub(_reqidx));
	cassert(rreq.flags & RequestStub::HaveRequestFlag);
	cassert(rreq.acceptid == d.acceptid + 1);
	++d.acceptid;
	
	d.reqmap.erase(&rreq.sig->id);
	this->accept(rreq.sig);
	rreq.sig.clear();
	rreq.flags &= (~RequestStub::HaveRequestFlag);
}
void Object::doEraseRequest(RunData &_rd, const size_t _reqidx){
	idbg(""<<_reqidx);
	if(d.pendingacceptwaitidx == _reqidx){
		d.pendingacceptwaitidx = -1;
	}
	d.eraseRequestStub(_reqidx);
}
void Object::doStartCoordinate(RunData &_rd, const size_t _reqidx){
	idbg(""<<_reqidx);
}
void Object::doEnterRecoveryState(){
	idbg("ENTER RECOVERY STATE");
	state(PrepareRecoveryState);
}
void Object::enterRunState(){
	idbg("");
	state(PrepareRunState);
}
//========================================================

}//namespace server
}//namespace consensus
}//namespace distributed