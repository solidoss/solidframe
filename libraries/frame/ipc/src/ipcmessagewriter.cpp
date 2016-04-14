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
	order_inner_list(message_vec),
	write_inner_list(message_vec), cache_inner_list(message_vec){
}
//-----------------------------------------------------------------------------
MessageWriter::~MessageWriter(){}
//-----------------------------------------------------------------------------
void MessageWriter::prepare(WriterConfiguration const &_rconfig){
	
}
//-----------------------------------------------------------------------------
void MessageWriter::unprepare(){
	
}
//-----------------------------------------------------------------------------
bool MessageWriter::enqueue(
	WriterConfiguration const &_rconfig,
	MessageBundle &_rmsgbundle,
	MessageId const &_rpool_msg_id,
	MessageId &_rconn_msg_id
){
	//see if we can accept the message
	if(write_inner_list.size() >= _rconfig.max_message_count_multiplex){
		return false;
	}
	
	if(
		Message::is_waiting_response(_rmsgbundle.message_flags) and
		order_inner_list.size() >= (_rconfig.max_message_count_multiplex + _rconfig.max_message_count_response_wait)
	){
		return false;
	}
	
	//creal all disrupting flags
	_rmsgbundle.message_flags &= ~Message::StartedSendFlagE;
	_rmsgbundle.message_flags &= ~Message::WaitResponseFlagE;
	
	size_t		idx;
	
	if(cache_inner_list.size()){
		idx = cache_inner_list.popFront();
	}else{
		idx = message_vec.size();
		message_vec.push_back(MessageStub());
	}
	
	MessageStub 	&rmsgstub(message_vec[idx]);
	
	rmsgstub.msgbundle = std::move(_rmsgbundle);
	rmsgstub.pool_msg_id = _rpool_msg_id;
	
	_rconn_msg_id = MessageId(idx, rmsgstub.unique);
	
	order_inner_list.pushBack(idx);
	write_inner_list.pushBack(idx);
	
	return true;
}
//-----------------------------------------------------------------------------
void MessageWriter::doUnprepareMessageStub(const size_t _msgidx){
	MessageStub		&rmsgstub(message_vec[_msgidx]);
	rmsgstub.clear();
	cache_inner_list.pushFront(_msgidx);
}
//-----------------------------------------------------------------------------
bool MessageWriter::cancel(
	MessageId const &_rmsguid,
	MessageBundle &_rmsgbundle,
	MessageId &_rpool_msg_id
){
	if(_rmsguid.index < message_vec.size() and _rmsguid.unique == message_vec[_rmsguid.index].unique){
		return doCancel(_rmsguid.index, _rmsgbundle, _rpool_msg_id);
	}
	
	return false;
}
//-----------------------------------------------------------------------------
bool MessageWriter::cancelOldest(
	MessageBundle &_rmsgbundle,
	MessageId &_rpool_msg_id
){
	if(order_inner_list.size()){
		return doCancel(order_inner_list.frontIndex(), _rmsgbundle, _rpool_msg_id);
	}
	return false;
}
//-----------------------------------------------------------------------------
bool MessageWriter::doCancel(
	const size_t _msgidx,
	MessageBundle &_rmsgbundle,
	MessageId &_rpool_msg_id
){
	MessageStub		&rmsgstub = order_inner_list.front();
	
	if(Message::is_canceled(rmsgstub.msgbundle.message_flags)){
		return false;//already canceled
	}
	
	rmsgstub.msgbundle.message_flags |= Message::CanceledFlagE;
	
	_rmsgbundle = std::move(rmsgstub.msgbundle);
	_rpool_msg_id = rmsgstub.pool_msg_id;
	
	order_inner_list.erase(_msgidx);
	
	if(rmsgstub.serializer_ptr.get()){
		//the message is being sent
		rmsgstub.serializer_ptr->clear();
	}else if(Message::is_waiting_response(rmsgstub.msgbundle.message_flags)){
		//message is waiting response
		doUnprepareMessageStub(_msgidx);
	}else{
		//message is waiting to be sent
		write_inner_list.erase(_msgidx);
		doUnprepareMessageStub(_msgidx);
	}
	
	return true;
}
//-----------------------------------------------------------------------------
bool MessageWriter::empty()const{
	return order_inner_list.empty();
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
	CompleteFunctionT &_complete_fnc,
	WriterConfiguration const &_rconfig,
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
		char			*pbuftmp = doFillPacket(pbufdata, pbufend, packet_options, more, _complete_fnc, _rconfig, _ridmap, _rctx, _rerror);
		
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
	CompleteFunctionT &_complete_fnc,
	WriterConfiguration const &_rconfig,
	TypeIdMapT const & _ridmap,
	ConnectionContext &_rctx,
	ErrorConditionT & _rerror
){
	char 					*pbufpos = _pbufbeg;
	uint32					freesz = _pbufend - pbufpos;
	
	SerializerPointerT		tmp_serializer;
	size_t					packet_message_count = 0;
	while(write_inner_list.size() and freesz >= MinimumFreePacketDataSize){
		
		const size_t			msgidx = write_inner_list.frontIndex();
		
		MessageStub				&rmsgstub = message_vec[msgidx];
		PacketHeader::Types		msgswitch;// = PacketHeader::ContinuedMessageTypeE;
		
		if(rmsgstub.isStop()){
			//we need to stop
			if(pbufpos == _pbufbeg){
				_rerror = error_connection_delayed_closed;
				pbufpos = nullptr;
				write_inner_list.popFront();
			}
			_rmore = false;
			break;
		}
		
		msgswitch = doPrepareMessageForSending(msgidx, _rconfig, _ridmap, _rctx, tmp_serializer);
		
		if(packet_message_count == 0){
			//first message in the packet
			_rpacket_options.packet_type = msgswitch;
		}else{
			uint8	tmp = static_cast<uint8>(msgswitch);
			pbufpos = SerializerT::storeValue(pbufpos, tmp);
		}
		
		++packet_message_count;
		
		if(rmsgstub.isCanceled()){
			//message already completed - just drop it write list
			write_inner_list.erase(msgidx);
			doUnprepareMessageStub(msgidx);
			continue;
		}
		
		if(not rmsgstub.msgbundle.message_ptr->isOnPeer()){
			//on sender
			_rctx.request_id.index  = msgidx;
			_rctx.request_id.unique = rmsgstub.unique;
		}else{
			_rctx.request_id = rmsgstub.msgbundle.message_ptr->requestId();
		}
		
		_rctx.message_state = rmsgstub.msgbundle.message_ptr->state() + 1;
		
		
		const int rv = rmsgstub.serializer_ptr->run(pbufpos, _pbufend - pbufpos, _rctx);
		
		if(rv > 0){
			
			pbufpos += rv;
			freesz -= rv;
			
			doTryCompleteMessageAfterSerialization(msgidx, _complete_fnc, _rconfig, _ridmap, _rctx, tmp_serializer, _rerror);
			
			if(_rerror){
				pbufpos = nullptr;
				break;
			}
			
		}else{
			_rerror = rmsgstub.serializer_ptr->error();
			pbufpos = nullptr;
			break;
		}
	}
	
	vdbgx(Debug::ipc, "write_q_size "<<write_inner_list.size()<<" order_q_size "<<order_inner_list.size());
	
	return pbufpos;
}
//-----------------------------------------------------------------------------
void MessageWriter::doTryCompleteMessageAfterSerialization(
	const size_t _msgidx,
	CompleteFunctionT &_complete_fnc,
	WriterConfiguration const &_rconfig,
	TypeIdMapT const & _ridmap,
	ConnectionContext &_rctx,
	SerializerPointerT &_rtmp_serializer,
	ErrorConditionT & _rerror
){
	
	MessageStub &rmsgstub(message_vec[_msgidx]);
	
	if(rmsgstub.serializer_ptr->empty()){
		RequestId		requid(_msgidx, rmsgstub.unique);
		vdbgx(Debug::ipc, "done serializing message "<<requid<<". Message id sent to client "<<_rctx.request_id);
		_rtmp_serializer = std::move(rmsgstub.serializer_ptr);
		//done serializing the message:
		
		write_inner_list.popFront();
		
		rmsgstub.msgbundle.message_flags &= (~Message::StartedSendFlagE);
		rmsgstub.msgbundle.message_flags |= Message::DoneSendFlagE;
		
		rmsgstub.serializer_ptr = nullptr;//free some memory
		
		vdbgx(Debug::ipc, MessageWriterPrintPairT(*this, PrintInnerListsE));
		
		if(not Message::is_waiting_response(rmsgstub.msgbundle.message_flags)){
			//no wait response for the message - complete
			MessageBundle	tmp_msg_bundle(std::move(rmsgstub.msgbundle));
			MessageId		tmp_pool_msg_id(rmsgstub.pool_msg_id);
			
			order_inner_list.erase(_msgidx);
			doUnprepareMessageStub(_msgidx);
			
			_rerror = _complete_fnc(tmp_msg_bundle, tmp_pool_msg_id);
			
			vdbgx(Debug::ipc, MessageWriterPrintPairT(*this, PrintInnerListsE));
		}else{
			rmsgstub.msgbundle.message_flags |= Message::WaitResponseFlagE;
		}
		
		vdbgx(Debug::ipc, MessageWriterPrintPairT(*this, PrintInnerListsE));
	}else{
		//message not done - packet should be full
		++rmsgstub.packet_count;
		
		if(rmsgstub.packet_count >= _rconfig.max_message_continuous_packet_count){
			rmsgstub.packet_count = 0;
			write_inner_list.popFront();
			write_inner_list.pushBack(_msgidx);
			vdbgx(Debug::ipc, MessageWriterPrintPairT(*this, PrintInnerListsE));
		}
	}
}
//-----------------------------------------------------------------------------
PacketHeader::Types MessageWriter::doPrepareMessageForSending(
	const size_t _msgidx,
	WriterConfiguration const &_rconfig,
	TypeIdMapT const & _ridmap,
	ConnectionContext &_rctx,
	SerializerPointerT &_rtmp_serializer
){
	PacketHeader::Types		msgswitch;// = PacketHeader::ContinuedMessageTypeE;
	
	MessageStub				&rmsgstub(message_vec[_msgidx]);
	
	if(not rmsgstub.serializer_ptr){
		
		//switch to new message
		msgswitch = PacketHeader::SwitchToNewMessageTypeE;
		
		if(_rtmp_serializer){
			rmsgstub.serializer_ptr = std::move(_rtmp_serializer);
		}else{
			rmsgstub.serializer_ptr = std::move(SerializerPointerT(new Serializer(_ridmap)));
		}
		
		rmsgstub.msgbundle.message_flags |= Message::StartedSendFlagE;
		
		_rconfig.reset_serializer_limits_fnc(_rctx, rmsgstub.serializer_ptr->limits());
		
		rmsgstub.serializer_ptr->push(rmsgstub.msgbundle.message_ptr, rmsgstub.msgbundle.message_type_id, "message");
		
	}else if(rmsgstub.isCanceled()){
		
		if(rmsgstub.packet_count == 0){
			msgswitch = PacketHeader::SwitchToOldCanceledMessageTypeE;
		}else{
			msgswitch = PacketHeader::ContinuedCanceledMessageTypeE;
		}
	
		
	}else if(rmsgstub.packet_count == 0){
		
		//switch to old message
		msgswitch = PacketHeader::PacketHeader::SwitchToOldMessageTypeE;
	
	}else{
		
		//continued message
		msgswitch = PacketHeader::PacketHeader::ContinuedMessageTypeE;
	
	}
	return msgswitch;
}
//-----------------------------------------------------------------------------
void MessageWriter::forEveryMessagesNewerToOlder(VisitFunctionT const &_rvisit_fnc){
	{//iterate through non completed messages
		size_t 			msgidx = order_inner_list.frontIndex();
		
		while(msgidx != InvalidIndex()){
			MessageStub		&rmsgstub = message_vec[msgidx];
			
			if(rmsgstub.msgbundle.message_ptr.empty()){
				THROW_EXCEPTION_EX("invalid message - something went wrong with the nested queue for message: ", msgidx);
				continue;
			}
			
			const bool	message_in_write_queue = not Message::is_waiting_response(rmsgstub.msgbundle.message_flags);
			
			_rvisit_fnc(
				rmsgstub.msgbundle,
				rmsgstub.pool_msg_id
			);
			
			if(rmsgstub.msgbundle.message_ptr.empty()){
				
				if(message_in_write_queue){
					write_inner_list.erase(msgidx);
				}
				
				const size_t oldidx = msgidx;
				
				msgidx = order_inner_list.previousIndex(oldidx);
				
				order_inner_list.erase(oldidx);
				
				doUnprepareMessageStub(oldidx);
				
			}else{
			
				msgidx = order_inner_list.previousIndex(msgidx);
			}
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
		
		_ros<<"WriteList: ";
		write_inner_list.forEach(print_fnc);
		_ros<<'\t';
		
		_ros<<"CacheList: ";
		cache_inner_list.forEach(print_fnc);
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
