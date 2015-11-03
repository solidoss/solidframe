// frame/ipc/src/ipcmessagereader.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_SRC_IPC_MESSAGE_READER_HPP
#define SOLID_FRAME_IPC_SRC_IPC_MESSAGE_READER_HPP

#include "utility/queue.hpp"
#include "system/common.hpp"
#include "system/error.hpp"
#include "frame/ipc/ipcserialization.hpp"
#include "system/specific.hpp"

namespace solid{
namespace frame{
namespace ipc{

struct Configuration;

struct PacketHeader;


struct Deserializer: public DeserializerT, public SpecificObject{
	Deserializer(TypeIdMapT const &_ridmap): DeserializerT(&_ridmap){}
};

typedef std::unique_ptr<Deserializer>		DeserializerPointerT;

class MessageReader{
public:
	enum Events{
		MessageCompleteE,
		KeepaliveCompleteE,
	};
	typedef FUNCTION<void(const Events, MessagePointerT /*const*/&)>	CompleteFunctionT;
	
	MessageReader();
	
	~MessageReader();
	
	uint32 read(
		const char *_pbuf,
		uint32 _bufsz,
		CompleteFunctionT &_complete_fnc,
		Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx,
		ErrorConditionT &_rerror
	);
	
	void prepare(Configuration const &_rconfig);
	void unprepare();
	template <class S, uint32 I>
	serialization::binary::ReturnValues serializationReinit(S &_rs, const uint64 &_rv, ConnectionContext &);
private:
	void doConsumePacket(
		const char *_pbuf,
		PacketHeader const &_packet_header,
		CompleteFunctionT &_complete_fnc,
		Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx,
		ErrorConditionT &_rerror
	);
private:
	
	enum States{
		HeaderReadStateE = 1,
		DataReadStateE,
	};
	
	struct MessageStub{
		MessageStub(
			MessagePointerT &_rmsgptr,
			const size_t _msg_type_idx,
			ulong _flags
		): message_ptr(std::move(_rmsgptr)), message_type_idx(_msg_type_idx), packet_count(0){}
		
		MessageStub():message_type_idx(InvalidIndex()), packet_count(0){}
		
		void clear(){
			message_ptr.clear();
			deserializer_ptr = nullptr;
		}
		
		MessagePointerT 		message_ptr;
		size_t					message_type_idx;
		DeserializerPointerT	deserializer_ptr;
		size_t				packet_count;
	};
	
	typedef Queue<MessageStub>		MessageQueueT;
	
	States			state;
	uint32			current_message_type_id;
	MessageQueueT	message_q;
};

}//namespace ipc
}//namespace frame
}//namespace solid


#endif
