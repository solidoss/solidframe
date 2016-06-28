// solid/frame/ipc/src/ipcmessagewriter.hpp
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

#include "solid/system/common.hpp"
#include "solid/system/error.hpp"
#include "solid/system/function.hpp"

#include "ipcutility.hpp"
#include "solid/utility/innerlist.hpp"
#include "solid/frame/ipc/ipcprotocol.hpp"




namespace solid{
namespace frame{
namespace ipc{

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
		MessageId &_rpool_msg_id
	);
	
	bool cancelOldest(
		MessageBundle &_rmsgbundle,
		MessageId &_rpool_msg_id
	);
	
	uint32_t write(
		char *_pbuf,
		uint32_t _bufsz, const bool _keep_alive, 
		CompleteFunctionT &_complete_fnc,
		WriterConfiguration const &_rconfig,
		Protocol const &_rproto,
		ConnectionContext &_rctx,
		ErrorConditionT &_rerror
	);
	
	bool empty()const;
	
	bool full(WriterConfiguration const &_rconfig)const;
	
	void prepare(WriterConfiguration const &_rconfig);
	void unprepare();
	
	void forEveryMessagesNewerToOlder(VisitFunctionT const &_rvisit_fnc);
	
	void print(std::ostream &_ros, const PrintWhat _what)const;
private:
	
	enum{
		InnerLinkStatus = 0,
		InnerLinkOrder,
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
		):	msgbundle(std::move(_rmsgbundle)), packet_count(0){}
		
		MessageStub(
		):	unique(0), packet_count(0){}
		
		MessageStub(
			MessageStub &&_rmsgstub
		):	InnerNode<InnerLinkCount>(std::move(_rmsgstub)),
			msgbundle(std::move(_rmsgstub.msgbundle)), unique(_rmsgstub.unique),
			packet_count(_rmsgstub.packet_count), serializer_ptr(std::move(_rmsgstub.serializer_ptr)),
			pool_msg_id(_rmsgstub.pool_msg_id){}
		
		void clear(){
			msgbundle.clear();
			++unique;
			packet_count = 0;
			
			serializer_ptr = nullptr;
			
			pool_msg_id.clear();
		}
		
		bool isStop()const noexcept {
			return not msgbundle.message_ptr and not Message::is_canceled(msgbundle.message_flags);
		}
		
		bool isCanceled()const noexcept {
			return Message::is_canceled(msgbundle.message_flags);
		}
		
		MessageBundle				msgbundle;
		uint32_t						unique;
		size_t						packet_count;
		SerializerPointerT			serializer_ptr;
		MessageId					pool_msg_id;
	};
	
	using MessageVectorT 			= std::vector<MessageStub>;
	using MessageOrderInnerListT	= InnerList<MessageVectorT, InnerLinkOrder>;
	using MessageStatusInnerListT	= InnerList<MessageVectorT, InnerLinkStatus>;
	
	struct PacketOptions{
		PacketOptions(): packet_type(PacketHeader::SwitchToNewMessageTypeE), force_no_compress(false){}
		
		PacketHeader::Types		packet_type;
		bool 					force_no_compress;
	};
	
	char* doFillPacket(
		char* _pbufbeg,
		char* _pbufend,
		PacketOptions &_rpacket_options,
		bool &_rmore,
		CompleteFunctionT &_complete_fnc,
		WriterConfiguration const &_rconfig,
		Protocol const &_rproto,
		ConnectionContext &_rctx,
		ErrorConditionT & _rerror
	);
	
	bool doCancel(
		const size_t _msgidx,
		MessageBundle &_rmsgbundle,
		MessageId &_rpool_msg_id
	);
	
	bool isSynchronousInSendingQueue()const;
	bool isAsynchronousInPendingQueue()const;
	bool isDelayedCloseInPendingQueue()const;
	
	void doTryMoveMessageFromPendingToWriteQueue(ipc::Configuration const &_rconfig);
	
	PacketHeader::Types doPrepareMessageForSending(
		const size_t _msgidx,
		WriterConfiguration const &_rconfig,
		Protocol const &_rproto,
		ConnectionContext &_rctx,
		SerializerPointerT &_rtmp_serializer
	);
	
	void doTryCompleteMessageAfterSerialization(
		const size_t _msgidx,
		CompleteFunctionT &_complete_fnc,
		WriterConfiguration const &_rconfig,
		Protocol const &_rproto,
		ConnectionContext &_rctx,
		SerializerPointerT &_rtmp_serializer,
		ErrorConditionT & _rerror
	);
	
	void doUnprepareMessageStub(const size_t _msgidx);
private:
	MessageVectorT				message_vec;
	uint32_t						current_message_type_id;
	size_t						current_synchronous_message_idx;
	MessageOrderInnerListT		order_inner_list;
	MessageStatusInnerListT		write_inner_list;
	MessageStatusInnerListT		cache_inner_list;
};

typedef std::pair<MessageWriter const&, MessageWriter::PrintWhat>	MessageWriterPrintPairT;

std::ostream& operator<<(std::ostream &_ros, MessageWriterPrintPairT const &_msgwriter);

}//namespace ipc
}//namespace frame
}//namespace solid


#endif
