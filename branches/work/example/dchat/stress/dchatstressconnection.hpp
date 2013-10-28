#ifndef DCHAT_STRESS_CONNECTION_HPP
#define DCHAT_STRESS_CONNECTION_HPP

#include "protocol/binary/binaryaiosession.hpp"
#include "protocol/binary/binarybasicbuffercontroller.hpp"

#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"
#include "utility/stack.hpp"

#include "serialization/idtypemapper.hpp"
#include "serialization/binary.hpp"

#include "frame/aio/aiosingleobject.hpp"
#include "frame/aio/aioselector.hpp"


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

typedef solid::serialization::binary::Serializer<ConnectionContext>		BinSerializerT;
typedef solid::serialization::binary::Deserializer<ConnectionContext>	BinDeserializerT;
typedef solid::serialization::IdTypeMapper<
	BinSerializerT, BinDeserializerT, solid::uint8,
	solid::serialization::FakeMutex
>																		UInt8TypeMapperT;

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


#endif
