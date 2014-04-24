#include "dchatstressservice.hpp"
#include "dchatmessagematrix.hpp"

#include "example/dchat/core/messages.hpp"
#include "example/dchat/core/compressor.hpp"


#include "system/mutex.hpp"
#include "system/debug.hpp"
#include "system/thread.hpp"
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"

#include "utility/stack.hpp"
#include "utility/dynamictype.hpp"

#include "serialization/idtypemapper.hpp"
#include "serialization/binary.hpp"

#include "protocol/binary/binaryaiosession.hpp"
#include "protocol/binary/binarybasicbuffercontroller.hpp"

#include "frame/manager.hpp"
#include "frame/aio/aiosingleobject.hpp"


#include <iostream>
#include <sstream>


using namespace solid;
using namespace std;

//=======================================================================

class Connection;

struct SendJob: Dynamic<SendJob, DynamicShared<solid::frame::Message> >{
	typedef DynamicPointer<SendJob>				PointerT;
	typedef DynamicSharedPointer<SendJob>		SharedPointerT;
	size_t		msgrow;
	size_t		sleepms;
	size_t		count;
	SendJob(
		const size_t _msgrow = 0,
		const size_t _sleepms = 0,
		const size_t _count = 0
	):msgrow(_msgrow), sleepms(_sleepms), count(_count){}
};


struct ConnectionContext{
	ConnectionContext(Connection &_rcon):rcon(_rcon), sndmsgidx(-1),rcvmsgidx(-1){}
	void sendMessageIndex(const solid::uint32 _msgidx){
		sndmsgidx = _msgidx;
	}
	void recvMessageIndex(const solid::uint32 _msgidx){
		rcvmsgidx = _msgidx;
	}
	Connection			&rcon;
	solid::uint32		sndmsgidx;
	solid::uint32		rcvmsgidx;
};

//=======================================================================

typedef solid::serialization::binary::Serializer<ConnectionContext>		BinSerializerT;
typedef solid::serialization::binary::Deserializer<ConnectionContext>	BinDeserializerT;
typedef solid::serialization::IdTypeMapper<
	BinSerializerT, BinDeserializerT, solid::uint8,
	solid::serialization::FakeMutex
>																		UInt8TypeMapperT;

namespace {
	UInt8TypeMapperT				typemap;
}

//=======================================================================

struct Handle{
	void beforeSerialization(BinSerializerT &_rs, void *_pt, ConnectionContext &_rctx){}
	
	void beforeSerialization(BinDeserializerT &_rs, void *_pt, ConnectionContext &_rctx){}
	bool checkStore(void *, ConnectionContext &_rctx)const{
		return true;
	}
	
	bool checkLoad(void *_pm, ConnectionContext &_rctx)const{
		return true;
	}
	void afterSerialization(BinSerializerT &_rs, void *_pm, ConnectionContext &_rctx){}
	
	void afterSerialization(BinDeserializerT &_rs, BasicMessage *_pm, ConnectionContext &_rctx);
		
	void afterSerialization(BinDeserializerT &_rs, TextMessage *_pm, ConnectionContext &_rctx);
	void afterSerialization(BinDeserializerT &_rs, NoopMessage *_pm, ConnectionContext &_rctx);
};

//=======================================================================

class Connection: public solid::frame::aio::SingleObject{
	typedef solid::DynamicMapper<void, Connection>						DynamicMapperT;
	typedef solid::DynamicPointer<solid::frame::Message>				MessageDynamicPointerT;
	typedef std::vector<solid::DynamicPointer<> >						DynamicPointerVectorT;
	
	static DynamicMapperT		dm;
	struct MessageStub{
		MessageStub():flags(0), idx(-1){}
		MessageStub(
			MessageDynamicPointerT	&_rmsgptr,
			const solid::uint32 _flags = 0,
			const size_t _idx = -1
		):msgptr(_rmsgptr), flags(_flags), idx(_idx){}
		
		MessageDynamicPointerT	msgptr;
		solid::uint32			flags;
		size_t					idx;
	};
	struct ProtocolController: solid::protocol::binary::BasicController{
		void onDoneSend(ConnectionContext &_rctx, const size_t _msgidx){
			_rctx.rcon.onDoneSend(_msgidx);
		}
		void onRecv(ConnectionContext &_rctx, const size_t _sz){
			_rctx.rcon.service().onReceive(_sz);
		}
	};
	typedef std::vector<MessageStub>									MessageVectorT;
	typedef solid::protocol::binary::AioSession<
		solid::frame::Message,
		int,
		ProtocolController
	>																	ProtocolSessionT;
		
	typedef solid::protocol::binary::BasicBufferController<2048>		BufferControllerT;
	enum{
		CreateState = 1,
		InitState,
		PrepareState,
		ConnectState,
		ConnectWaitState,
		RunningState
	};
public:
	Connection(
		const size_t _idx,
		const SocketAddressInet &_rsa,
		const solid::serialization::TypeMapperBase &_rtm,
		const uint16 _reconnectcnt,
		const size_t _reconnectsleepms
	):	idx(_idx), rsa(_rsa), st(CreateState), connectcnt(_reconnectcnt + 1), ser(_rtm), des(_rtm),
		sndidx(1), waitnoop(false), reconnectsleepms(_reconnectsleepms){}
	
	
	/*virtual*/ bool notify(solid::DynamicPointer<solid::frame::Message> &_rmsgptr);
	
	Service& service()const{
		return static_cast<Service&>(frame::Manager::specific().service(*this));
	}
	void dynamicHandle(solid::DynamicPointer<> &_dp);
	void dynamicHandle(solid::DynamicPointer<SendJob> &_rmsgptr);
	void onReceiveMessage(){
		service().onReceive();
	}
	void onLoginDone(){
		service().onLogin();
	}
private:
	friend struct Handle;
	friend struct SessionController;
	/*virtual*/ void execute(ExecuteContext &_rexectx);
	void done();
	void onReceiveNoop();
	void onDoneSend(const size_t _msgidx);
private:
	typedef solid::Queue<SendJob::PointerT>		JobQueueT;
	
	const size_t			idx;
	const SocketAddressInet &rsa;
	solid::uint16			st;
	solid::uint16			connectcnt;
	BinSerializerT			ser;
	BinDeserializerT		des;
	ProtocolSessionT		session;
	MessageVectorT			sndmsgvec;
	BufferControllerT		bufctl;
	size_t					sndidx;
	bool					waitnoop;
	const size_t 			reconnectsleepms;
	DynamicPointerVectorT	msgvec;
	JobQueueT				jobq;
	solid::TimeSpec			nexttime;
	solid::TimeSpec			nooptime;
	size_t					sendidx;
};

//=======================================================================
/*static*/ void Service::registerMessages(){
	typemap.insert<LoginRequest>();
	typemap.insertHandle<BasicMessage, Handle>();
	typemap.insertHandle<TextMessage, Handle>();
	typemap.insertHandle<NoopMessage, Handle>();
}
//-----------------------------------------------------------------------
struct Service::StartThread: Thread{
	Service			&rsvc;
	StartData		sd;
	
	StartThread(
		Service &_rsvc,
		const StartData &_rsd
	):rsvc(_rsvc), sd(_rsd){}
	
	/*virtual*/ void run(){
		rsvc.doStart(sd);
	}
};
void Service::start(const StartData &_rsd, const bool _async){
	{
		Locker<Mutex>	lock(mtx);
		if(!expect_create_cnt){
			start_time.currentRealTime();
			last_time = start_time;
		}
		expect_create_cnt += _rsd.concnt;
		expect_connect_cnt += _rsd.concnt;
		expect_login_cnt += _rsd.concnt;
	}
	if(_async){
		StartThread *pt = new StartThread(*this, _rsd);
		pt->start();
	}else{
		doStart(_rsd);
	}
}
void Service::doStart(const StartData &_rsd){
	if(!isRegistered()){
		manager().registerService(*this);
	}
	for(size_t i = 0; i < _rsd.concnt; ++i){
		DynamicPointer<frame::aio::Object>	conptr(new Connection(i, raddrvec[i % raddrvec.size()], typemap, _rsd.reconnectcnt, _rsd.reconnectsleepmsec));
		
		this->registerObject(*conptr);
		rsched.schedule(conptr);
	}
}

void Service::send(const size_t _msgrow, const size_t _sleepms, const size_t _cnt){
	{
		Locker<Mutex>	lock(mtx);
		if(!expect_receive_cnt){
			send_time.currentRealTime();
		}
		expect_receive_cnt = expect_create_cnt * (expect_create_cnt - 1) * _cnt;
		
	}
	frame::MessageSharedPointerT	msgptr(new SendJob(_msgrow, _sleepms, _cnt));
	this->notifyAll(msgptr);
}

TimeSpec Service::startTime(){
	Locker<Mutex>	lock(mtx);
	return start_time;
}
TimeSpec Service::sendTime(){
	Locker<Mutex>	lock(mtx);
	return send_time;
}

void Service::onCreate(){
	Locker<Mutex>	lock(mtx);
	++actual_create_cnt;
	cassert(actual_create_cnt <= expect_create_cnt);
	if(actual_create_cnt == expect_create_cnt){
		cnd.signal();
	}
}
void Service::onConnect(){
	Locker<Mutex>	lock(mtx);
	++actual_connect_cnt;
	cassert(actual_connect_cnt <= expect_connect_cnt);
	if(actual_connect_cnt == expect_connect_cnt){
		cnd.signal();
	}
}
void Service::onReceive(){
	Locker<Mutex>	lock(mtx);
	++actual_receive_cnt;
	cassert(actual_receive_cnt <= expect_receive_cnt);
	if(actual_receive_cnt == expect_receive_cnt){
		cnd.signal();
	}
}
void Service::onLogin(){
	Locker<Mutex>	lock(mtx);
	++actual_login_cnt;
	cassert(actual_login_cnt <= expect_login_cnt);
	if(actual_login_cnt == expect_login_cnt){
		cnd.signal();
	}
}

void Service::onReceive(const size_t _sz){
	Locker<Mutex>	lock(mtx);
	recv_sz += _sz;
	idbg(" sz = "<<_sz<<" recv_sz = "<<recv_sz<<" waiting = "<<waiting);
	if(waiting){
		if(!(recv_cnt % 100)){
			TimeSpec crt_time;
			TimeSpec crt_time_ex;
			crt_time.currentRealTime();
			crt_time_ex = crt_time;
			crt_time -= last_time;
			if(crt_time.seconds()){
				last_time = crt_time_ex;
				crt_time_ex -= start_time;
				uint64	msecs = crt_time_ex.seconds() * 1000;
				msecs += crt_time_ex.nanoSeconds() / 1000000;
				uint64	s = recv_sz / msecs;
				cout<<"Download speed = "<<s<<" KB/s                \r";
				
			}
		}
		++recv_cnt;
	}
}
void Service::onReceiveDone(){
	TimeSpec crt_time_ex;
	crt_time_ex.currentRealTime();
	crt_time_ex -= start_time;
	uint64	msecs = crt_time_ex.seconds() * 1000;
	msecs += crt_time_ex.nanoSeconds() / 1000000;
	uint64	s = recv_sz / msecs;
	cout<<"Download speed = "<<s<<" KB/s. Received size = "<<recv_sz<<" over "<<msecs<<" milliseconds.                  \n";
}

void Service::waitCreate(){
	Locker<Mutex>	lock(mtx);
	waiting = true;
	while(actual_create_cnt < expect_create_cnt){
		cnd.wait(lock);
	}
	waiting = false;
}
void Service::waitConnect(){
	Locker<Mutex>	lock(mtx);
	waiting = true;
	while(actual_connect_cnt < expect_connect_cnt){
		cnd.wait(lock);
	}
	waiting = false;
}
void Service::waitReceive(){
	Locker<Mutex>	lock(mtx);
	waiting = true;
	while(actual_receive_cnt < expect_receive_cnt){
		cnd.wait(lock);
	}
	onReceiveDone();
	waiting = false;
}
void Service::waitLogin(){
	Locker<Mutex>	lock(mtx);
	waiting = true;
	while(actual_login_cnt < expect_login_cnt){
		cnd.wait(lock);
	}
	waiting = false;
}

//=======================================================================

void Connection::done(){
	idbg("");
}
//-----------------------------------------------------------------------
/*virtual*/ bool Connection::notify(solid::DynamicPointer<solid::frame::Message> &_rmsgptr){
	msgvec.push_back(DynamicPointer<>(_rmsgptr));
	return Object::notify(frame::S_SIG | frame::S_RAISE);
}
//-----------------------------------------------------------------------

/*static*/ Connection::DynamicMapperT		Connection::dm;

/*virtual*/ void Connection::execute(ExecuteContext &_rexectx){
	static struct Init{
		Init(DynamicMapperT &_rdm){
			_rdm.insert<SendJob, Connection>();
		}
	} ini(dm);
	typedef DynamicSharedPointer<solid::frame::Message>		MessageSharedPointerT;
	
	static Compressor 				compressor(BufferControllerT::DataCapacity);
	static MessageSharedPointerT	noopmsgptr(new NoopMessage);
	
	solid::ulong					sm = grabSignalMask();
	if(sm){
		if(sm & frame::S_KILL){
			done();
			_rexectx.close();
			return;
		}
		if(sm & frame::S_SIG){
			idbg("Receive message");
			DynamicHandler<DynamicMapperT>	dh(dm);
			{
				Locker<Mutex>	lock(frame::Manager::specific().mutex(*this));
				dh.init(msgvec.begin(), msgvec.end());
				msgvec.clear();
			}
			for(size_t i = 0; i < dh.size(); ++i){
				dh.handle(*this, i);
			}
		}
	}
	
	if(_rexectx.eventMask() & (frame::EventDoneError)){
		idbg("errdone");
		done();
		_rexectx.close();
		return;
	}
	
	ConnectionContext				ctx(*this);
	
	bool reenter = false;
	if(st == RunningState){
		if(nooptime.seconds() && nooptime <= _rexectx.currentTime()){
			if(waitnoop){
				idbg("noopreceiveerror");
				done();
				_rexectx.close();
				return;
			}else if(session.isSendQueueEmpty()){
				idbg("sendnoop");
				DynamicPointer<solid::frame::Message>	msgptr(noopmsgptr);
				session.send(3, msgptr);
				waitnoop = true;
				nooptime = _rexectx.currentTime();
				nooptime.add(10);
			}
		}
		if(nexttime.seconds() && nexttime <= _rexectx.currentTime()){
			MessageDynamicPointerT	msgptr = MessageMatrix::the().message(jobq.front()->msgrow, sendidx);
			session.send(2, msgptr);
			nexttime.seconds(0);
		}
		AsyncE rv = session.execute(*this, _rexectx.eventMask(), ctx, ser, des, bufctl, compressor);
		if(rv == solid::AsyncError){
			done();
			_rexectx.close();
			return;
		}
		if(rv == solid::AsyncWait){
			idbg("nooptime.sec = "<<nooptime.seconds()<<" nexttime.sec = "<<nexttime.seconds()<<" crttime.sec = "<<_rexectx.currentTime().seconds());
			if(nooptime.seconds() && (!nexttime.seconds() || nooptime <= nexttime)){
				_rexectx.waitUntil(nooptime);
			}else if(nexttime.seconds()){
				_rexectx.waitUntil(nexttime);
			}
			return;
		}
		if(rv == solid::AsyncError){
			_rexectx.close();
		}else{
			_rexectx.reschedule();
		}
		return;
	}else if(st == CreateState){
		st = PrepareState;
		service().onCreate();
		_rexectx.reschedule();
		return;
	}else if(st == PrepareState){
		SocketDevice	sd;
		sd.create(rsa.family(), SocketInfo::Stream);
		sd.makeNonBlocking();
		socketInsert(sd);
		socketRequestRegister();
		st = ConnectState;
		reenter = false;
	}else if(st == ConnectState){
		idbg("ConnectState");
		switch(socketConnect(rsa)){
			case frame::aio::AsyncError:
				done();
				_rexectx.close();
				return;
			case frame::aio::AsyncSuccess:
				idbg("");
				st = InitState;
				reenter = true;
				break;
			case frame::aio::AsyncWait:
				st = ConnectWaitState;
				idbg("");
				break;
		}
	}else if(st == ConnectWaitState){
		idbg("ConnectWaitState");
		if(_rexectx.eventMask() & (frame::EventDoneError | frame::EventTimeout)){
			--connectcnt;
			if(connectcnt == 0){
				done();
				_rexectx.close();
				return;
			}else{
				st = PrepareState;
			}	
		}else{
			st = InitState;
		}
		reenter = true;
	}else if(st == InitState){
		st = RunningState;
		service().onConnect();
		ostringstream			oss;
		oss<<this->service().index()<<'_'<<idx;
		string	str = oss.str();
		idbg("InitState - Login with "<<str);
		MessageDynamicPointerT	msgptr = new LoginRequest(str, str);
		session.send(1, msgptr);
		reenter = true;
	}
	if(reenter){
		_rexectx.reschedule();
	}
}
//---------------------------------------------------------------------------------
void Connection::dynamicHandle(solid::DynamicPointer<> &_dp){
	cassert(false);
}
//---------------------------------------------------------------------------------
void Connection::dynamicHandle(solid::DynamicPointer<SendJob> &_rmsgptr){
	idbg("handle send job");
	jobq.push(_rmsgptr);
	if(jobq.size() == 1){
		sendidx = 0;
		if(sendidx < jobq.front()->count){
			MessageDynamicPointerT	msgptr = MessageMatrix::the().message(jobq.front()->msgrow, sendidx);
			session.send(2, msgptr);
			nexttime = currentTime();
			nexttime += jobq.front()->sleepms;
		}
	}
}
//---------------------------------------------------------------------------------
void Connection::onReceiveNoop(){
	idbg("");
	cassert(waitnoop);
	waitnoop = false;
	nooptime = currentTime();
	nooptime.add(30);
}
//---------------------------------------------------------------------------------
void Connection::onDoneSend(const size_t _msgidx){
	idbg(""<<_msgidx);
	if(_msgidx == 2){
		++sendidx;
		if(sendidx < jobq.front()->count){
			nexttime = currentTime();
			nexttime += jobq.front()->sleepms;
		}
	}
	if(!waitnoop){
		idbg("set nooptime");
		nooptime = currentTime();
		nooptime.add(10);
	}
}
//---------------------------------------------------------------------------------
void Handle::afterSerialization(BinDeserializerT &_rs, BasicMessage *_pm, ConnectionContext &_rctx){
	idbg("on login response");
	_rctx.rcon.onLoginDone();
}
void Handle::afterSerialization(BinDeserializerT &_rs, TextMessage *_pm, ConnectionContext &_rctx){
	idbg("on text message");
	bool b = MessageMatrix::the().check(*_pm);
	cassert(b);
	_rctx.rcon.onReceiveMessage();
}
void Handle::afterSerialization(BinDeserializerT &_rs, NoopMessage *_pm, ConnectionContext &_rctx){
	idbg("on noop");
	_rctx.rcon.onReceiveNoop();
}
//---------------------------------------------------------------------------------
