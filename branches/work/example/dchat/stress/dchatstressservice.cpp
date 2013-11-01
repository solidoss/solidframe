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

#include "serialization/idtypemapper.hpp"
#include "serialization/binary.hpp"

#include "protocol/binary/binaryaiosession.hpp"
#include "protocol/binary/binarybasicbuffercontroller.hpp"

#include "frame/manager.hpp"
#include "frame/aio/aiosingleobject.hpp"


using namespace solid;
using namespace std;

//=======================================================================

class Connection;

struct SendJob: solid::frame::Message{
	size_t		msgrow;
	size_t		sleepms;
	SendJob(
		const size_t _msgrow = 0,
		const size_t _sleepms = 0
	):msgrow(_msgrow), sleepms(_sleepms){}
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
	typedef solid::DynamicPointer<solid::frame::Message>				MessageDynamicPointerT;
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
	
	typedef std::vector<MessageStub>									MessageVectorT;
	typedef solid::protocol::binary::AioSession<
		solid::frame::Message,
		int
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
		const SocketAddressInet &_rsa,
		const solid::serialization::TypeMapperBase &_rtm,
		const uint16 _reconnectcnt,
		const size_t _reconnectsleepms
	):	rsa(_rsa), st(CreateState), connectcnt(_reconnectcnt + 1), ser(_rtm), des(_rtm),
		sndidx(1), waitnoop(false), reconnectsleepms(_reconnectsleepms){}
	
	size_t send(solid::DynamicPointer<solid::frame::Message>	&_rmsgptr, const solid::uint32 _flags = 0);
	Service& service()const{
		return static_cast<Service&>(frame::Manager::specific().service(*this));
	}
private:
	friend struct Handle;
	friend struct SessionController;
	/*virtual*/ int execute(solid::ulong _evs, solid::TimeSpec& _crtime);
	int done();
	void onReceiveNoop();
	void onDoneIndex(solid::uint32 _msgidx);
private:
	typedef solid::Stack<size_t>	SizeStackT;
	
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
	SizeStackT				freesndidxstk;
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
	expect_create_cnt = _rsd.concnt;
	expect_connect_cnt = _rsd.concnt;
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
		DynamicPointer<frame::aio::Object>	conptr(new Connection(raddrvec[i % raddrvec.size()], typemap, _rsd.reconnectcnt, _rsd.reconnectsleepmsec));
		
		this->registerObject(*conptr);
		rsched.schedule(conptr);
	}
}

void Service::send(const size_t _msgrow, const size_t _sleepms, const size_t _cnt){
	expect_receive_cnt = expect_create_cnt * (expect_create_cnt - 1) * _cnt;
	//TODO: ...
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
	
void Service::waitCreate(){
	Locker<Mutex>	lock(mtx);
	while(actual_create_cnt < expect_create_cnt){
		cnd.wait(lock);
	}
}
void Service::waitConnect(){
	Locker<Mutex>	lock(mtx);
	while(actual_connect_cnt < expect_connect_cnt){
		cnd.wait(lock);
	}
}
void Service::waitReceive(){
	Locker<Mutex>	lock(mtx);
	while(actual_receive_cnt < expect_receive_cnt){
		cnd.wait(lock);
	}
}

//=======================================================================

size_t Connection::send(DynamicPointer<solid::frame::Message>	&_rmsgptr, const uint32 _flags){
	Locker<Mutex>	lock(frame::Manager::specific().mutex(*this));
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
		frame::Manager::specific().raise(*this);
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
	Locker<Mutex>	lock(frame::Manager::specific().mutex(*this));
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
			Locker<Mutex>	lock(frame::Manager::specific().mutex(*this));
			for(MessageVectorT::iterator it(sndmsgvec.begin()); it != sndmsgvec.end(); ++it){
				session.send(it->idx, it->msgptr, it->flags);
			}
			sndmsgvec.clear();
		}
	}
	
	if(_evs & (frame::TIMEOUT | frame::ERRDONE)){
		if(st != ConnectWaitState){
			if(waitnoop){
				return done();
			}else if(session.isSendQueueEmpty()){
				DynamicPointer<solid::frame::Message>	msgptr(noopmsgptr);
				send(msgptr);
				waitnoop = true;
			}
		}else{
			--connectcnt;
			if(connectcnt == 0){
				return done();
			}else{
				st = PrepareState;
			}
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
	}else if(st == CreateState){
		st = PrepareState;
		service().onCreate();
		return OK;
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
		service().onConnect();
		reenter = true;
	}
	return reenter ? OK : NOK;
}

//---------------------------------------------------------------------------------
void Handle::afterSerialization(BinDeserializerT &_rs, BasicMessage *_pm, ConnectionContext &_rctx){
}
void Handle::afterSerialization(BinDeserializerT &_rs, TextMessage *_pm, ConnectionContext &_rctx){
}
void Handle::afterSerialization(BinDeserializerT &_rs, NoopMessage *_pm, ConnectionContext &_rctx){
}
//---------------------------------------------------------------------------------