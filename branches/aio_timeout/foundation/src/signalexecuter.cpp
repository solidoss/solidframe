#include <deque>

#include "foundation/signalexecuter.hpp"
#include "foundation/signal.hpp"
#include "foundation/manager.hpp"

#include "system/cassert.hpp"
#include "system/mutex.hpp"
#include "system/debug.hpp"
#include "system/timespec.hpp"

#include "utility/stack.hpp"
#include "utility/queue.hpp"


namespace foundation{


struct SignalExecuter::Data{
	Data():pm(NULL), extsz(0), sz(0){}
	enum{
		Running, Stopping
	};
	void push(DynamicPointer<Signal> &_sig);
	void eraseToutPos(uint32 _pos);
	struct SigData{
		SigData(const DynamicPointer<Signal>& _rsig):sig(_rsig), toutidx(-1), uid(0),tout(0xffffffff){}
		SigData(){}
		DynamicPointer<Signal>	sig;
		int32					toutidx;
		uint32					uid;
		TimeSpec				tout;
	};
	typedef std::deque<int32>					TimeoutVectorTp;
	typedef std::deque<SigData >				SignalDequeTp;
	typedef std::deque<DynamicPointer<Signal> >	SignalExtDequeTp;
	typedef Queue<uint32>						ExecQueueTp;
	typedef Stack<uint32>						FreeStackTp;
	Mutex 				*pm;
	uint32				extsz;
	uint32				sz;
	SignalDequeTp		sdq;
	SignalExtDequeTp	sedq;
	FreeStackTp			fs;
	FreeStackTp			fs2;
	ExecQueueTp			sq;
	ExecQueueTp			eq;
	TimeoutVectorTp		toutv;
	TimeSpec			tout;
};

void SignalExecuter::Data::push(DynamicPointer<Signal> &_sig){
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

void SignalExecuter::Data::eraseToutPos(uint32 _pos){
	cassert(_pos < toutv.size());
	toutv[_pos] = toutv.back();
	sdq[toutv.back()].toutidx = _pos;
	toutv.pop_back();
}

SignalExecuter::SignalExecuter():d(*(new Data)){
	state(Data::Running);
	d.tout.set(0xffffffff);
}

SignalExecuter::~SignalExecuter(){
	delete &d;
}

int SignalExecuter::signal(DynamicPointer<Signal> &_sig){
	cassert(!d.pm->tryLock());
	
	if(this->state() != Data::Running){
		_sig.clear();
		return 0;//no reason to raise the pool thread!!
	}
	d.push(_sig);
	return Object::signal(S_SIG | S_RAISE);
}


/*
NOTE:
	- be carefull, some siganls may keep stream (filemanager streams) and because
	the signal executer is on the same level as filemanager, you must release/delete
	the signals before destructor (e.g. when received kill) else the filemanager will
	wait forever, because the the destructor of the signal executer will be called
	after all services/and managers have stopped - i.e. a mighty deadlock.
*/
int SignalExecuter::execute(ulong _evs, TimeSpec &_rtout){
	d.pm->lock();
	idbgx(Dbg::cs, "d.extsz = "<<d.extsz);
	if(d.extsz){
		for(Data::SignalExtDequeTp::const_iterator it(d.sedq.begin()); it != d.sedq.end(); ++it){
			d.sdq.push_back(Data::SigData(*it));
		}
		d.sz += d.sedq.size();
		d.sedq.clear();
		d.extsz = 0;
	}
	if(signaled()){
		ulong sm = grabSignalMask(0);
		idbgx(Dbg::cs, "signalmask "<<sm);
		if(sm & S_KILL){
			state(Data::Stopping);
			if(!d.sz){//no signal
				state(-1);
				d.pm->unlock();
				d.sdq.clear();
				removeFromManager();
				idbgx(Dbg::cs, "~SignalExecuter");
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
	d.pm->unlock();
	if(state() == Data::Running){
		idbgx(Dbg::cs, "d.eq.size = "<<d.eq.size());
		while(d.eq.size()){
			uint32 pos = d.eq.front();
			d.eq.pop();
			doExecute(pos, 0, _rtout);
		}
		if((_evs & TIMEOUT) && _rtout >= d.tout){
			TimeSpec tout(0xffffffff);
			idbgx(Dbg::cs, "1 tout.size = "<<d.toutv.size());
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
			idbgx(Dbg::cs, "2 tout.size = "<<d.toutv.size());
			d.tout = tout;
		}
	}else{
		for(Data::SignalDequeTp::iterator it(d.sdq.begin()); it != d.sdq.end(); ++it){
			if(it->sig.ptr()){
				//TODO: should you use clear?!
				delete it->sig.release();
			}
		}
		idbgx(Dbg::cs, "~SignalExecuter");
		removeFromManager();
		state(-1);
		d.sdq.clear();
		return BAD;
	}
	if(d.fs2.size()){
		d.pm->lock();
		d.sz -= d.fs2.size();
		while(d.fs2.size()){
			d.fs.push(d.fs2.top());
			d.fs2.pop();
		}
		d.pm->unlock();
	}
	_rtout = d.tout;
	return NOK;
}

void SignalExecuter::mutex(Mutex *_pmut){
	d.pm = _pmut;
}

int SignalExecuter::execute(){
	cassert(false);
}

void SignalExecuter::doExecute(uint _pos, uint32 _evs, const TimeSpec &_rtout){
	Data::SigData &rcp(d.sdq[_pos]);
	TimeSpec ts(_rtout);
	switch(rcp.sig->execute( _evs, *this, SignalUidTp(_pos, rcp.uid), ts)){
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
				idbgx(Dbg::cs, "tout idx = "<<rcp.toutidx);
				rcp.tout = ts;
				if(d.tout > ts){
					d.tout = ts;
				}
				if(rcp.toutidx < 0){
					d.toutv.push_back(_pos);
					rcp.toutidx = d.toutv.size() - 1;
				}
				idbgx(Dbg::cs, "tout idx = "<<rcp.toutidx<<" toutv.size = "<<d.toutv.size());
			}else{
				rcp.tout.set(0xffffffff);
				if(rcp.toutidx >= 0){
					d.eraseToutPos(rcp.toutidx);
					rcp.toutidx = -1;
				}
			}
			break;
		case LEAVE:
			++rcp.uid;
			rcp.sig.release();
			d.fs2.push(_pos);
			if(rcp.toutidx >= 0){
				d.eraseToutPos(rcp.toutidx);
				rcp.toutidx = -1;
			}
			break;
	}
}

void SignalExecuter::sendSignal(
	DynamicPointer<Signal> &_rsig,
	const RequestUidTp &_requid,
	const ObjectUidTp& _from,
	const ipc::ConnectorUid *_conid
){
	idbgx(Dbg::cs, "_requid.first = "<<_requid.first<<" _requid.second = "<<_requid.second<<" uid = "<<d.sdq[_requid.first].uid);
	if(_requid.first < d.sdq.size() && d.sdq[_requid.first].uid == _requid.second){
		idbgx(Dbg::cs, "");
		if(d.sdq[_requid.first].sig->receiveSignal(_rsig, _from, _conid) == OK){
			idbgx(Dbg::cs, "");
			d.eq.push(_requid.first);
		}
	}
}




}//namespace foundation



