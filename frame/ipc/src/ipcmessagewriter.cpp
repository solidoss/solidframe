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
MessageWriter::MessageWriter(){}
//-----------------------------------------------------------------------------
MessageWriter::~MessageWriter(){}
//-----------------------------------------------------------------------------
void MessageWriter::prepare(Configuration const &_rconfig){
	
}
//-----------------------------------------------------------------------------
void MessageWriter::unprepare(){
	
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
	ulong _flags,
	Configuration const &_rconfig,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx
){
	
	if(
		_rmsgptr.empty() or //always deliver the terminate message
		(
			write_q.size() < _rconfig.max_writer_multiplex_message_count and
			(message_vec.size() - cache_stk.size()) < _rconfig.max_writer_waiting_message_count and
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
		
		if(cache_stk.size()){
			idx = cache_stk.top();
			cache_stk.pop();
		}else{
			idx = message_vec.size();
			message_vec.push_back(MessageStub()); 
		}
		
		MessageStub		&rmsgstub(message_vec[idx]);
		
		rmsgstub.flags = _flags;
		rmsgstub.message_type_idx = _msg_type_idx;
		rmsgstub.message_ptr = std::move(_rmsgptr);
		
		if(Message::is_synchronous(_flags)){
			this->flags |= SynchronousMessageInWriteQueueFlag;
		}
		
		write_q.push(idx);
	}else if(
		pending_message_q.size() < _rconfig.max_writer_pending_message_count
	){
		//put the message in pending queue
		pending_message_q.push(PendingMessageStub(_rmsgptr, _msg_type_idx, _flags));
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
		write_q.empty() or 
		(
			write_q.size() < _rconfig.max_writer_multiplex_message_count and
			message_vec[write_q.front()].packet_count == 0
		)
	);
}
//-----------------------------------------------------------------------------
bool MessageWriter::empty()const{
	return write_q.empty();
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
	
	while(write_q.size() and freesz >= MinimumFreePacketDataSize){
		const size_t			msgidx = write_q.front();
		MessageStub				&rmsgstub = message_vec[msgidx];
		PacketHeader::Types		msgswitch;// = PacketHeader::ContinuedMessageTypeE;
		
		if(not rmsgstub.serializer_ptr){
			//switch to new message
			
			if(rmsgstub.message_ptr.empty()){
				//we need to stop
				_rerror.assign(-1, _rerror.category());//TODO:
				pbufpos = nullptr;
				write_q.pop();
				break;
			}
			
			msgswitch = PacketHeader::SwitchToNewMessageTypeE;
			if(tmp_serializer){
				rmsgstub.serializer_ptr = std::move(tmp_serializer);
			}else{
				rmsgstub.serializer_ptr = std::move(SerializerPointerT(new Serializer(_ridmap)));
			}
			
			_rconfig.reset_serializer_limits_fnc(_rctx, rmsgstub.serializer_ptr->limits());
			
			rmsgstub.serializer_ptr->push(rmsgstub.message_ptr, rmsgstub.message_type_idx, "message");
			
			bool 		rv = compute_value_with_crc(current_message_type_id, rmsgstub.message_type_idx);
			cassert(rv);(void)rv;
			//Not sending by value (pushCrossValue), in order to avoid a unnecessary 
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
			_rctx.message_uid.index  = msgidx;
			_rctx.message_uid.unique = rmsgstub.unique;
		}else{
			_rctx.message_uid = rmsgstub.message_ptr->idOnSender();
		}
		_rctx.message_state = rmsgstub.message_ptr->state() + 1;
		
		
		int rv = rmsgstub.serializer_ptr->run(pbufpos, _pbufend - pbufpos, _rctx);
		
		if(rv > 0){
			pbufpos += rv;
			freesz -= rv;
			
			if(rmsgstub.serializer_ptr->empty()){
				MessageUid		msguid(msgidx, rmsgstub.unique);
				idbgx(Debug::ipc, "done serializing message "<<msguid<<". Message id sent to client "<<_rctx.message_uid);
				tmp_serializer = std::move(rmsgstub.serializer_ptr);
				//done serializing the message:
				write_q.pop();
				
				if(Message::is_synchronous(rmsgstub.flags)){
					this->flags &= ~(SynchronousMessageInWriteQueueFlag);
				}
				
				if(not Message::is_waiting_response(rmsgstub.flags)){
					//no wait response for the message - complete
					ErrorConditionT	err;
					doCompleteMessage(msguid, _rconfig, _ridmap, _rctx, err);
				}
				doTryMoveMessageFromPendingToWriteQueue(_rconfig);
			}else{
				//message not done - packet should be full
				cassert((_pbufend - pbufpos) < MinimumFreePacketDataSize);
				
				++rmsgstub.packet_count;
				
				if(rmsgstub.packet_count >= _rconfig.max_writer_message_continuous_packet_count){
					rmsgstub.packet_count = 0;
					write_q.pop();
					write_q.push(msgidx);
				}
			}
		}else{
			_rerror = rmsgstub.serializer_ptr->error();
			pbufpos = nullptr;
			break;
		}
	}
	idbgx(Debug::ipc, "write_q_size"<<write_q.size());
	return pbufpos;
}
//-----------------------------------------------------------------------------
void MessageWriter::doTryMoveMessageFromPendingToWriteQueue(ipc::Configuration const &_rconfig){
	if(
		pending_message_q.empty() or cache_stk.empty()
	) return;
	
	if(
		(message_vec.size() - cache_stk.size()) == _rconfig.max_writer_waiting_message_count
	) return;
	
	if(
		Message::is_asynchronous(pending_message_q.front().flags) or
		not isSynchronousInWriteQueue()
	){
		const size_t	idx = cache_stk.top();
		MessageStub		&rmsgstub(message_vec[idx]);
		cache_stk.pop();
		
		rmsgstub.flags = pending_message_q.front().flags;
		rmsgstub.message_type_idx = pending_message_q.front().message_type_idx;
		rmsgstub.message_ptr = std::move(pending_message_q.front().message_ptr);
		pending_message_q.pop();
		if(Message::is_synchronous(rmsgstub.flags)){
			this->flags |= SynchronousMessageInWriteQueueFlag;
		}
		write_q.push(idx);
		return;
	}
	
	if(isAsynchronousInPendingQueue()){
		//worst case - there is an Synchronous Message in the write queue
		//while the first message in pending queue is Synchronous
		//we must see if there are any asynchronous message to move to writequeue
		
		//For that:
		//we rotate the queue so that the currently Synchronous Message will still be at front
		
		size_t				qsz = pending_message_q.size();
		PendingMessageStub	asyncmsgstub;
		bool				has_another_asyncmsgstub = false;
		
		while(qsz--){
			PendingMessageStub	tmp(pending_message_q.front());
			
			pending_message_q.pop();
			
			if(Message::is_synchronous(tmp.flags)){
				pending_message_q.push(tmp);
			}else{
				if(asyncmsgstub.message_ptr.get()){
					has_another_asyncmsgstub = true;
					pending_message_q.push(tmp);
				}else{
					asyncmsgstub.message_ptr = std::move(tmp.message_ptr);
					asyncmsgstub.message_type_idx = tmp.message_type_idx;
					asyncmsgstub.flags = tmp.flags;
				}
			}
		}
		
		if(has_another_asyncmsgstub){
			this->flags |= AsynchronousMessageInPendingQueueFlag;
		}else{
			this->flags &= ~AsynchronousMessageInPendingQueueFlag;
		}
		
		if(asyncmsgstub.message_ptr.get()){
			//we have an asynchronous message
			const size_t	idx = cache_stk.top();
			MessageStub		&rmsgstub(message_vec[idx]);
			
			cache_stk.pop();
		
			rmsgstub.flags = asyncmsgstub.flags;
			rmsgstub.message_type_idx = asyncmsgstub.message_type_idx;
			rmsgstub.message_ptr = std::move(asyncmsgstub.message_ptr);
			write_q.push(idx);
		}
	}
}
//-----------------------------------------------------------------------------
void MessageWriter::completeMessage(
	MessageUid const &_rmsguid,
	ipc::Configuration const &_rconfig,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx,
	ErrorConditionT const & _rerror
){
	doCompleteMessage(_rmsguid, _rconfig, _ridmap, _rctx, _rerror);
	doTryMoveMessageFromPendingToWriteQueue(_rconfig);
}
//-----------------------------------------------------------------------------
void MessageWriter::doCompleteMessage(
	MessageUid const &_rmsguid,
	ipc::Configuration const &_rconfig,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx,
	ErrorConditionT const & _rerror
){
	idbgx(Debug::ipc, _rmsguid);
	if(_rmsguid.index < message_vec.size() and _rmsguid.unique == message_vec[_rmsguid.index].unique){
		//we have the message
		const size_t			msgidx = _rmsguid.index;
		MessageStub				&rmsgstub = message_vec[msgidx];
		
		cassert(not rmsgstub.serializer_ptr);
		cassert(not rmsgstub.message_ptr.empty());
		
		_ridmap[rmsgstub.message_type_idx].complete_fnc(_rctx, rmsgstub.message_ptr, _rerror);
		
		rmsgstub.clear();
		cache_stk.push(msgidx);
	}
}
//-----------------------------------------------------------------------------
void MessageWriter::completeAllMessages(
	ipc::Configuration const &_rconfig,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx,
	ErrorConditionT const & _rerror
){
	idbgx(Debug::ipc, "");
	for(auto it = message_vec.begin(); it != message_vec.end(); ++it){
		if(it->message_ptr.empty()){
		}else{
			MessageUid	msguid(it - message_vec.begin(), it->unique);
			doCompleteMessage(msguid, _rconfig, _ridmap, _rctx, _rerror);
		}
	}
	
	if(cache_stk.size()){
	
		size_t msgidx = cache_stk.top();
		doTryMoveMessageFromPendingToWriteQueue(_rconfig);
		
		while(message_vec[msgidx].message_ptr.get()){
			MessageUid	msguid(msgidx, message_vec[msgidx].unique);
			doCompleteMessage(msguid, _rconfig, _ridmap, _rctx, _rerror);
			doTryMoveMessageFromPendingToWriteQueue(_rconfig);
		}
		
	}else{
		cassert(write_q.empty());
	}
	
	while(write_q.size()){
		write_q.pop();
	}
	message_vec.clear();
}
//-----------------------------------------------------------------------------
void MessageWriter::visitAllMessages(MessageWriterVisitFunctionT const &_rvisit_fnc){
	for(auto it = message_vec.begin(); it != message_vec.end(); ++it){
		if(it->message_ptr.empty()){
		}else{
			_rvisit_fnc(it->message_ptr, it->message_type_idx, flags, it->serializer_ptr.get() != nullptr);
			if(it->message_ptr.empty()){
				it->clear();
				//TODO: the message should also be poped from the write_q
				cache_stk.push(it - message_vec.begin());
			}
		}
	}
	
	size_t qsz = pending_message_q.size();
	
	while(qsz--){
		PendingMessageStub	&rms(pending_message_q.front());
		_rvisit_fnc(rms.message_ptr, rms.message_type_idx, rms.flags, false/*not sent*/);
		if(rms.message_ptr.empty()){
			pending_message_q.push(PendingMessageStub(rms.message_ptr, rms.message_type_idx, rms.flags));
		}
		pending_message_q.pop();
	}
}
//-----------------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
