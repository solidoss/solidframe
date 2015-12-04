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

#include <vector>

#include "system/common.hpp"
#include "system/error.hpp"
#include "system/specific.hpp"
#include "system/function.hpp"

#include "ipcutility.hpp"
#include "utility/innerlist.hpp"
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
	ResponseHandlerFunctionT &,
	ulong /*_flags*/
)>										MessageWriterVisitFunctionT;

class MessageWriter{
public:
	enum PrintWhat{
		PrintInnerListsE,
	};
	
	MessageWriter();
	~MessageWriter();
	
	//must be used under lock, i.e. under Connection's lock
	MessageId safeNewMessageId(Configuration const &_rconfig);
	MessageId safeForcedNewMessageId();
	
	bool isNonSafeCacheEmpty()const;
	
	void safeMoveCacheToSafety();
	
	void enqueue(
		MessageBundle &_rmsgbundle,
		MessageId const &_rmsguid,
		Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx
	);
	
	void cancel(
		MessageId const &_rmsguid,
		Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx
	);
	
	void enqueueClose(MessageId const &_rmsguid);
	
	uint32 write(
		char *_pbuf,
		uint32 _bufsz, const bool _keep_alive, 
		Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx,
		ErrorConditionT &_rerror
	);
	
	void completeMessage(
		MessagePointerT &_rmsgptr,
		MessageId const &_rmsguid,
		ipc::Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx,
		ErrorConditionT const & _rerror
	);
	
	bool shouldTryFetchNewMessage(Configuration const &_rconfig)const;
	
	bool empty()const;
	
	size_t freeSeatsCount(Configuration const &_rconfig)const;
	
	void prepare(Configuration const &_rconfig);
	void unprepare();
	
	void visitAllMessages(MessageWriterVisitFunctionT const &_rvisit_fnc);
	
	void completeAllMessages(
		ipc::Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx,
		ErrorConditionT const & _rerror
	);
	void print(std::ostream &_ros, const PrintWhat _what)const;
private:
	
	enum{
		InnerLinkOrder = 0,
		InnerLinkStatus,
		
		InnerLinkCount
	};
	
	enum InnerStatus{
		InnerStatusInvalid,
		InnerStatusPending = 1,
		InnerStatusSending,
		InnerStatusWaiting,
	};
	
	struct MessageStub: InnerNode<InnerLinkCount>{
		MessageStub(
			MessageBundle &_rmsgbundle
		):	msgbundle(std::move(_rmsgbundle)), packet_count(0),
			inner_status(InnerStatusInvalid){}
		
		MessageStub(
		):	unique(0), packet_count(0),
			inner_status(InnerStatusInvalid){}
		
		void clear(){
			msgbundle.clear();
			++unique;
			packet_count = 0;
			
			inner_status = InnerStatusInvalid;
			
			serializer_ptr = nullptr;
		}
		
		bool isStop()const noexcept {
			return msgbundle.message_ptr.empty() and not Message::is_canceled(msgbundle.message_flags);
		}
		
		bool isCanceled()const noexcept {
			return Message::is_canceled(msgbundle.message_flags);
		}
		
		MessageBundle				msgbundle;
		uint32						unique;
		size_t						packet_count;
		SerializerPointerT			serializer_ptr;
		InnerStatus					inner_status;
	};
	
	typedef std::vector<MessageStub>							MessageVectorT;
	typedef std::vector<MessageId>								MessageIdVectorT;
	typedef InnerList<MessageVectorT, InnerLinkOrder>			MessageOrderInnerListT;
	typedef InnerList<MessageVectorT, InnerLinkStatus>			MessageStatusInnerListT;
	
	struct PacketOptions{
		PacketOptions(): packet_type(PacketHeader::SwitchToNewMessageTypeE), force_no_compress(false){}
		
		PacketHeader::Types		packet_type;
		bool 					force_no_compress;
	};
	
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
	
	bool isSynchronousInSendingQueue()const;
	bool isAsynchronousInPendingQueue()const;
	bool isDelayedCloseInPendingQueue()const;
	
	void doTryMoveMessageFromPendingToWriteQueue(ipc::Configuration const &_rconfig);
	void doCompleteMessage(
		MessagePointerT &_rmsgptr,
		MessageId const &_rmsguid,
		ipc::Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx,
		ErrorConditionT const & _rerror
	);
	
	PacketHeader::Types doPrepareMessageForSending(
		MessageStub &_rmsgstub, ipc::Configuration const &_rconfig,
		TypeIdMapT const & _ridmap,
		ConnectionContext &_rctx,
		SerializerPointerT &_rtmp_serializer
	);
	
	void doTryCompleteMessageAfterSerialization(
		const size_t _msgidx,
		MessageStub &_rmsgstub, ipc::Configuration const &_rconfig,
		TypeIdMapT const & _ridmap,
		ConnectionContext &_rctx,
		SerializerPointerT &_rtmp_serializer
	);
private:
	MessageVectorT				message_vec;
	MessageIdVectorT			message_uid_cache_vec;
	std::atomic<size_t>			message_idx_cache;
	uint32						current_message_type_id;
	uint32						flags;
	MessageOrderInnerListT		order_inner_list;
	MessageStatusInnerListT		pending_inner_list;
	MessageStatusInnerListT		sending_inner_list;
	MessageStatusInnerListT		cached_inner_list;
};

typedef std::pair<MessageWriter const&, MessageWriter::PrintWhat>	MessageWriterPrintPairT;

std::ostream& operator<<(std::ostream &_ros, MessageWriterPrintPairT const &_msgwriter);

inline bool MessageWriter::isNonSafeCacheEmpty()const{
	return cached_inner_list.empty();
}

}//namespace ipc
}//namespace frame
}//namespace solid


#endif
