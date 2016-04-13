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

class MessageWriter{
public:
	
	using VisitFunctionT = FUNCTION<void(
		MessageBundle &/*_rmsgbundle*/,
		MessageId const &/*_rmsgid*/
	)>;
	
	using CompleteFunctionT = FUNCTION<ErrorConditionT(
		MessageBundle &/*_rmsgbundle*/,
		MessageId const &/*_rmsgid*/
	)>;
	
	enum PrintWhat{
		PrintInnerListsE,
	};
	
	MessageWriter();
	~MessageWriter();
	
	bool enqueue(
		WriterConfiguration const &_rconfig,
		MessageBundle &_rmsgbundle,
		MessageId const &_rpool_msg_id,
		MessageId &_rconn_msg_id
	);
	
	bool cancel(
		MessageId const &_rmsguid,
		MessageBundle &_rmsgbundle,
		MessageId &_rconn_msg_id
	);
	
	bool cancelOldest(
		MessageBundle &_rmsgbundle,
		MessageId &_rconn_msg_id
	);
	
	uint32 write(
		char *_pbuf,
		uint32 _bufsz, const bool _keep_alive, 
		CompleteFunctionT &_complete_fnc,
		Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx,
		ErrorConditionT &_rerror
	);
	
	bool empty()const;
	
	void prepare(WriterConfiguration const &_rconfig);
	void unprepare();
	
	void visitAllMessages(VisitFunctionT const &_rvisit_fnc);
	
	void print(std::ostream &_ros, const PrintWhat _what)const;
private:
	
	enum{
		InnerLinkOrder = 0,
		InnerLinkStatus,
		
		InnerLinkCount
	};
	
	enum struct InnerStatus{
		Invalid,
		Pending = 1,
		Sending,
		Waiting,
		Completing
	};
	
	struct MessageStub: InnerNode<InnerLinkCount>{
		MessageStub(
			MessageBundle &_rmsgbundle
		):	msgbundle(std::move(_rmsgbundle)), packet_count(0),
			inner_status(InnerStatus::Invalid){}
		
		MessageStub(
		):	unique(0), packet_count(0),
			inner_status(InnerStatus::Invalid){}
		
		MessageStub(
			MessageStub &&_rmsgstub
		):	InnerNode<InnerLinkCount>(std::move(_rmsgstub)),
			msgbundle(std::move(_rmsgstub.msgbundle)), unique(_rmsgstub.unique),
			packet_count(_rmsgstub.packet_count), serializer_ptr(std::move(_rmsgstub.serializer_ptr)),
			inner_status(_rmsgstub.inner_status), msg_id(_rmsgstub.msg_id){}
		
		void clearToInvalid(){
			msgbundle.clear();
			++unique;
			packet_count = 0;
			
			inner_status = InnerStatus::Invalid;
			
			serializer_ptr = nullptr;
			
			msg_id.clear();
		}
		
		void clearToCompleting(){
			msgbundle.clear();
			++unique;
			packet_count = 0;
			
			inner_status = InnerStatus::Completing;
			
			serializer_ptr = nullptr;
			
			//no msg_id.clear() - wait after visitCompletingMessages
		}
		
		bool isStop()const noexcept {
			return msgbundle.message_ptr.empty() and not Message::is_canceled(msgbundle.message_flags);
		}
		
		bool isCompletingStatus()const noexcept{
			return inner_status == InnerStatus::Completing;
		}
		
		bool isInvalidStatus()const noexcept{
			return inner_status == InnerStatus::Invalid;
		}
		
		bool isCanceled()const noexcept {
			return Message::is_canceled(msgbundle.message_flags);
		}
		
		MessageBundle				msgbundle;
		uint32						unique;
		size_t						packet_count;
		SerializerPointerT			serializer_ptr;
		InnerStatus					inner_status;
		MessageId					msg_id;
	};
	
	typedef std::vector<MessageStub>							MessageVectorT;
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
	
	void doUnprepareMessageStub(const size_t _msgidx);
private:
	MessageVectorT				message_vec;
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

}//namespace ipc
}//namespace frame
}//namespace solid


#endif
