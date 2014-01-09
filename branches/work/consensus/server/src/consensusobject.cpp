// consensus/server/src/consensusobject.cpp
//
// Copyright (c) 2011, 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <deque>
#include <queue>
#include <vector>
#include <algorithm>

#include "system/common.hpp"
#include "system/exception.hpp"

#ifdef HAS_CPP11
#include <unordered_map>
#include <unordered_set>
#else
#include <map>
#include <set>
#endif

#include "system/timespec.hpp"

#include "serialization/binary.hpp"

#include "frame/object.hpp"
#include "frame/manager.hpp"
#include "frame/message.hpp"
#include "frame/ipc/ipcservice.hpp"

#include "utility/stack.hpp"
#include "utility/queue.hpp"
#include "utility/timerqueue.hpp"

#include "consensus/consensusregistrar.hpp"
#include "consensus/consensusrequest.hpp"
#include "consensus/server/consensusobject.hpp"
#include "consensusmessages.hpp"

using namespace std;

namespace solid{
namespace consensus{
namespace server{

struct TimerValue{
	TimerValue(
		uint32 _idx = 0,
		uint16 _uid = 0,
		uint16 _val = 0
	):index(_idx), uid(_uid), value(_val){}
	uint32 index;
	uint16 uid;
	uint16 value;
};

typedef TimerQueue<TimerValue>							TimerQueueT;
typedef DynamicPointer<consensus::WriteRequestMessage>	WriteRequestMessagePointerT;

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
		DynamicPointer<consensus::WriteRequestMessage> &_rmsgptr
	):
		msgptr(_rmsgptr), evs(0), flags(0), proposeid(-1), acceptid(-1), timerid(0),
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
	
	WriteRequestMessagePointerT		msgptr;
	uint16							evs;
	uint16							flags;
	uint32							proposeid;
	uint32							acceptid;
	uint16							timerid;
	uint8							recvpropconf;//received accepts for propose
	uint8							recvpropdecl;//received accepts for propose
private:
	uint8							st;
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



typedef std::deque<RequestStub>			RequestStubVectorT;
typedef DynamicPointer<Configuration> 	ConfigurationPointerT;

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
	
#ifdef HAS_CPP11
	typedef std::unordered_map<const consensus::RequestId*, size_t, ReqHash, ReqCmpEqual>		RequestStubMapT;
	typedef std::unordered_set<consensus::RequestId, SenderHash, SenderCmpEqual>				SenderSetT;
#else
	typedef std::map<const consensus::RequestId*, size_t, ReqCmpLess>							RequestStubMapT;
	typedef std::set<consensus::RequestId, SenderCmpLess>										SenderSetT;
#endif
	typedef Stack<size_t>																		SizeTStackT;
	typedef Queue<size_t>																		SizeTQueueT;
	typedef std::vector<DynamicPointer<> >														DynamicPointerVectorT;

//methods:
	Data(DynamicPointer<Configuration> &_rcfgptr);
	~Data();
	
	bool insertRequestStub(DynamicPointer<WriteRequestMessage> &_rmsgptr, size_t &_ridx);
	void eraseRequestStub(const size_t _idx);
	RequestStub& requestStub(const size_t _idx);
	RequestStub& safeRequestStub(const consensus::RequestId &_reqid, size_t &_rreqidx);
	RequestStub* requestStubPointer(const consensus::RequestId &_reqid, size_t &_rreqidx);
	const RequestStub& requestStub(const size_t _idx)const;
	bool checkAlreadyReceived(DynamicPointer<WriteRequestMessage> &_rmsgptr);
	bool isCoordinator()const;
	bool canSendFastAccept()const;
	void coordinatorId(int8 _coordid);
	
	
//data:	
	ConfigurationPointerT 	cfgptr;
	DynamicPointerVectorT	dv;
	uint32					proposeid;
	frame::IndexT			srvidx;
		
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
	TimerQueueT				timerq;
	int						state;
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
Object::Data::Data(DynamicPointer<Configuration> &_rcfgptr):
	cfgptr(_rcfgptr),
	proposeid(0), srvidx(INVALID_INDEX), acceptid(0), proposedacceptid(0), confirmedacceptid(0),
	continuousacceptedproposes(0), pendingacceptwaitidx(-1),
	coordinatorid(-2), distancefromcoordinator(-1), acceptpendingcnt(0), isnotjuststarted(false)
{
	if(cfgptr->crtidx){
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
		int8 maxsz(cfgptr->addrvec.size());
		distancefromcoordinator = circular_distance(static_cast<int8>(cfgptr->crtidx), coordinatorid, maxsz);
		idbg("non-coordinator with distance from coordinator: "<<(int)distancefromcoordinator);
	}else{
		distancefromcoordinator = -1;
		idbg("coordinator");
	}
}
bool Object::Data::insertRequestStub(DynamicPointer<WriteRequestMessage> &_rmsgptr, size_t &_ridx){
	RequestStubMapT::iterator	it(reqmap.find(&_rmsgptr->consensusRequestId()));
	if(it != reqmap.end()){
		_ridx = it->second;
		reqmap.erase(it);
		RequestStub &rreq(requestStub(_ridx));
		if(rreq.flags & RequestStub::HaveRequestFlag){
			edbg("conflict "<<_rmsgptr->consensusRequestId()<<" against existing "<<rreq.msgptr->consensusRequestId()<<" idx = "<<_ridx);
		}
		cassert(!(rreq.flags & RequestStub::HaveRequestFlag));
		rreq.msgptr = _rmsgptr;
		reqmap[&rreq.msgptr->consensusRequestId()] = _ridx;
		return false;
	}
	if(freeposstk.size()){
		_ridx = freeposstk.top();
		freeposstk.pop();
		RequestStub &rreq(requestStub(_ridx));
		rreq.reinit();
		rreq.msgptr = _rmsgptr;
	}else{
		_ridx = reqvec.size();
		reqvec.push_back(RequestStub(_rmsgptr));
	}
	reqmap[&requestStub(_ridx).msgptr->consensusRequestId()] = _ridx;
	return true;
}
void Object::Data::eraseRequestStub(const size_t _idx){
	RequestStub &rreq(requestStub(_idx));
	//cassert(rreq.sig.get());
	if(rreq.msgptr.get()){
		reqmap.erase(&rreq.msgptr->consensusRequestId());
	}
	rreq.msgptr.clear();
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
bool Object::Data::checkAlreadyReceived(DynamicPointer<WriteRequestMessage> &_rmsgptr){
	SenderSetT::iterator it(senderset.find(_rmsgptr->consensusRequestId()));
	if(it != senderset.end()){
		if(overflow_safe_less(_rmsgptr->consensusRequestId().requid, it->requid)){
			return true;
		}
		senderset.erase(it);
		senderset.insert(_rmsgptr->consensusRequestId());
	}else{
		senderset.insert(_rmsgptr->consensusRequestId());
	}
	return false;
}
bool Object::Data::canSendFastAccept()const{
	TimeSpec ct(frame::Object::currentTime());
	ct -= lastaccepttime;
	idbg("continuousacceptedproposes = "<<continuousacceptedproposes<<" ct.sec = "<<ct.seconds());
	return (continuousacceptedproposes >= 5) && ct.seconds() < 60;
}

struct DummyWriteRequestMessage: public WriteRequestMessage{
	DummyWriteRequestMessage(const consensus::RequestId &_reqid):WriteRequestMessage(_reqid){}
	/*virtual*/ void consensusNotifyServerWithThis(){}
	/*virtual*/ void consensusNotifyClientWithThis(){}
	/*virtual*/ void consensusNotifyClientWithFail(){}
};

RequestStub& Object::Data::safeRequestStub(const consensus::RequestId &_reqid, size_t &_rreqidx){
	RequestStubMapT::const_iterator	it(reqmap.find(&_reqid));
	if(it != reqmap.end()){
		_rreqidx = it->second;
	}else{
		DynamicPointer<WriteRequestMessage> reqsig(new DummyWriteRequestMessage(_reqid));
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
		TimeSpec const &_rts,
		int8 _coordinatorid
	):signals(_sig), rtimepos(_rts), coordinatorid(_coordinatorid), opcnt(0), crtacceptincrement(1){}
	
	bool isOperationsTableFull()const{
		return opcnt == OperationCapacity;
	}
	bool isCoordinator()const{
		return coordinatorid == -1;
	}
	
	ulong			signals;
	TimeSpec const 	&rtimepos;
	int8			coordinatorid;
	size_t			opcnt;
	size_t			crtacceptincrement;
	OperationStub	ops[OperationCapacity];
};
//========================================================
Object::DynamicMapperT Object::dm;

/*static*/ void Object::dynamicRegister(){
	dm.insert<WriteRequestMessage, Object>();
	dm.insert<ReadRequestMessage, Object>();
	dm.insert<OperationMessage<1>, Object>();
	dm.insert<OperationMessage<2>, Object>();
	dm.insert<OperationMessage<4>, Object>();
	dm.insert<OperationMessage<8>, Object>();
	dm.insert<OperationMessage<16>, Object>();
	dm.insert<OperationMessage<32>, Object>();
	//TODO: add here the other consensus Messages
}
/*static*/ void Object::registerMessages(frame::ipc::Service &_ripcsvc){
	_ripcsvc.registerMessageType<OperationMessage<1> >();
	_ripcsvc.registerMessageType<OperationMessage<2> >();
	_ripcsvc.registerMessageType<OperationMessage<4> >();
	_ripcsvc.registerMessageType<OperationMessage<8> >();
	_ripcsvc.registerMessageType<OperationMessage<16> >();
	_ripcsvc.registerMessageType<OperationMessage<32> >();
}
Object::Object(DynamicPointer<Configuration> &_rcfgptr):d(*(new Data(_rcfgptr))){
	idbg((void*)this);
}
//---------------------------------------------------------
Object::~Object(){
	Registrar::the().unregisterObject(d.srvidx);
	delete &d;
	idbg((void*)this);
}
//---------------------------------------------------------
void Object::state(int _st){
	d.state = _st;
}
//---------------------------------------------------------
int Object::state()const{
	return d.state;
}
//---------------------------------------------------------
void Object::serverIndex(const frame::IndexT &_ridx){
	d.srvidx = _ridx;
}
//---------------------------------------------------------
frame::IndexT Object::serverIndex()const{
	return d.srvidx;
}
//---------------------------------------------------------
bool Object::isRecoveryState()const{
	return state() >= PrepareRecoveryState && state() <= LastRecoveryState;
}
//---------------------------------------------------------
void Object::dynamicHandle(DynamicPointer<> &_dp, RunData &_rrd){
	
}
//---------------------------------------------------------
void Object::dynamicHandle(DynamicPointer<WriteRequestMessage> &_rmsgptr, RunData &_rrd){
	if(d.checkAlreadyReceived(_rmsgptr)) return;
	size_t			idx;
	bool			alreadyexisted(!d.insertRequestStub(_rmsgptr, idx));
	RequestStub		&rreq(d.requestStub(idx));
	
	idbg("adding new request "<<rreq.msgptr->consensusRequestId()<<" on idx = "<<idx<<" existing = "<<alreadyexisted);
	rreq.flags |= RequestStub::HaveRequestFlag;
	
	if(!(rreq.evs & RequestStub::SignaledEvent)){
		rreq.evs |= RequestStub::SignaledEvent;
		d.reqq.push(idx);
	}
}
void Object::dynamicHandle(DynamicPointer<ReadRequestMessage> &_rmsgptr, RunData &_rrd){
	
}
void Object::dynamicHandle(DynamicPointer<OperationMessage<1> > &_rmsgptr, RunData &_rrd){
	idbg("opcount = 1");
	doExecuteOperation(_rrd, _rmsgptr->replicaidx, _rmsgptr->op);
}
void Object::dynamicHandle(DynamicPointer<OperationMessage<2> > &_rmsgptr, RunData &_rrd){
	idbg("opcount = 2");
	doExecuteOperation(_rrd, _rmsgptr->replicaidx, _rmsgptr->op[0]);
	doExecuteOperation(_rrd, _rmsgptr->replicaidx, _rmsgptr->op[1]);
}
void Object::dynamicHandle(DynamicPointer<OperationMessage<4> > &_rmsgptr, RunData &_rrd){
	idbg("opcount = "<<_rmsgptr->opsz);
	for(size_t i(0); i < _rmsgptr->opsz; ++i){
		doExecuteOperation(_rrd, _rmsgptr->replicaidx, _rmsgptr->op[i]);
	}
}
void Object::dynamicHandle(DynamicPointer<OperationMessage<8> > &_rmsgptr, RunData &_rrd){
	idbg("opcount = "<<_rmsgptr->opsz);
	for(size_t i(0); i < _rmsgptr->opsz; ++i){
		doExecuteOperation(_rrd, _rmsgptr->replicaidx, _rmsgptr->op[i]);
	}
}
void Object::dynamicHandle(DynamicPointer<OperationMessage<16> > &_rmsgptr, RunData &_rrd){
	idbg("opcount = "<<_rmsgptr->opsz);
	for(size_t i(0); i < _rmsgptr->opsz; ++i){
		doExecuteOperation(_rrd, _rmsgptr->replicaidx, _rmsgptr->op[i]);
	}
}
void Object::dynamicHandle(DynamicPointer<OperationMessage<32> > &_rmsgptr, RunData &_rrd){
	idbg("opcount = "<<_rmsgptr->opsz);
	for(size_t i(0); i < _rmsgptr->opsz; ++i){
		doExecuteOperation(_rrd, _rmsgptr->replicaidx, _rmsgptr->op[i]);
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
	TimeSpec ts(frame::Object::currentTime());
	ts += 10 * 1000;//ms
	d.timerq.push(ts, TimerValue(reqidx, rreq.timerid));
	
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
	
	if(preq->recvpropconf == d.cfgptr->quorum){
		++d.continuousacceptedproposes;
		preq->state(RequestStub::WaitAcceptConfirmState);
		TimeSpec ts(frame::Object::currentTime());
		ts += 60 * 1000;//ms
		++preq->timerid;
		d.timerq.push(ts, TimerValue(reqidx, preq->timerid));
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
	if(preq->recvpropdecl == d.cfgptr->quorum){
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
/*virtual*/ bool Object::notify(DynamicPointer<frame::Message> &_rmsgptr){
	if(this->state() < 0){
		_rmsgptr.clear();
		return false;//no reason to raise the pool thread!!
	}
	DynamicPointer<>	dp(_rmsgptr);
	d.dv.push_back(dp);
	return frame::Object::notify(frame::S_SIG | frame::S_RAISE);
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
/*virtual*/ void Object::execute(ExecuteContext &_rexectx){
	frame::Manager &rm(frame::Manager::specific());
	
	RunData								rd(_rexectx.eventMask(), _rexectx.currentTime(), d.coordinatorid);
	if(notified()){//we've received a signal
		ulong							sm(0);
		DynamicHandler<DynamicMapperT>	dh(dm);
		if(state() != InitState && state() != PrepareRunState && state() != PrepareRecoveryState){
			Locker<Mutex>	lock(rm.mutex(*this));
			sm = grabSignalMask(0);//grab all bits of the signal mask
			if(sm & frame::S_KILL){
				_rexectx.close();
				return;
			}
			if(sm & frame::S_SIG){//we have signals
				dh.init(d.dv.begin(), d.dv.end());
				d.dv.clear();
			}
		}
		if(sm & frame::S_SIG){//we've grabed signals, execute them
			for(size_t i = 0; i < dh.size(); ++i){
				dh.handle(*this, i, rd);
			}
		}
	}
	
	AsyncE	rv;

	switch(state()){
		case InitState:				rv = doInit(rd); break;
		case PrepareRunState:		rv = doPrepareRun(rd); break;
		case RunState:				rv = doRun(rd); break;
		case PrepareRecoveryState:	rv = doPrepareRecovery(rd); break;
		default:
			if(state() >= FirstRecoveryState && state() <= LastRecoveryState){
				rv = doRecovery(rd);
			}
			break;
	}
	if(rv == AsyncSuccess){
		_rexectx.reschedule();
	}else if(rv == AsyncError){
		_rexectx.close();
	}else if(d.timerq.size()){
		if(d.timerq.isHit(_rexectx.currentTime())){
			_rexectx.reschedule();
			return;
		}
		_rexectx.waitUntil(d.timerq.frontTime());
	}
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
AsyncE Object::doInit(RunData &_rd){
	this->init();
	state(PrepareRunState);
	return AsyncSuccess;
}
//---------------------------------------------------------
AsyncE Object::doPrepareRun(RunData &_rd){
	this->prepareRun();
	state(RunState);
	return AsyncSuccess;
}
//---------------------------------------------------------
AsyncE Object::doRun(RunData &_rd){
	idbg("");
	//first we scan for timeout:
	while(d.timerq.isHit(_rd.rtimepos)){
		RequestStub &rreq(d.requestStub(d.timerq.frontValue().index));
		
		if(rreq.timerid == d.timerq.frontValue().uid){
			rreq.evs |= RequestStub::TimeoutEvent;
			if(!(rreq.evs & RequestStub::SignaledEvent)){
				rreq.evs |= RequestStub::SignaledEvent;
				d.reqq.push(d.timerq.frontValue().index);
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
	
	if(d.reqq.size()) return AsyncSuccess;
	
	return AsyncWait;
}
//---------------------------------------------------------
AsyncE Object::doPrepareRecovery(RunData &_rd){
	//erase all requests in erase state
	for(RequestStubVectorT::const_iterator it(d.reqvec.begin()); it != d.reqvec.end(); ++it){
		const RequestStub &rreq(*it);
		if(rreq.state() == RequestStub::EraseState){
			d.eraseRequestStub(it - d.reqvec.begin());
		}
	}
	prepareRecovery();
	state(FirstRecoveryState);
	return AsyncSuccess;
}
//---------------------------------------------------------
AsyncE Object::doRecovery(RunData &_rd){
	idbg("");
	return recovery();
}
//---------------------------------------------------------
void Object::doProcessRequest(RunData &_rd, const size_t _reqidx){
	RequestStub		&rreq(d.requestStub(_reqidx));
	uint32			events = rreq.evs;
	rreq.evs = 0;
	switch(rreq.state()){
		case RequestStub::InitState://any
			if(d.isCoordinator()){
				if(d.canSendFastAccept()){
					idbg("InitState coordinator - send fast accept for "<<rreq.msgptr->consensusRequestId());
					doSendFastAccept(_rd, _reqidx);
					rreq.state(RequestStub::WaitAcceptConfirmState);
				}else{
					idbg("InitState coordinator - send propose for "<<rreq.msgptr->consensusRequestId());
					doSendPropose(_rd, _reqidx);
					rreq.state(RequestStub::WaitProposeConfirmState);
					TimeSpec ts(frame::Object::currentTime());
					ts += 60 * 1000;//ms
					d.timerq.push(ts, TimerValue(_reqidx, rreq.timerid));
					rreq.recvpropconf = 1;//one is the propose_accept from current coordinator
				}
			}else{
				idbg("InitState non-coordinator - wait propose/accept for "<<rreq.msgptr->consensusRequestId())
				rreq.state(RequestStub::WaitProposeState);
				TimeSpec ts(frame::Object::currentTime());
				ts += d.distancefromcoordinator * 1000;//ms
				d.timerq.push(ts, TimerValue(_reqidx, rreq.timerid));
			}
			break;
		case RequestStub::WaitProposeState://on replica
			idbg("WaitProposeState for "<<rreq.msgptr->consensusRequestId());
			if(events & RequestStub::TimeoutEvent){
				idbg("WaitProposeState - timeout for "<<rreq.msgptr->consensusRequestId());
				doSendPropose(_rd, _reqidx);
				rreq.state(RequestStub::WaitProposeConfirmState);
				TimeSpec ts(frame::Object::currentTime());
				ts += 60 * 1000;//ms
				d.timerq.push(ts, TimerValue(_reqidx, rreq.timerid));
				rreq.recvpropconf = 1;//one is the propose_accept from current coordinator
				break;
			}
			break;
		case RequestStub::WaitProposeConfirmState://on coordinator
			idbg("WaitProposeAcceptState for "<<rreq.msgptr->consensusRequestId());
			if(events & RequestStub::TimeoutEvent){
				idbg("WaitProposeAcceptState - timeout: erase request "<<rreq.msgptr->consensusRequestId());
				doEraseRequest(_rd, _reqidx);
				break;
			}
			break;
		case RequestStub::WaitAcceptState://on replica
			if(events & RequestStub::TimeoutEvent){
				idbg("WaitAcceptState - timeout "<<rreq.msgptr->consensusRequestId());
				rreq.state(RequestStub::WaitProposeState);
				TimeSpec ts(frame::Object::currentTime());
				ts += d.distancefromcoordinator * 1000;//ms
				++rreq.timerid;
				d.timerq.push(ts, TimerValue(_reqidx, rreq.timerid));
				break;
			}
			break;
		case RequestStub::WaitAcceptConfirmState://on coordinator
			idbg("WaitProposeAcceptConfirmState "<<rreq.msgptr->consensusRequestId());
			if(events & RequestStub::TimeoutEvent){
				idbg("WaitProposeAcceptConfirmState - timeout: erase request "<<rreq.msgptr->consensusRequestId());
				doEraseRequest(_rd, _reqidx);
				break;
			}
			break;
		case RequestStub::AcceptWaitRequestState://any
			idbg("AcceptWaitRequestState "<<rreq.msgptr->consensusRequestId());
			if(events & RequestStub::TimeoutEvent){
				idbg("AcceptWaitRequestState - timeout: enter recovery state "<<rreq.msgptr->consensusRequestId());
				doEnterRecoveryState();
				break;
			}
			if(!(rreq.flags & RequestStub::HaveRequestFlag)){
				break;
			}
		case RequestStub::AcceptState://any
			idbg("AcceptState "<<_reqidx<<" "<<rreq.msgptr->consensusRequestId()<<" rreq.acceptid = "<<rreq.acceptid<<" d.acceptid = "<<d.acceptid<<" acceptpendingcnt = "<<(int)d.acceptpendingcnt);
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
				TimeSpec ts(frame::Object::currentTime());
				ts.add(60);//60 secs
				d.timerq.push(ts, TimerValue(_reqidx, rreq.timerid));
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
					TimeSpec ts(frame::Object::currentTime());
					ts.add(2*60);//2 mins
					d.timerq.push(ts, TimerValue(_reqidx, rreq.timerid));
					if(d.acceptpendingcnt < 255){
						++d.acceptpendingcnt;
					}
				}
				break;
			}
			d.lastaccepttime = frame::Object::currentTime();
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
			idbg("AcceptPendingState "<<rreq.msgptr->consensusRequestId());
			if(events & RequestStub::TimeoutEvent){
				idbg("AcceptPendingState - timeout: enter recovery state "<<rreq.msgptr->consensusRequestId());
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
	
	idbg(""<<_reqidx<<" rreq.proposeid = "<<rreq.proposeid<<" rreq.acceptid = "<<rreq.acceptid<<" "<<rreq.msgptr->consensusRequestId());
	
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
	idbg(""<<_reqidx<<" rreq.proposeid = "<<rreq.proposeid<<" rreq.acceptid = "<<rreq.acceptid<<" "<<rreq.msgptr->consensusRequestId());
	
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
	idbg("sendpropose: proposeid = "<<rreq.proposeid<<" acceptid = "<<rreq.acceptid<<" "<<rreq.msgptr->consensusRequestId());
	
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
	idbg(""<<_reqidx<<" "<<(int)_replicaidx<<" "<<rreq.msgptr->consensusRequestId());
	
	
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
	OperationMessage<1>	*po(new OperationMessage<1>);
	
	po->replicaidx = d.cfgptr->crtidx;
	po->srvidx = serverIndex();
	
	po->op.operation = Data::ProposeDeclineOperation;
	po->op.proposeid = d.proposeid;
	po->op.acceptid = d.acceptid;
	po->op.reqid = _rop.reqid;
	
	DynamicPointer<frame::ipc::Message>		msgptr(po);
	this->doSendMessage(msgptr, d.cfgptr->addrvec[_replicaidx]);
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
	
	idbg(""<<(int)_replicaidx<<" "<<_reqidx<<" "<<rreq.msgptr->consensusRequestId());
	
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
	OperationMessage<1>	*po(new OperationMessage<1>);
	
	po->replicaidx = d.cfgptr->crtidx;
	po->srvidx = serverIndex();
	
	po->op.operation = Data::AcceptDeclineOperation;
	po->op.proposeid = d.proposeid;
	po->op.acceptid = d.acceptid;
	po->op.reqid = _rop.reqid;
	
	DynamicPointer<frame::ipc::Message>		msgptr(po);
	
	this->doSendMessage(msgptr, d.cfgptr->addrvec[_replicaidx]);
}
void Object::doFlushOperations(RunData &_rd){
	idbg("");
	Message			*pm(NULL);
	OperationStub	*pos(NULL);
	const size_t	opcnt = _rd.opcnt;
	_rd.opcnt = 0;
	
	if(opcnt == 0){
		return;
	}else if(opcnt == 1){
		OperationMessage<1>		*po(new OperationMessage<1>);
		RequestStub				&rreq(d.requestStub(_rd.ops[0].reqidx));
		pm = po;
		
		po->replicaidx = d.cfgptr->crtidx;
		po->srvidx = serverIndex();
		
		po->op.operation = _rd.ops[0].operation;
		po->op.acceptid = _rd.ops[0].acceptid;
		po->op.proposeid = _rd.ops[0].proposeid;
		po->op.reqid = rreq.msgptr->consensusRequestId();
	}else if(opcnt == 2){
		OperationMessage<2>	*po(new OperationMessage<2>);
		pm = po;
		po->replicaidx = d.cfgptr->crtidx;
		po->srvidx = serverIndex();
		pos = po->op;
	}else if(opcnt <= 4){
		OperationMessage<4>	*po(new OperationMessage<4>);
		pm = po;
		po->replicaidx = d.cfgptr->crtidx;
		po->srvidx = serverIndex();
		po->opsz = opcnt;
		pos = po->op;
	}else if(opcnt <= 8){
		OperationMessage<8>	*po(new OperationMessage<8>);
		pm = po;
		po->replicaidx = d.cfgptr->crtidx;
		po->srvidx = serverIndex();
		po->opsz = opcnt;
		pos = po->op;
	}else if(opcnt <= 16){
		OperationMessage<16>	*po(new OperationMessage<16>);
		pm = po;
		po->replicaidx = d.cfgptr->crtidx;
		po->srvidx = serverIndex();
		po->opsz = opcnt;
		pos = po->op;
	}else if(opcnt <= 32){
		OperationMessage<32>	*po(new OperationMessage<32>);
		pm = po;
		po->replicaidx = d.cfgptr->crtidx;
		po->srvidx = serverIndex();
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
			pos[i].reqid = rreq.msgptr->consensusRequestId();
			idbg("pos["<<i<<"].operation = "<<(int)pos[i].operation<<" proposeid = "<<pos[i].proposeid);
		}
	}
	
	if(_rd.isCoordinator()){
		DynamicSharedPointer<Message>	sharedmsgptr(pm);
		idbg("broadcast to other replicas");
		//broadcast to replicas
		for(uint i(0); i < d.cfgptr->addrvec.size(); ++i){
			if(i != d.cfgptr->crtidx){
				DynamicPointer<frame::ipc::Message>		msgptr(sharedmsgptr);
				this->doSendMessage(msgptr, d.cfgptr->addrvec[i]);
			}
		}
	}else{
		idbg("send to "<<(int)_rd.coordinatorid);
		DynamicPointer<frame::ipc::Message>		msgptr(pm);
		this->doSendMessage(msgptr, d.cfgptr->addrvec[_rd.coordinatorid]);
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
	const size_t	accpendcnt = d.acceptpendingcnt;
	size_t			cnt(0);
	
	d.acceptpendingcnt = 0;
	
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
		
		RequestStubAcceptCmp	cmp(d.reqvec);
		
		std::sort(posarr, posarr + idx, cmp);
		
		uint32					crtacceptid(d.acceptid);
		
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
					TimeSpec	ts(frame::Object::currentTime());
					ts.add(2*60);//2 mins
					d.timerq.push(ts, TimerValue(posarr[i], rreq.timerid));
					d.pendingacceptwaitidx = posarr[i];
				}
				break;
			}
		}
		idbg("d.acceptpendingcnt = "<<(int)d.acceptpendingcnt<<" d.pendingacceptwaitidx = "<<d.pendingacceptwaitidx);
	}else{
		std::deque<size_t>		posvec;
		for(RequestStubVectorT::const_iterator it(d.reqvec.begin()); it != d.reqvec.end(); ++it){
			const RequestStub	&rreq(*it);
			if(rreq.state() == RequestStub::AcceptPendingState){
				posvec.push_back(it - d.reqvec.begin());
			}else if(rreq.state() == RequestStub::AcceptWaitRequestState){
				++cnt;
			}
		}
		
		RequestStubAcceptCmp	cmp(d.reqvec);
		
		std::sort(posvec.begin(), posvec.end(), cmp);
		
		uint32					crtacceptid(d.acceptid);
		
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
					TimeSpec	ts(frame::Object::currentTime());
					ts.add(2*60);//2 mins
					d.timerq.push(ts, TimerValue(*it, rreq.timerid));
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
	
	d.reqmap.erase(&rreq.msgptr->consensusRequestId());
	this->accept(rreq.msgptr);
	rreq.msgptr.clear();
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
