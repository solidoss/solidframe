// frame/ipc/src/ipcmessagereader.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "ipcmessagewriter.hpp"
#include "ipcutility.hpp"

#include "frame/ipc/ipcconfiguration.hpp"
#include "frame/ipc/ipcerror.hpp"

#include "system/cassert.hpp"
#include "system/debug.hpp"

namespace solid{
namespace frame{
namespace ipc{
//-----------------------------------------------------------------------------
enum WriterFlags{
	SynchronousMessageInSendingQueueFlag	= 1,
	AsynchronousMessageInPendingQueueFlag	= 2,
	DelayedCloseInPendingQueueFlag			= 4,
};
inline bool MessageWriter::isSynchronousInSendingQueue()const{
	return (flags & SynchronousMessageInSendingQueueFlag) != 0;
}
inline bool MessageWriter::isAsynchronousInPendingQueue()const{
	return (flags & AsynchronousMessageInPendingQueueFlag) != 0;
}

inline bool MessageWriter::isDelayedCloseInPendingQueue()const{
	return (flags & DelayedCloseInPendingQueueFlag) != 0;
}

//-----------------------------------------------------------------------------
MessageWriter::MessageWriter(
):	current_message_type_id(InvalidIndex()), flags(0),
	order_inner_list(message_vec), pending_inner_list(message_vec),
	sending_inner_list(message_vec), cached_inner_list(message_vec){
}
//-----------------------------------------------------------------------------
MessageWriter::~MessageWriter(){}
//-----------------------------------------------------------------------------
void MessageWriter::prepare(Configuration const &_rconfig){
	
}
//-----------------------------------------------------------------------------
void MessageWriter::unprepare(){
	
}
//-----------------------------------------------------------------------------
MessageUid MessageWriter::safeNewMessageUid(Configuration const &_rconfig){
	MessageUid msguid;
	if(message_uid_cache_vec.size()){
		msguid = message_uid_cache_vec.back();
		message_uid_cache_vec.pop_back();
	}else if(message_idx_cache < _rconfig.max_writer_message_count){
		msguid.unique = 0;
		msguid.index = message_idx_cache.fetch_add(1);
	}
	return msguid;
}
//-----------------------------------------------------------------------------
MessageUid MessageWriter::safeForcedNewMessageUid(){
	MessageUid msguid;
	if(message_uid_cache_vec.size()){
		msguid = message_uid_cache_vec.back();
		message_uid_cache_vec.pop_back();
	}else{
		msguid.unique = 0;
		msguid.index = message_idx_cache.fetch_add(1);
	}
	return msguid;
}
//-----------------------------------------------------------------------------
void MessageWriter::safeMoveCacheToSafety(){
	while(cached_inner_list.size()){
		const size_t 	msgidx = cached_inner_list.frontIndex();
		MessageStub		&rmsgstub = cached_inner_list.front();
		
		rmsgstub.inner_status = InnerStatusInvalid;
		
		message_uid_cache_vec.push_back(MessageUid(msgidx, rmsgstub.unique));
		
		cached_inner_list.popFront();
	}
}
//-----------------------------------------------------------------------------
// Needs:
// max prequeue size
// max multiplexed message count
// max waiting message count
// function message_complete
// enqueuing Synchronous Messages:
// 		put on pending_message_q
// 
//
void MessageWriter::enqueue(
	MessageBundle &_rmsgbundle,
	MessageUid const &_rmsguid,
	Configuration const &_rconfig,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx
){
	cassert(not _rmsgbundle.message_ptr.empty());
	cassert(_rmsguid.isValid());
	
	const size_t	idx = _rmsguid.index;
	
	if(isDelayedCloseInPendingQueue()){
		//message cannot be enqued because a close is enqueued
		ErrorConditionT		error = error_delayed_closed_pending;
		
		_rctx.message_flags = _rmsgbundle.message_flags;
		
		if(not FUNCTION_EMPTY(_rmsgbundle.response_fnc)){
			MessagePointerT		msgptr;//the empty response message
			
			_rmsgbundle.response_fnc(_rctx, msgptr, error);
			FUNCTION_CLEAR(_rmsgbundle.response_fnc);
		}
		
		_ridmap[_rmsgbundle.message_type_id].complete_fnc(_rctx, _rmsgbundle.message_ptr, error);
		
		cached_inner_list.pushBack(idx);
		return;
	}
		
	if(idx >= message_vec.size()){
		message_vec.resize(idx + 1);
	}
	
	MessageStub		&rmsgstub(message_vec[idx]);
	
	rmsgstub.msgbundle = std::move(_rmsgbundle);
	
	order_inner_list.pushBack(idx);
	
	if(
		sending_inner_list.size() < _rconfig.max_writer_multiplex_message_count and
		(
			Message::is_asynchronous(_rmsgbundle.message_flags) or
			(
				Message::is_synchronous(_rmsgbundle.message_flags) and not isSynchronousInSendingQueue()
			)
		)
	){
		//put the message in the sending queue
		
		sending_inner_list.pushBack(idx);
		
		rmsgstub.inner_status = InnerStatusSending;
		
		if(Message::is_synchronous(rmsgstub.msgbundle.message_flags)){
			this->flags |= SynchronousMessageInSendingQueueFlag;
		}
	}else{
		//put the message in the pending queue
		
		pending_inner_list.pushBack(idx);
		
		rmsgstub.inner_status = InnerStatusPending;
		
		if(Message::is_asynchronous(rmsgstub.msgbundle.message_flags)){
			this->flags |= AsynchronousMessageInPendingQueueFlag;
		}
	}
	vdbgx(Debug::ipc, MessageWriterPrintPairT(*this, PrintInnerListsE));
}
//-----------------------------------------------------------------------------
void MessageWriter::enqueueClose(MessageUid const &_rmsguid){
	const size_t	idx = _rmsguid.index;
	
	if(isDelayedCloseInPendingQueue()){
		//close cannot be enqued because one is already 
		cached_inner_list.pushBack(idx);
		return;
	}
	
	flags |= DelayedCloseInPendingQueueFlag;
	
	if(idx >= message_vec.size()){
		message_vec.resize(idx + 1);
	}
	
	MessageStub		&rmsgstub(message_vec[idx]);
	
	order_inner_list.pushBack(idx);
	
	if(
		sending_inner_list.size()
	){
		pending_inner_list.pushBack(idx);
		
		rmsgstub.inner_status = InnerStatusPending;
	}else{
		sending_inner_list.pushBack(idx);
		
		rmsgstub.inner_status = InnerStatusSending;
	}
	vdbgx(Debug::ipc, MessageWriterPrintPairT(*this, PrintInnerListsE));
}
//-----------------------------------------------------------------------------
void MessageWriter::cancel(
	MessageUid const &_rmsguid,
	Configuration const &_rconfig,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx
){
	if(
		_rmsguid.index < message_vec.size() and
		message_vec[_rmsguid.index].unique == _rmsguid.unique and
		not message_vec[_rmsguid.index].msgbundle.message_ptr.empty()
	){
		const size_t		msgidx = _rmsguid.index;
		MessageStub			&rmsgstub(message_vec[msgidx]);
		ErrorConditionT		error;
		
		cassert(rmsgstub.inner_status != InnerStatusInvalid);
		
		rmsgstub.msgbundle.message_flags |= Message::CanceledFlagE;
		
		_rctx.message_flags = rmsgstub.msgbundle.message_flags;
		
		error.assign(-1, error.category());//TODO: canceled
		
		if(not FUNCTION_EMPTY(rmsgstub.msgbundle.response_fnc)){
			MessagePointerT		msgptr;//the empty response message
			
			rmsgstub.msgbundle.response_fnc(_rctx, msgptr, error);
			FUNCTION_CLEAR(rmsgstub.msgbundle.response_fnc);
		}
		
		_ridmap[rmsgstub.msgbundle.message_type_id].complete_fnc(_rctx, rmsgstub.msgbundle.message_ptr, error);
		
		if(rmsgstub.inner_status == InnerStatusPending){
			//message not already sending - erase it from the lists and clear the stub
			order_inner_list.erase(msgidx);
			pending_inner_list.erase(msgidx);
			rmsgstub.clear();
			cached_inner_list.pushBack(msgidx);
			
		}else if(rmsgstub.inner_status == InnerStatusSending and not rmsgstub.serializer_ptr){
			//message not already sending - erase it from the lists and clear the stub
			order_inner_list.erase(msgidx);
			sending_inner_list.erase(msgidx);
			rmsgstub.clear();
			cached_inner_list.pushBack(msgidx);
			
		}else if(rmsgstub.inner_status == InnerStatusSending){
			//message is currently being sent
			//we cannot erase it from the lists
			//we need to clear the clear both the message_ptr and the serializer_ptr
			rmsgstub.msgbundle.message_ptr.clear();
			rmsgstub.serializer_ptr->clear();
		}else if(rmsgstub.inner_status == InnerStatusWaiting){
			//message already sent - erase it from the lists and clear the stub
			order_inner_list.erase(msgidx);
			rmsgstub.clear();
			cached_inner_list.pushBack(msgidx);
		}
	}
	vdbgx(Debug::ipc, MessageWriterPrintPairT(*this, PrintInnerListsE));
}
//-----------------------------------------------------------------------------
#if 0
void MessageWriter::completeAllCanceledMessages(
	Configuration const &_rconfig,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx
){
	size_t 				msgidx = inner_list[InnerListCache].front;
	size_t				sz = inner_list[InnerListCache].size;
	ErrorConditionT		error;
	
	//TODO:
	error.assign(-1, error.category());
	
	while(sz){
		MessageStub		&rmsgstub(message_vec[msgidx]);
		MessageUid		msguid(msgidx, rmsgstub.unique);
		
		cassert(rmsgstub.inner_status == InnerStatusCache);
		
		msgidx = rmsgstub.inner_link[InnerLinkStatus].prev;
		
		if(not rmsgstub.message_ptr.empty()){
			MessagePointerT		msgptr;
			doCompleteMessage(msgptr, msguid, _rconfig, _ridmap, _rctx, error);
		}
		
		--sz;
	}
}
#endif
//-----------------------------------------------------------------------------
size_t MessageWriter::freeSeatsCount(Configuration const &_rconfig)const{
	return _rconfig.max_writer_message_count - sending_inner_list.size() - pending_inner_list.size();
}
//-----------------------------------------------------------------------------
bool MessageWriter::shouldTryFetchNewMessage(Configuration const &_rconfig)const{
	return (
		freeSeatsCount(_rconfig) and (
			sending_inner_list.empty() or 
			(
				sending_inner_list.size() < _rconfig.max_writer_multiplex_message_count and
				sending_inner_list.front().packet_count == 0
			)
		)
	);
}
//-----------------------------------------------------------------------------
bool MessageWriter::empty()const{
	return sending_inner_list.empty() and pending_inner_list.empty();
}
//-----------------------------------------------------------------------------
// Does:
// prepare message
// serialize messages on buffer
// completes messages - those that do not need wait for response
// keeps a serializer for every message that is multiplexed
// compress the filled buffer
// 
// Needs:
// 

uint32 MessageWriter::write(
	char *_pbuf, uint32 _bufsz, const bool _keep_alive,
	Configuration const &_rconfig,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx, ErrorConditionT &_rerror
){
	char		*pbufpos = _pbuf;
	char		*pbufend = _pbuf + _bufsz;
	
	uint32		freesz = pbufend - pbufpos;
	
	bool		more = true;
	
	while(freesz >= (PacketHeader::SizeOfE + MinimumFreePacketDataSize) and more){
		PacketHeader	packet_header(PacketHeader::SwitchToNewMessageTypeE, 0, 0);
		PacketOptions	packet_options;
		char			*pbufdata = pbufpos + PacketHeader::SizeOfE;
		char			*pbuftmp = doFillPacket(pbufdata, pbufend, packet_options, more, _rconfig, _ridmap, _rctx, _rerror);
		
		if(pbuftmp != pbufdata){
			
			if(not packet_options.force_no_compress){
				size_t compressed_size = _rconfig.inplace_compress_fnc(pbufdata, pbuftmp - pbufdata, _rerror);
				if(compressed_size){
					packet_header.flags( packet_header.flags() | PacketHeader::CompressedFlagE);
					pbuftmp = pbufdata + compressed_size;
				}else if(!_rerror){
					//the buffer was not modified, we can send it uncompressed
				}else{
					//there was an error and the inplace buffer was changed - exit with error
					more = false;
					continue;
				}
			}
			
			cassert((pbuftmp - pbufdata) < static_cast<size_t>(0xffff));
			
			packet_header.type(packet_options.packet_type);
			packet_header.size(pbuftmp - pbufdata);
			
			pbufpos = packet_header.store<SerializerT>(pbufpos);
			pbufpos = pbuftmp;
			freesz  = pbufend - pbufpos;
		}else{
			more = false;
		}
	}
	
	if(not _rerror){
		if(pbufpos == _pbuf and _keep_alive){
			PacketHeader			packet_header(PacketHeader::KeepAliveTypeE, 0, 0);
			pbufpos = packet_header.store<SerializerT>(pbufpos);
		}
		return pbufpos - _pbuf;
	}else{
		return 0;
	}
}
//-----------------------------------------------------------------------------
//	|4B - PacketHeader|PacketHeader.size - PacketData|
//	Examples:
//
//	3 Messages Packet
//	|[PH(NewMessageTypeE)]|[MessageData-1][1B - DataType = NewMessageTypeE][MessageData-2][NewMessageTypeE][MessageData-3]|
//
//	2 Messages, one spread over 3 packets and one new:
//	|[PH(NewMessageTypeE)]|[MessageData-1]|
//	|[PH(ContinuedMessageTypeE)]|[MessageData-1]|
//	|[PH(ContinuedMessageTypeE)]|[MessageData-1][NewMessageTypeE][MessageData-2]|
//
//	3 Messages, one Continued, one old continued and one new
//	|[PH(ContinuedMessageTypeE)]|[MessageData-3]|
//	|[PH(ContinuedMessageTypeE)]|[MessageData-3]| # message 3 not finished
//	|[PH(OldMessageTypeE)]|[MessageData-2]|
//	|[PH(ContinuedMessageTypeE)]|[MessageData-2]|
//	|[PH(ContinuedMessageTypeE)]|[MessageData-2]|
//	|[PH(ContinuedMessageTypeE)]|[MessageData-2][NewMessageTypeE][MessageData-4][OldMessageTypeE][MessageData-3]| # message 2 finished, message 3 still not finished
//	|[PH(ContinuedMessageTypeE)]|[MessageData-3]|
//
//
//	NOTE:
//	Header type can be: NewMessageTypeE, OldMessageTypeE, ContinuedMessageTypeE
//	Data type can be: NewMessageTypeE, OldMessageTypeE
//	
//	PROBLEM:
//	1) Should we call prepare on a message, and if so when?
//		> If we call it on doFillPacket, we cannot use prepare as a flags filter.
//			This is because of Send Synchronous Flag which, if sent on prepare would be too late.
//			One cannot set Send Synchronous Flag because the connection might not be the
//			one handling Synchronous Messages.
//		
//		> If we call it before message gets to a connection we do not have a MessageId (e.g. can be used for tracing).
//
//		
//		* Decided to drop the "prepare" functionality.
//
char* MessageWriter::doFillPacket(
	char* _pbufbeg,
	char* _pbufend,
	PacketOptions &_rpacket_options,
	bool &_rmore,
	ipc::Configuration const &_rconfig,
	TypeIdMapT const & _ridmap,
	ConnectionContext &_rctx,
	ErrorConditionT & _rerror
){
	char 					*pbufpos = _pbufbeg;
	uint32					freesz = _pbufend - pbufpos;
	
	SerializerPointerT		tmp_serializer;
	
	while(sending_inner_list.size() and freesz >= MinimumFreePacketDataSize){
		const size_t			msgidx = sending_inner_list.frontIndex();
		MessageStub				&rmsgstub = message_vec[msgidx];
		PacketHeader::Types		msgswitch;// = PacketHeader::ContinuedMessageTypeE;
		
		if(rmsgstub.msgbundle.message_ptr.empty()){
			//we need to stop
			if(pbufpos == _pbufbeg){
				_rerror = error_connection_delayed_closed;
				pbufpos = nullptr;
				sending_inner_list.popFront();
			}
			_rmore = false;
			break;
		}
		
		msgswitch = doPrepareMessageForSending(rmsgstub, _rconfig, _ridmap, _rctx, tmp_serializer);
		
		if(pbufpos == _pbufbeg){
			//first message in the packet
			_rpacket_options.packet_type = msgswitch;
		}else{
			uint8	tmp = static_cast<uint8>(msgswitch);
			pbufpos = SerializerT::storeValue(pbufpos, tmp);
		}
		
		if(Message::is_canceled(rmsgstub.msgbundle.message_flags)){
			//message already completed - just drop it from lists
			order_inner_list.erase(msgidx);
			sending_inner_list.erase(msgidx);
			rmsgstub.clear();
			cached_inner_list.pushBack(msgidx);
			continue;
		}
		
		if(not rmsgstub.msgbundle.message_ptr->isOnPeer()){
			_rctx.request_uid.index  = msgidx;
			_rctx.request_uid.unique = rmsgstub.unique;
		}else{
			_rctx.request_uid = rmsgstub.msgbundle.message_ptr->requestId();
		}
		_rctx.message_state = rmsgstub.msgbundle.message_ptr->state() + 1;
		
		
		const int rv = rmsgstub.serializer_ptr->run(pbufpos, _pbufend - pbufpos, _rctx);
		
		if(rv > 0){
			
			pbufpos += rv;
			freesz -= rv;
			
			doTryCompleteMessageAfterSerialization(msgidx, rmsgstub, _rconfig, _ridmap, _rctx, tmp_serializer);
			
		}else{
			_rerror = rmsgstub.serializer_ptr->error();
			pbufpos = nullptr;
			break;
		}
	}
	
	vdbgx(Debug::ipc, "write_q_size "<<sending_inner_list.size()<<" pending_q_size "<<pending_inner_list.size());
	
	return pbufpos;
}
//-----------------------------------------------------------------------------
void MessageWriter::doTryCompleteMessageAfterSerialization(
	const size_t _msgidx,
	MessageStub &_rmsgstub, ipc::Configuration const &_rconfig,
	TypeIdMapT const & _ridmap,
	ConnectionContext &_rctx,
	SerializerPointerT &_rtmp_serializer
){
	if(_rmsgstub.serializer_ptr->empty()){
		RequestUid		requid(_msgidx, _rmsgstub.unique);
		vdbgx(Debug::ipc, "done serializing message "<<requid<<". Message id sent to client "<<_rctx.request_uid);
		_rtmp_serializer = std::move(_rmsgstub.serializer_ptr);
		//done serializing the message:
		
		sending_inner_list.popFront();
		
		_rmsgstub.inner_status = InnerStatusWaiting;
		
		if(Message::is_synchronous(_rmsgstub.msgbundle.message_flags)){
			this->flags &= ~(SynchronousMessageInSendingQueueFlag);
		}
		
		_rmsgstub.msgbundle.message_flags &= (~Message::StartedSendFlagE);
		_rmsgstub.msgbundle.message_flags |= Message::DoneSendFlagE;
		
		_rmsgstub.serializer_ptr = nullptr;//free some memory
		
		vdbgx(Debug::ipc, MessageWriterPrintPairT(*this, PrintInnerListsE));
		
		if(not Message::is_waiting_response(_rmsgstub.msgbundle.message_flags)){
			//no wait response for the message - complete
			ErrorConditionT		err;
			MessagePointerT 	msgptr;
			
			doCompleteMessage(msgptr, requid, _rconfig, _ridmap, _rctx, err);
			vdbgx(Debug::ipc, MessageWriterPrintPairT(*this, PrintInnerListsE));
		}
		
		doTryMoveMessageFromPendingToWriteQueue(_rconfig);
		vdbgx(Debug::ipc, MessageWriterPrintPairT(*this, PrintInnerListsE));
	}else{
		//message not done - packet should be full
		++_rmsgstub.packet_count;
		
		if(_rmsgstub.packet_count >= _rconfig.max_writer_message_continuous_packet_count){
			_rmsgstub.packet_count = 0;
			sending_inner_list.popFront();
			sending_inner_list.pushBack(_msgidx);
			vdbgx(Debug::ipc, MessageWriterPrintPairT(*this, PrintInnerListsE));
		}
	}
}
//-----------------------------------------------------------------------------
PacketHeader::Types MessageWriter::doPrepareMessageForSending(
	MessageStub &_rmsgstub, ipc::Configuration const &_rconfig,
	TypeIdMapT const & _ridmap,
	ConnectionContext &_rctx,
	SerializerPointerT &_rtmp_serializer
){
	PacketHeader::Types		msgswitch;// = PacketHeader::ContinuedMessageTypeE;
	
	if(not _rmsgstub.serializer_ptr){
		
		//switch to new message
		msgswitch = PacketHeader::SwitchToNewMessageTypeE;
		
		if(_rtmp_serializer){
			_rmsgstub.serializer_ptr = std::move(_rtmp_serializer);
		}else{
			_rmsgstub.serializer_ptr = std::move(SerializerPointerT(new Serializer(_ridmap)));
		}
		
		_rmsgstub.msgbundle.message_flags |= Message::StartedSendFlagE;
		
		_rconfig.reset_serializer_limits_fnc(_rctx, _rmsgstub.serializer_ptr->limits());
		
		_rmsgstub.serializer_ptr->push(_rmsgstub.msgbundle.message_ptr, _rmsgstub.msgbundle.message_type_id, "message");
		
		bool 		rv = compute_value_with_crc(current_message_type_id, _rmsgstub.msgbundle.message_type_id);
		cassert(rv);(void)rv;
		
		//Not sending by value (pushCrossValue), in order to avoid an unnecessary 
		//"ext" data allocation in serializer.
		_rmsgstub.serializer_ptr->pushCross(current_message_type_id, "message_type_id");
		
	}else if(Message::is_canceled(_rmsgstub.msgbundle.message_flags)){
		
		if(_rmsgstub.packet_count == 0){
			msgswitch = PacketHeader::SwitchToOldCanceledMessageTypeE;
		}else{
			msgswitch = PacketHeader::ContinuedCanceledMessageTypeE;
		}
	
		
	}else if(_rmsgstub.packet_count == 0){
		
		//switch to old message
		msgswitch = PacketHeader::PacketHeader::SwitchToOldMessageTypeE;
	
	}else{
		
		//continued message
		msgswitch = PacketHeader::PacketHeader::ContinuedMessageTypeE;
	
	}
	return msgswitch;
}
//-----------------------------------------------------------------------------
void MessageWriter::doTryMoveMessageFromPendingToWriteQueue(ipc::Configuration const &_rconfig){
	if(
		pending_inner_list.empty()
	) return;
	{
		const size_t	msgidx = pending_inner_list.frontIndex();
		MessageStub		&rmsgstub(message_vec[msgidx]);
		
		if(
			(
				not rmsgstub.msgbundle.message_ptr.empty() and (
					Message::is_asynchronous(rmsgstub.msgbundle.message_flags) or
					not isSynchronousInSendingQueue()
				)
			) or
			(
				rmsgstub.msgbundle.message_ptr.empty() and
				sending_inner_list.empty()
			)
		){
			
			pending_inner_list.popFront();
			sending_inner_list.pushBack(msgidx);
			
			rmsgstub.inner_status = InnerStatusSending;
			
			if(Message::is_synchronous(rmsgstub.msgbundle.message_flags)){
				this->flags |= SynchronousMessageInSendingQueueFlag;
			}
			return;
		}
	}
	
	if(isAsynchronousInPendingQueue()){
		//worst case - there is an Synchronous Message in the write queue
		//while the first message in pending queue is Synchronous
		//we must see if there are any asynchronous message to move to writequeue
		
		//For that:
		//we rotate the queue so that the currently Synchronous Message will still be at front
		
		size_t				qsz = pending_inner_list.size();//pending_message_q.size();
		size_t				async_msg_idx = InvalidIndex();
		bool				has_another_asyncmsgstub = false;
		
		while(qsz--){
			size_t			msgidx = pending_inner_list.frontIndex();
			
			MessageStub		&rmsgstub(message_vec[msgidx]);
			
			pending_inner_list.popFront();
			
			if(Message::is_synchronous(rmsgstub.msgbundle.message_flags) or rmsgstub.msgbundle.message_ptr.empty()){
				pending_inner_list.pushBack(msgidx);
			}else{
				if(async_msg_idx != InvalidIndex()){
					has_another_asyncmsgstub = true;
					pending_inner_list.pushBack(msgidx);
				}else{
					async_msg_idx = msgidx;
				}
			}
		}
		
		if(has_another_asyncmsgstub){
			this->flags |= AsynchronousMessageInPendingQueueFlag;
		}else{
			this->flags &= ~AsynchronousMessageInPendingQueueFlag;
		}
		
		if(async_msg_idx != InvalidIndex()){
			//we have an asynchronous message
			sending_inner_list.pushBack(async_msg_idx);
			
			MessageStub		&rmsgstub(message_vec[async_msg_idx]);
		
			rmsgstub.inner_status = InnerStatusSending;
		}
	}
}
//-----------------------------------------------------------------------------
void MessageWriter::completeMessage(
	MessagePointerT &_rmsgptr,
	MessageUid const &_rmsguid,
	ipc::Configuration const &_rconfig,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx,
	ErrorConditionT const & _rerror
){
	doCompleteMessage(_rmsgptr, _rmsguid, _rconfig, _ridmap, _rctx, _rerror);
	doTryMoveMessageFromPendingToWriteQueue(_rconfig);
}
//-----------------------------------------------------------------------------
void MessageWriter::doCompleteMessage(
	MessagePointerT &_rmsgptr,
	MessageUid const &_rmsguid,
	ipc::Configuration const &_rconfig,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx,
	ErrorConditionT const & _rerror
){
	vdbgx(Debug::ipc, _rmsguid);
	if(_rmsguid.index < message_vec.size() and _rmsguid.unique == message_vec[_rmsguid.index].unique){
		//we have the message
		const size_t			msgidx = _rmsguid.index;
		MessageStub				&rmsgstub = message_vec[msgidx];
		
		cassert(not rmsgstub.serializer_ptr);
		cassert(not rmsgstub.msgbundle.message_ptr.empty());
		
		_rctx.message_flags = rmsgstub.msgbundle.message_flags;
		
		if(not FUNCTION_EMPTY(rmsgstub.msgbundle.response_fnc)){
			rmsgstub.msgbundle.response_fnc(_rctx, _rmsgptr, _rerror);
		}
		
		_ridmap[rmsgstub.msgbundle.message_type_id].complete_fnc(_rctx, rmsgstub.msgbundle.message_ptr, _rerror);
		
		order_inner_list.erase(msgidx);
		
		rmsgstub.clear();
		
		cached_inner_list.pushBack(msgidx);
	}
}
//-----------------------------------------------------------------------------
void MessageWriter::completeAllMessages(
	ipc::Configuration const &_rconfig,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx,
	ErrorConditionT const & _rerror
){
	vdbgx(Debug::ipc, "");
	
	while(order_inner_list.size()){
		const size_t	msgidx = order_inner_list.frontIndex();
		MessageStub		&rmsgstub = message_vec[msgidx];
		
		if(not rmsgstub.msgbundle.message_ptr.empty()){
			MessagePointerT 	msgptr;
			RequestUid			requid(msgidx, rmsgstub.unique);
			
			doCompleteMessage(msgptr, requid, _rconfig, _ridmap, _rctx, _rerror);
		}else{
			order_inner_list.popFront();
		}
		
	}
	
	message_vec.clear();
	
	order_inner_list.fastClear();
	pending_inner_list.fastClear();
	sending_inner_list.fastClear();
	cached_inner_list.fastClear();
	
	
}
//-----------------------------------------------------------------------------
void MessageWriter::visitAllMessages(MessageWriterVisitFunctionT const &_rvisit_fnc){
	{//iterate through non completed messages
		size_t 			msgidx = order_inner_list.frontIndex();
		
		while(msgidx != InvalidIndex()){
			MessageStub		&rmsgstub = message_vec[msgidx];
			
			if(rmsgstub.msgbundle.message_ptr.empty()){
				THROW_EXCEPTION_EX("invalid message - something went wrong with the nested queue for message: ", msgidx);
			}
			
			_rvisit_fnc(
				rmsgstub.msgbundle.message_ptr,
				rmsgstub.msgbundle.message_type_id,
				rmsgstub.msgbundle.response_fnc,
				rmsgstub.msgbundle.message_flags
			);
			
			if(rmsgstub.msgbundle.message_ptr.empty()){
				if(rmsgstub.inner_status == InnerStatusPending){
					pending_inner_list.erase(msgidx);
				}else if(rmsgstub.inner_status == InnerStatusSending){
					sending_inner_list.erase(msgidx);
				}else{
					cassert(false);
				}
				
				rmsgstub.clear();
				cached_inner_list.pushBack(msgidx);
			}
			
			msgidx = order_inner_list.previousIndex(msgidx);
		}
	}
}
//-----------------------------------------------------------------------------
void MessageWriter::print(std::ostream &_ros, const PrintWhat _what)const{
	if(_what == PrintInnerListsE){
		_ros<<"InnerLists: ";
		auto print_fnc = [&_ros](const size_t _idx, MessageStub const &){_ros<<_idx<<' ';};
		_ros<<"OrderList: ";
		order_inner_list.forEach(print_fnc);
		_ros<<'\t';
		
		_ros<<"PendingList: ";
		pending_inner_list.forEach(print_fnc);
		_ros<<'\t';
		
		_ros<<"SendingList: ";
		sending_inner_list.forEach(print_fnc);
		_ros<<'\t';
		
		_ros<<"CacheList: ";
		cached_inner_list.forEach(print_fnc);
		_ros<<'\t';
	}
}
//-----------------------------------------------------------------------------
std::ostream& operator<<(std::ostream &_ros, std::pair<MessageWriter const&, MessageWriter::PrintWhat> const &_msgwriterpair){
	_msgwriterpair.first.print(_ros, _msgwriterpair.second);
	return _ros;
}
//-----------------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
