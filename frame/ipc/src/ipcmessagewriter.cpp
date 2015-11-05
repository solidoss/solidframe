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

#include "system/cassert.hpp"
#include "system/debug.hpp"

namespace solid{
namespace frame{
namespace ipc{
//-----------------------------------------------------------------------------
enum WriterFlags{
	SynchronousMessageInWriteQueueFlag		= 1,
	AsynchronousMessageInPendingQueueFlag	= 2,
};
inline bool MessageWriter::isSynchronousInWriteQueue()const{
	return (flags & SynchronousMessageInWriteQueueFlag) != 0;
}
inline bool MessageWriter::isAsynchronousInPendingQueue()const{
	return (flags & AsynchronousMessageInPendingQueueFlag) != 0;
}

//-----------------------------------------------------------------------------
MessageWriter::MessageWriter():current_message_type_id(InvalidIndex()), flags(0){
	innerListClearAll();
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
MessageUid MessageWriter::safeNewMessageUid(){
	return MessageUid();
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
	MessagePointerT &_rmsgptr,
	const size_t _msg_type_idx,
	ResponseHandlerFunctionT &_rresponse_fnc,
	ulong _flags,
	MessageUid const &_rmsguid,
	Configuration const &_rconfig,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx
){
	
	if(
		_rmsgptr.empty() or //always deliver the terminate message
		(
			inner_list[InnerListSending].size < _rconfig.max_writer_multiplex_message_count and
			(message_vec.size() - inner_list[InnerListCache].size) < _rconfig.max_writer_waiting_message_count and
			(
				Message::is_asynchronous(_flags) or
				(
					Message::is_synchronous(_flags) and not isSynchronousInWriteQueue()
				)
			)	
		)
	){
		//put message in write_q
		uint32			idx;
		
		
		
		
		if(inner_list[InnerListCache].size){
			idx = inner_list[InnerListCache].front;
			innerListPopFront<InnerListCache, InnerLinkStatus>();
		}else{
			idx = message_vec.size();
			message_vec.push_back(MessageStub()); 
		}
		
		MessageStub		&rmsgstub(message_vec[idx]);
		
		rmsgstub.flags = _flags;
		rmsgstub.message_type_idx = _msg_type_idx;
		rmsgstub.message_ptr = std::move(_rmsgptr);
		rmsgstub.response_fnc = std::move(_rresponse_fnc);
		
		
		if(Message::is_synchronous(_flags)){
			this->flags |= SynchronousMessageInWriteQueueFlag;
		}
		
		innerListPushBack<InnerListOrder, InnerLinkOrder>(idx);
		innerListPushBack<InnerListSending, InnerLinkStatus>(idx);
	}else if(
		inner_list[InnerListPending].size < _rconfig.max_writer_pending_message_count
	){
		//put the message in pending queue
		//TODO
		//pending_message_q.push(PendingMessageStub(_rmsgptr, _msg_type_idx, _rresponse_fnc, _flags));
		if(Message::is_asynchronous(_flags)){
			this->flags |= AsynchronousMessageInPendingQueueFlag;
		}
	}else{
		//fail to enqueue message - complete the message
		ErrorConditionT error;
		error.assign(-1, error.category());//TODO:
		_rctx.message_flags = _flags;
		_ridmap[_msg_type_idx].complete_fnc(_rctx, _rmsgptr, error);
		_rmsgptr.clear();
	}
}
//-----------------------------------------------------------------------------
bool MessageWriter::shouldTryFetchNewMessage(Configuration const &_rconfig)const{
	return (
		inner_list[InnerListSending].empty() or 
		(
			inner_list[InnerListSending].size < _rconfig.max_writer_multiplex_message_count and
			message_vec[inner_list[InnerListSending].front].packet_count == 0
		)
	);
}
//-----------------------------------------------------------------------------
bool MessageWriter::empty()const{
	return inner_list[InnerListSending].empty();
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
	
	while(inner_list[InnerListSending].size and freesz >= MinimumFreePacketDataSize){
		const size_t			msgidx = inner_list[InnerListSending].front;
		MessageStub				&rmsgstub = message_vec[msgidx];
		PacketHeader::Types		msgswitch;// = PacketHeader::ContinuedMessageTypeE;
		
		if(rmsgstub.message_ptr.empty()){
			//we need to stop
			if(pbufpos == _pbufbeg){
				_rerror.assign(-1, _rerror.category());//TODO:
				pbufpos = nullptr;
				innerListPopFront<InnerListSending, InnerLinkStatus>();
			}
			_rmore = false;
			break;
		}
		
		if(not rmsgstub.serializer_ptr){
			
			//switch to new message
			msgswitch = PacketHeader::SwitchToNewMessageTypeE;
			
			if(tmp_serializer){
				rmsgstub.serializer_ptr = std::move(tmp_serializer);
			}else{
				rmsgstub.serializer_ptr = std::move(SerializerPointerT(new Serializer(_ridmap)));
			}
			
			rmsgstub.flags |= Message::StartedSendFlagE;
			
			_rconfig.reset_serializer_limits_fnc(_rctx, rmsgstub.serializer_ptr->limits());
			
			rmsgstub.serializer_ptr->push(rmsgstub.message_ptr, rmsgstub.message_type_idx, "message");
			
			bool 		rv = compute_value_with_crc(current_message_type_id, rmsgstub.message_type_idx);
			cassert(rv);(void)rv;
			//Not sending by value (pushCrossValue), in order to avoid an unnecessary 
			//"ext" data allocation in serializer.
			rmsgstub.serializer_ptr->pushCross(current_message_type_id, "message_type_id");
			
		}else if(rmsgstub.packet_count == 0){
			
			//switch to old message
			msgswitch = PacketHeader::PacketHeader::SwitchToOldMessageTypeE;
		
			
		}else{
			
			//continued message
			msgswitch = PacketHeader::PacketHeader::ContinuedMessageTypeE;
		
		}
		
		if(pbufpos == _pbufbeg){
			//first message in the packet
			_rpacket_options.packet_type = msgswitch;
		}else{
			uint8	tmp = static_cast<uint8>(msgswitch);
			pbufpos = SerializerT::storeValue(pbufpos, tmp);
		}
		
		if(not rmsgstub.message_ptr->isOnPeer()){
			_rctx.request_uid.index  = msgidx;
			_rctx.request_uid.unique = rmsgstub.unique;
		}else{
			_rctx.request_uid = rmsgstub.message_ptr->requestId();
		}
		_rctx.message_state = rmsgstub.message_ptr->state() + 1;
		
		
		int rv = rmsgstub.serializer_ptr->run(pbufpos, _pbufend - pbufpos, _rctx);
		
		if(rv > 0){
			pbufpos += rv;
			freesz -= rv;
			
			if(rmsgstub.serializer_ptr->empty()){
				RequestUid		requid(msgidx, rmsgstub.unique);
				vdbgx(Debug::ipc, "done serializing message "<<requid<<". Message id sent to client "<<_rctx.request_uid);
				tmp_serializer = std::move(rmsgstub.serializer_ptr);
				//done serializing the message:
				
				innerListPopFront<InnerListSending, InnerLinkStatus>();
				
				if(Message::is_synchronous(rmsgstub.flags)){
					this->flags &= ~(SynchronousMessageInWriteQueueFlag);
				}
				
				rmsgstub.flags &= (~Message::StartedSendFlagE);
				rmsgstub.flags |= Message::DoneSendFlagE;
				
				rmsgstub.serializer_ptr = nullptr;//free some memory
				
				if(not Message::is_waiting_response(rmsgstub.flags)){
					//no wait response for the message - complete
					ErrorConditionT		err;
					MessagePointerT 	msgptr;
					
					doCompleteMessage(msgptr, requid, _rconfig, _ridmap, _rctx, err);
				}
				
				doTryMoveMessageFromPendingToWriteQueue(_rconfig);
			}else{
				//message not done - packet should be full
				cassert((_pbufend - pbufpos) < MinimumFreePacketDataSize);
				
				++rmsgstub.packet_count;
				
				if(rmsgstub.packet_count >= _rconfig.max_writer_message_continuous_packet_count){
					rmsgstub.packet_count = 0;
					innerListPopFront<InnerListSending, InnerLinkStatus>();
					innerListPushBack<InnerListSending, InnerLinkStatus>(msgidx);
				}
			}
		}else{
			_rerror = rmsgstub.serializer_ptr->error();
			pbufpos = nullptr;
			break;
		}
	}
	vdbgx(Debug::ipc, "write_q_size "<<inner_list[InnerListSending].size<<" pending_q_size "<<inner_list[InnerListPending].size);
	return pbufpos;
}
//-----------------------------------------------------------------------------
void MessageWriter::doTryMoveMessageFromPendingToWriteQueue(ipc::Configuration const &_rconfig){
	if(
		inner_list[InnerListPending].empty() or inner_list[InnerListCache].empty()
	) return;
	
	if(
		(message_vec.size() - inner_list[InnerListCache].size) == _rconfig.max_writer_waiting_message_count
	) return;
	
	if(
		//Message::is_asynchronous(pending_message_q.front().flags) or
		Message::is_asynchronous(message_vec[inner_list[InnerListPending].front].flags) or
		not isSynchronousInWriteQueue()
	){
		const size_t	msgidx = inner_list[InnerListPending].front;
		
		innerListPopFront<InnerListPending, InnerLinkStatus>();
		innerListPushBack<InnerListSending, InnerLinkStatus>(msgidx);
		
		MessageStub		&rmsgstub(message_vec[msgidx]);

		if(Message::is_synchronous(rmsgstub.flags)){
			this->flags |= SynchronousMessageInWriteQueueFlag;
		}
		
		innerListPushBack<InnerListOrder, InnerLinkOrder>(msgidx);
		return;
	}
	
	if(isAsynchronousInPendingQueue()){
		//worst case - there is an Synchronous Message in the write queue
		//while the first message in pending queue is Synchronous
		//we must see if there are any asynchronous message to move to writequeue
		
		//For that:
		//we rotate the queue so that the currently Synchronous Message will still be at front
		
		size_t				qsz = inner_list[InnerListPending].size;//pending_message_q.size();
		size_t				async_msg_idx = InvalidIndex();
		bool				has_another_asyncmsgstub = false;
		
		while(qsz--){
			size_t			msgidx = inner_list[InnerListPending].front;
			
			MessageStub		&rmsgstub(message_vec[msgidx]);
			
			innerListPopFront<InnerListPending, InnerLinkStatus>();
			
			if(Message::is_synchronous(rmsgstub.flags)){
				innerListPushBack<InnerListPending, InnerLinkStatus>(msgidx);
			}else{
				if(async_msg_idx != InvalidIndex()){
					has_another_asyncmsgstub = true;
					innerListPushBack<InnerListPending, InnerLinkStatus>(msgidx);
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
			innerListPushBack<InnerListSending, InnerLinkStatus>(async_msg_idx);
			innerListPushBack<InnerListOrder, InnerLinkOrder>(async_msg_idx);
		}
	}
}
//-----------------------------------------------------------------------------
void MessageWriter::completeMessage(
	MessagePointerT &_rmsgptr,
	RequestUid const &_rrequid,
	ipc::Configuration const &_rconfig,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx,
	ErrorConditionT const & _rerror
){
	doCompleteMessage(_rmsgptr, _rrequid, _rconfig, _ridmap, _rctx, _rerror);
	doTryMoveMessageFromPendingToWriteQueue(_rconfig);
}
//-----------------------------------------------------------------------------
void MessageWriter::doCompleteMessage(
	MessagePointerT &_rmsgptr,
	RequestUid const &_rrequid,
	ipc::Configuration const &_rconfig,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx,
	ErrorConditionT const & _rerror
){
	vdbgx(Debug::ipc, _rrequid);
	if(_rrequid.index < message_vec.size() and _rrequid.unique == message_vec[_rrequid.index].unique){
		//we have the message
		const size_t			msgidx = _rrequid.index;
		MessageStub				&rmsgstub = message_vec[msgidx];
		
		cassert(not rmsgstub.serializer_ptr);
		cassert(not rmsgstub.message_ptr.empty());
		
		_rctx.message_flags = rmsgstub.flags;
		
		if(not FUNCTION_EMPTY(rmsgstub.response_fnc)){
			rmsgstub.response_fnc(_rctx, _rmsgptr, _rerror);
		}
		
		_ridmap[rmsgstub.message_type_idx].complete_fnc(_rctx, rmsgstub.message_ptr, _rerror);
		
		innerListErase<InnerListOrder, InnerLinkOrder>(msgidx);
		
		rmsgstub.clear();
		
		innerListPushBack<InnerListSending, InnerLinkStatus>(msgidx);
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
	
	while(inner_list[InnerListOrder].size){
		const size_t	msgidx = inner_list[InnerListOrder].front;
		MessageStub		&rmsgstub = message_vec[msgidx];
		if(not rmsgstub.message_ptr.empty()){
			MessagePointerT 	msgptr;
			RequestUid			requid(msgidx, rmsgstub.unique);
			
			doCompleteMessage(msgptr, requid, _rconfig, _ridmap, _rctx, _rerror);
		}else{
			innerListPopFront<InnerListOrder, InnerLinkOrder>();
		}
		
	}
	
	message_vec.clear();
	
	innerListClearAll();
}
//-----------------------------------------------------------------------------
void MessageWriter::visitAllMessages(MessageWriterVisitFunctionT const &_rvisit_fnc){
	{//iterate through non completed messages
		size_t 			msgidx = inner_list[InnerListOrder].front;
		
		while(msgidx != InvalidIndex()){
			MessageStub		&rmsgstub = message_vec[msgidx];
			
			if(rmsgstub.message_ptr.empty()){
				THROW_EXCEPTION_EX("invalid message - something went wrong with the nested queue for message: ", msgidx);
			}
			
			_rvisit_fnc(rmsgstub.message_ptr, rmsgstub.message_type_idx, rmsgstub.response_fnc, rmsgstub.flags);
			
			if(rmsgstub.message_ptr.empty()){
				if(Message::is_being_sent(rmsgstub.flags)){
					//the message is in the sending queue - pop it
					innerListErase<InnerListSending, InnerLinkStatus>(msgidx);
				}
				
				rmsgstub.clear();
				innerListPushBack<InnerListCache, InnerLinkStatus>(msgidx);
			}
			
			msgidx = rmsgstub.inner_link[InnerLinkOrder].prev;
		}
	}
}
//-----------------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
