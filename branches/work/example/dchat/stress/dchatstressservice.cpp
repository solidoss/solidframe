#include "dchatstressservice.hpp"
#include "dchatmessagematrix.hpp"
#include "system/mutex.hpp"
#include "system/debug.hpp"
#include "frame/manager.hpp"
#include "example/dchat/core/messages.hpp"
#include "example/dchat/core/compressor.hpp"

#include "protocol/binary/binaryaiosession.hpp"
#include "protocol/binary/binarybasicbuffercontroller.hpp"

#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"
#include "utility/stack.hpp"

#include "serialization/idtypemapper.hpp"
#include "serialization/binary.hpp"

#include "frame/aio/aiosingleobject.hpp"

using namespace solid;
using namespace std;

//=======================================================================

class Connection;

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
		InitState = 1,
		PrepareState,
		ConnectState,
		ConnectWaitState,
		RunningState
	};
public:
	Connection(
		solid::frame::Manager &_rm,
		const solid::ResolveData &_rd,
		const solid::serialization::TypeMapperBase &_rtm
	):rm(_rm), rd(_rd), st(PrepareState), ser(_rtm), des(_rtm), sndidx(1), waitnoop(false){}
	
	size_t send(solid::DynamicPointer<solid::frame::Message>	&_rmsgptr, const solid::uint32 _flags = 0);
	
private:
	friend struct Handle;
	friend struct SessionController;
	/*virtual*/ int execute(solid::ulong _evs, solid::TimeSpec& _crtime);
	int done();
	void onReceiveNoop();
	void onDoneIndex(solid::uint32 _msgidx);
private:
	typedef solid::Stack<size_t>	SizeStackT;
	
	solid::frame::Manager	&rm;
	solid::ResolveData		rd;
	solid::uint16			st;
	BinSerializerT			ser;
	BinDeserializerT		des;
	ProtocolSessionT		session;
	MessageVectorT			sndmsgvec;
	BufferControllerT		bufctl;
	size_t					sndidx;
	bool					waitnoop;
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

//---------------------------------------------------------------------------------
void Handle::afterSerialization(BinDeserializerT &_rs, BasicMessage *_pm, ConnectionContext &_rctx){
}
void Handle::afterSerialization(BinDeserializerT &_rs, TextMessage *_pm, ConnectionContext &_rctx){
}
void Handle::afterSerialization(BinDeserializerT &_rs, NoopMessage *_pm, ConnectionContext &_rctx){
}
//---------------------------------------------------------------------------------