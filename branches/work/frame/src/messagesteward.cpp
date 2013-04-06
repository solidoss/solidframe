#include <deque>

#include "frame/messagesteward.hpp"
#include "frame/message.hpp"
#include "frame/manager.hpp"

#include "system/cassert.hpp"
#include "system/mutex.hpp"
#include "system/debug.hpp"
#include "system/timespec.hpp"

#include "utility/stack.hpp"
#include "utility/queue.hpp"


namespace solid{
namespace frame{


struct MessageSteward::Data{
	Data():extsz(0), sz(0){}
	enum{
		Running, Stopping
	};
	void push(DynamicPointer<Message> &_sig);
	void eraseToutPos(uint32 _pos);
	struct SigData{
		SigData(const DynamicPointer<Message>& _rsig):sig(_rsig), toutidx(-1), uid(0),tout(0xffffffff){}
		SigData(){}
		DynamicPointer<Message>	sig;
		int32					toutidx;
		uint32					uid;
		TimeSpec				tout;
	};
	typedef std::deque<int32>						TimeoutVectorT;
	typedef std::deque<SigData >					SignalDequeT;
	typedef std::deque<DynamicPointer<Message> >	SignalExtDequeT;
	typedef Queue<uint32>							ExecQueueT;
	typedef Stack<uint32>							FreeStackT;
	int					state;
//	Mutex 				*pm;
	uint32				extsz;
	uint32				sz;
	SignalDequeT		sdq;
	SignalExtDequeT	sedq;
	FreeStackT			fs;
	FreeStackT			fs2;
	ExecQueueT			sq;
	ExecQueueT			eq;
	TimeoutVectorT		toutv;
	TimeSpec			tout;
};

void MessageSteward::Data::push(DynamicPointer<Message> &_sig){
	if(fs.size()){
		sdq[fs.top()].sig = _sig;
		sq.push(fs.top());
		fs.pop();
	}else{
		sedq.push_back(_sig);
		++extsz;
		sq.push(sdq.size() + extsz - 1);
	}
}

void MessageSteward::Data::eraseToutPos(uint32 _pos){
	cassert(_pos < toutv.size());
	toutv[_pos] = toutv.back();
	sdq[toutv.back()].toutidx = _pos;
	toutv.pop_back();
}

MessageSteward::MessageSteward():d(*(new Data)){
	d.state = Data::Running;
	d.tout.set(0xffffffff);
}

MessageSteward::~MessageSteward(){
	delete &d;
}

bool MessageSteward::notify(DynamicPointer<Message> &_sig){
	
	if(d.state != Data::Running){
		_sig.clear();
		return false;//no reason to raise the pool thread!!
	}
	d.push(_sig);
	return Object::notify(S_SIG | S_RAISE);
}


/*
NOTE:
	- be carefull, some siganls may keep stream (filemanager streams) and because
	the signal executer is on the same level as filemanager, you must release/delete
	the signals before destructor (e.g. when received kill) else the filemanager will
	wait forever, because the the destructor of the signal executer will be called
	after all services/and managers have stopped - i.e. a mighty deadlock.
*/
int MessageSteward::execute(ulong _evs, TimeSpec &_rtout){
	Mutex &rmtx = frame::Manager::specific().mutex(*this);
	rmtx.lock();
	vdbgx(Debug::fdt, "d.extsz = "<<d.extsz);
	if(d.extsz){
		for(Data::SignalExtDequeT::const_iterator it(d.sedq.begin()); it != d.sedq.end(); ++it){
			d.sdq.push_back(Data::SigData(*it));
		}
		d.sz += d.sedq.size();
		d.sedq.clear();
		d.extsz = 0;
	}
	if(notified()){
		ulong sm = grabSignalMask(0);
		vdbgx(Debug::fdt, "signalmask "<<sm);
		if(sm & S_KILL){
			d.state = Data::Stopping;
			if(!d.sz){//no signal
				d.state = -1;
				rmtx.unlock();
				d.sdq.clear();
				Manager::specific().unregisterObject(*this);
				vdbgx(Debug::fdt, "~MessageSteward");
				return BAD;
			}
		}
		if(sm & S_SIG){
			while(d.sq.size()){
				d.eq.push(d.sq.front());
				d.sq.pop();
			}
		}
	}
	rmtx.unlock();
	if(d.state == Data::Running){
		vdbgx(Debug::fdt, "d.eq.size = "<<d.eq.size());
		while(d.eq.size()){
			uint32 pos = d.eq.front();
			d.eq.pop();
			doExecute(pos, 0, _rtout);
		}
		if((_evs & TIMEOUT) && _rtout >= d.tout){
			TimeSpec tout(0xffffffff);
			vdbgx(Debug::fdt, "1 tout.size = "<<d.toutv.size());
			for(uint i = 0; i < d.toutv.size();){
				uint pos = d.toutv[i];
				Data::SigData &rcp(d.sdq[pos]);
				if(_rtout >= rcp.tout){
					d.eraseToutPos(i);
					rcp.toutidx = -1;
					doExecute(pos, TIMEOUT, _rtout);
				}else{
					if(rcp.tout < tout){
						tout = rcp.tout;
					}
					++i;
				}
			}
			vdbgx(Debug::fdt, "2 tout.size = "<<d.toutv.size());
			d.tout = tout;
		}
	}else{
		for(Data::SignalDequeT::iterator it(d.sdq.begin()); it != d.sdq.end(); ++it){
			if(it->sig.get()){
				//TODO: should you use clear?!
				delete it->sig.release();
			}
		}
		idbgx(Debug::fdt, "remove signal executer from manager");
		Manager::specific().unregisterObject(*this);
		d.state = -1;
		d.sdq.clear();
		return BAD;
	}
	if(d.fs2.size()){
		rmtx.lock();
		d.sz -= d.fs2.size();
		while(d.fs2.size()){
			d.fs.push(d.fs2.top());
			d.fs2.pop();
		}
		rmtx.unlock();
	}
	_rtout = d.tout;
	return NOK;
}

void MessageSteward::init(Mutex *_pmtx){
}

int MessageSteward::execute(){
	cassert(false);
}

void MessageSteward::doExecute(uint _pos, uint32 _evs, const TimeSpec &_rtout){
	Data::SigData &rcp(d.sdq[_pos]);
	TimeSpec ts(_rtout);
	int rv(rcp.sig->execute(rcp.sig, _evs, *this, MessageUidT(_pos, rcp.uid), ts));
	if(!rcp.sig){
		rv = BAD;
	}
	switch(rv){
		case BAD: 
			++rcp.uid;
			rcp.sig.clear();
			d.fs2.push(_pos);
			if(rcp.toutidx >= 0){
				d.eraseToutPos(rcp.toutidx);
				rcp.toutidx = -1;
			}
			break;
		case OK:
			d.eq.push(_pos);
			if(rcp.toutidx >= 0){
				d.eraseToutPos(rcp.toutidx);
				rcp.toutidx = -1;
			}
			break;
		case NOK:
			if(ts != _rtout){
				vdbgx(Debug::fdt, "tout idx = "<<rcp.toutidx);
				rcp.tout = ts;
				if(d.tout > ts){
					d.tout = ts;
				}
				if(rcp.toutidx < 0){
					d.toutv.push_back(_pos);
					rcp.toutidx = d.toutv.size() - 1;
				}
				vdbgx(Debug::fdt, "tout idx = "<<rcp.toutidx<<" toutv.size = "<<d.toutv.size());
			}else{
				rcp.tout.set(0xffffffff);
				if(rcp.toutidx >= 0){
					d.eraseToutPos(rcp.toutidx);
					rcp.toutidx = -1;
				}
			}
			break;
	}
}

void MessageSteward::sendMessage(
	DynamicPointer<Message> &_rsig,
	const RequestUidT &_requid,
	const ObjectUidT& _from,
	const ipc::ConnectionUid *_conid
){
	vdbgx(Debug::fdt, "_requid.first = "<<_requid.first<<" _requid.second = "<<_requid.second<<" uid = "<<d.sdq[_requid.first].uid);
	if(_requid.first < d.sdq.size() && d.sdq[_requid.first].uid == _requid.second){
		vdbgx(Debug::fdt, "");
		if(d.sdq[_requid.first].sig->receiveMessage(_rsig, _from, _conid) == OK){
			vdbgx(Debug::fdt, "");
			d.eq.push(_requid.first);
		}
	}
}




}//namespace frame
}//namespace solid



