#include "dchatstressconnection.hpp"
#include "system/mutex.hpp"
#include "system/debug.hpp"
#include "frame/manager.hpp"
#include "example/dchat/core/messages.hpp"
#include "example/dchat/core/compressor.hpp"


using namespace solid;
using namespace std;

//-----------------------------------------------------------------------
size_t Connection::send(DynamicPointer<solid::frame::Message>	&_rmsgptr, const uint32 _flags){
	Locker<Mutex>	lock(rm.mutex(*this));
	size_t idx;
	
	if(freesndidxstk.size()){
		idx = freesndidxstk.top();
		freesndidxstk.pop();
	}else{
		idx = sndidx;
		++sndidx;
	}
	
	sndmsgvec.push_back(MessageStub(_rmsgptr, _flags, idx));
	
	if(Object::notify(frame::S_SIG | frame::S_RAISE)){
		rm.raise(*this);
	}
	return idx;
}
//-----------------------------------------------------------------------
int Connection::done(){
	idbg("");
	return BAD;
}
//-----------------------------------------------------------------------
void Connection::onReceiveNoop(){
	cassert(waitnoop);
	waitnoop = false;
}
//-----------------------------------------------------------------------
void Connection::onDoneIndex(uint32 _msgidx){
	Locker<Mutex>	lock(rm.mutex(*this));
	freesndidxstk.push(_msgidx);
}

//-----------------------------------------------------------------------
/*virtual*/ int Connection::execute(ulong _evs, TimeSpec& _crtime){
	typedef DynamicSharedPointer<solid::frame::Message>		MessageSharedPointerT;
	
	static Compressor 				compressor(BufferControllerT::DataCapacity);
	static MessageSharedPointerT	noopmsgptr(new NoopMessage);
	
	ulong							sm = grabSignalMask();
	if(sm){
		if(sm & frame::S_KILL) return done();
		if(sm & frame::S_SIG){
			Locker<Mutex>	lock(rm.mutex(*this));
			for(MessageVectorT::iterator it(sndmsgvec.begin()); it != sndmsgvec.end(); ++it){
				session.send(it->idx, it->msgptr, it->flags);
			}
			sndmsgvec.clear();
		}
	}
	
	if(_evs & (frame::TIMEOUT | frame::ERRDONE)){
		if(waitnoop){
			return done();
		}else if(session.isSendQueueEmpty()){
			DynamicPointer<solid::frame::Message>	msgptr(noopmsgptr);
			send(msgptr);
			waitnoop = true;
		}
	}
	
	ConnectionContext				ctx(*this);
	
	bool reenter = false;
	if(st == RunningState){
		int rv = session.execute(*this, _evs, ctx, ser, des, bufctl, compressor);
		if(rv == BAD) return done();
		if(rv == NOK){
			if(waitnoop){
				_crtime.add(3);//wait 3 seconds
			}else{
				_crtime.add(15);//wait 15 secs then send noop
			}
		}
		return rv;
	}else if(st == PrepareState){
		SocketDevice	sd;
		sd.create(rd.begin());
		sd.makeNonBlocking();
		socketInsert(sd);
		socketRequestRegister();
		st = ConnectState;
		reenter = false;
	}else if(st == ConnectState){
		idbg("ConnectState");
		switch(socketConnect(rd.begin())){
			case BAD: return done();
			case OK:
				idbg("");
				st = InitState;
				reenter = true;
				break;
			case NOK:
				st = ConnectWaitState;
				idbg("");
				break;
		}
	}else if(st == ConnectWaitState){
		idbg("ConnectWaitState");
		st = InitState;
		reenter = true;
	}else if(st == InitState){
		idbg("InitState");
		st = RunningState;
		reenter = true;
	}
	return reenter ? OK : NOK;
}

