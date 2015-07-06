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

namespace solid{
namespace frame{
namespace ipc{


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
		write_q.size() < _rconfig.max_writer_multiplex_message_count or _rmsgptr.empty()
	){
		//put message in write_q
		uint32		idx;
		
		if(cache_stk.size()){
			idx = cache_stk.top();
			cache_stk.pop();
		}else{
			idx = message_vec.size();
			message_vec.push_back(MessageStub()); 
		}
		
		MessageStub		&rmsgstub(message_vec[idx]);
		
		rmsgstub.flags = _flags;
		rmsgstub.msg_type_idx = _msg_type_idx;
		rmsgstub.msgptr = std::move(_rmsgptr);
		
		write_q.push(std::move(WriteStub(idx)));
	}else if(_rconfig.max_writer_pending_message_count == 0 or pending_message_q.size() < _rconfig.max_writer_pending_message_count){
		//put the message in pending queue
		pending_message_q.push(PendingMessageStub(_rmsgptr, _msg_type_idx, _flags));
	}else{
		//fail to enqueue message - complete the message
		ErrorConditionT error;
		error.assign(-1, error.category());//TODO:
		_rctx.messageflags = _flags;
		_ridmap[_msg_type_idx].complete_fnc(_rctx, _rmsgptr, error);
		_rmsgptr.clear();
	}
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
		PacketHeader	packet_header(PacketHeader::NewMessageTypeE, 0, 0);
		PacketOptions	packet_options;
		char			*pbufdata = pbufpos + PacketHeader::SizeOfE;
		char			*pbuftmp = doFillPacket(pbufdata, pbufend, packet_options, more, _rconfig, _ridmap, _rctx, _rerror);
		
		if(not _rerror){
			
			if(not packet_options.force_no_compress){
				pbuftmp = _rconfig.inplace_compress_fnc(pbufdata, pbuftmp - pbufdata);
				if(pbuftmp){
					packet_header.flags |= PacketHeader::CompressedFlagE;
				}
			}
			
			cassert((pbuftmp - pbufpos + PacketHeader::SizeOfE) < static_cast<size_t>(0xffff));
			
			packet_header.type = packet_options.packet_type;
			packet_header.size = pbuftmp - pbufpos + PacketHeader::SizeOfE;
			
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
	char 		*pbufpos = _pbufbeg;
	uint32		freesz = _pbufend - pbufpos;
	while(write_q.size() and freesz >= MinimumFreePacketDataSize){
		WriteStub				&rwritestub = write_q.front();
		MessageStub				&rmsgstub = message_vec[rwritestub.idx];
		PacketHeader::Types		msgswitch = PacketHeader::NewMessageTypeE;
		
		if(not rwritestub.serializer_ptr){
			//new message
		}else if(r
		
		if(pbufpos == _pbufbeg){
			//first message in the packet
			
			_rpacket_options.packet_type = 
		}else{
			
		}
		
	}
	
	return pbufpos;
}
//-----------------------------------------------------------------------------
void MessageWriter::completeMessage(
	MessageUid const &_rmsguid,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx
){
	
}
//-----------------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
