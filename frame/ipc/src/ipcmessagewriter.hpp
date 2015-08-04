// frame/ipc/src/ipcmessagewriter.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_SRC_IPC_MESSAGE_WRITER_HPP
#define SOLID_FRAME_IPC_SRC_IPC_MESSAGE_WRITER_HPP

#include "system/common.hpp"
#include "system/error.hpp"
#include "system/specific.hpp"
#include "system/function.hpp"

#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "ipcutility.hpp"
#include "frame/ipc/ipcserialization.hpp"




namespace solid{
namespace frame{
namespace ipc{

struct Configuration;

struct Serializer: public SerializerT, public SpecificObject{
	Serializer(TypeIdMapT const &_ridmap): SerializerT(&_ridmap){}
};

typedef std::unique_ptr<Serializer>		SerializerPointerT;
typedef FUNCTION<void(
	MessagePointerT &/*_rmsgptr*/,
	const size_t /*_msg_type_idx*/,
	ulong /*_flags*/, const bool /*_sent*/)
>										MessageWriterVisitFunctionT;


class MessageWriter{
public:
	MessageWriter();
	~MessageWriter();
	
	void enqueue(
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		ulong _flags,
		Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx
	);
	
	uint32 write(
		char *_pbuf,
		uint32 _bufsz, const bool _keep_alive, 
		Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx,
		ErrorConditionT &_rerror
	);
	
	void completeMessage(
		MessageUid const &_rmsguid,
		ipc::Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx,
		ErrorConditionT const & _rerror
	);
	
	void completeAllMessages(
		ipc::Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx,
		ErrorConditionT const & _rerror
	);
	
	bool shouldTryFetchNewMessage(Configuration const &_rconfig)const;
	
	bool empty()const;
	
	void prepare(Configuration const &_rconfig);
	void unprepare();
	
	void visitAllMessages(MessageWriterVisitFunctionT const &_rvisit_fnc);
private:
	struct PendingMessageStub{
		PendingMessageStub(
			MessagePointerT &_rmsgptr,
			const size_t _msg_type_idx,
			ulong _flags
		): message_ptr(std::move(_rmsgptr)), message_type_idx(_msg_type_idx), flags(_flags){}
		
		PendingMessageStub():message_type_idx(-1), flags(0){}
		
		MessagePointerT message_ptr;
		size_t			message_type_idx;
		ulong			flags;
	};
	
	struct MessageStub{
		MessageStub(
			MessagePointerT &_rmsgptr,
			const size_t _msg_type_idx,
			ulong _flags
		): message_ptr(std::move(_rmsgptr)), message_type_idx(_msg_type_idx), flags(_flags), packet_count(0){}
		
		MessageStub():message_type_idx(-1), flags(-1), unique(0), packet_count(0){}
		
		void clear(){
			message_ptr.clear();
			flags = 0;
			++unique;
			packet_count = 0;
			serializer_ptr = nullptr;
		}
		
		MessagePointerT 		message_ptr;
		size_t					message_type_idx;
		ulong					flags;
		uint32					unique;
		size_t					packet_count;
		SerializerPointerT		serializer_ptr;
	};
	
	typedef Queue<PendingMessageStub>							PendingMessageQueueT;
	typedef std::vector<MessageStub>							MessageVectorT;
	
	struct PacketOptions{
		PacketOptions(): packet_type(PacketHeader::SwitchToNewMessageTypeE), force_no_compress(false){}
		
		PacketHeader::Types		packet_type;
		bool 					force_no_compress;
	};
	
	typedef Queue<size_t>										SizeTQueueT;
	typedef Stack<size_t>										CacheStackT;
	
	char* doFillPacket(
		char* _pbufbeg,
		char* _pbufend,
		PacketOptions &_rpacket_options,
		bool &_rmore,
		ipc::Configuration const &_rconfig,
		TypeIdMapT const & _ridmap,
		ConnectionContext &_rctx,
		ErrorConditionT & _rerror
	);
	
	bool isSynchronousInWriteQueue()const;
	bool isAsynchronousInPendingQueue()const;
	
	void doTryMoveMessageFromPendingToWriteQueue(ipc::Configuration const &_rconfig);
	void doCompleteMessage(
		MessageUid const &_rmsguid,
		ipc::Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx,
		ErrorConditionT const & _rerror
	);
private:
	PendingMessageQueueT		pending_message_q;
	MessageVectorT				message_vec;
	SizeTQueueT					write_q;
	CacheStackT					cache_stk;
	uint32						current_message_type_id;
	uint32						flags;
};


}//namespace ipc
}//namespace frame
}//namespace solid


#endif
