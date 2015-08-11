// frame/ipc/src/ipcmessagereader.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "ipcmessagereader.hpp"
#include "frame/ipc/ipccontext.hpp"
#include "frame/ipc/ipcmessage.hpp"
#include "frame/ipc/ipcconfiguration.hpp"
#include "ipcutility.hpp"
#include "frame/ipc/ipcserialization.hpp"


namespace solid{
namespace frame{
namespace ipc{

//-----------------------------------------------------------------------------
MessageReader::MessageReader():state(HeaderReadStateE){
	
}
//-----------------------------------------------------------------------------
MessageReader::~MessageReader(){
	
}
//-----------------------------------------------------------------------------
void MessageReader::prepare(Configuration const &_rconfig){
	message_q.push(MessageStub());//start with an empty message
}
//-----------------------------------------------------------------------------
void MessageReader::unprepare(){
	
}
//-----------------------------------------------------------------------------
uint32 MessageReader::read(
	const char *_pbuf,
	uint32 _bufsz,
	CompleteFunctionT &_complete_fnc,
	Configuration const &_rconfig,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx,
	ErrorConditionT &_rerror
){
	const char 			*pbufpos = _pbuf;
	const char 			*pbufend = _pbuf + _bufsz;
	PacketHeader		packet_header;
	
	while(pbufpos != pbufend){
		if(state == HeaderReadStateE){
			//try read the header
			if((pbufend - pbufpos) >= PacketHeader::SizeOfE){
				state = DataReadStateE;
			}else{
				break;
			}
		}
		
		if(state == DataReadStateE){
			//try read the data
			const char		*tmpbufpos = packet_header.load<DeserializerT>(pbufpos);
			if((pbufend - tmpbufpos) >= packet_header.size()){
				pbufpos = tmpbufpos;
			}else{
				break;
			}
		}
		
		if(not packet_header.isOk()){
			_rerror.assign(-1, _rerror.category());//TODO:
			break;
		}
		
		if(packet_header.isTypeKeepAlive()){
			MessagePointerT dummy_message_ptr;
			_complete_fnc(KeepaliveCompleteE, dummy_message_ptr);
			continue;
		}
		
		//parse the data
		doConsumePacket(
			pbufpos,
			packet_header,
			_complete_fnc,
			_rconfig,
			_ridmap,
			_rctx,
			_rerror
		);
		
		if(!_rerror){
			pbufpos += packet_header.size();
		}else{
			break;
		}
		state = HeaderReadStateE;
	}
	
	
	return pbufpos - _pbuf;
}
//-----------------------------------------------------------------------------
template </*class S, uint32 I*/>
serialization::binary::CbkReturnValueE 
MessageReader::serializationReinit<DeserializerT, 0>(DeserializerT &_rd, const uint64 &_rv, ConnectionContext &){
	bool 	rv = check_value_with_crc(current_message_type_id, current_message_type_id);
	
	if(!rv){
		return serialization::binary::Failure;
	}
	message_q.front().message_type_idx = current_message_type_id;
	
	_rd.pop();
	
	_rd.push(message_q.front().message_ptr, current_message_type_id, "message");
	
	
	return serialization::binary::Continue;
}
//-----------------------------------------------------------------------------
void MessageReader::doConsumePacket(
	const char *_pbuf,
	PacketHeader const &_packet_header,
	CompleteFunctionT &_complete_fnc,
	Configuration const &_rconfig,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx,
	ErrorConditionT &_rerror
){
	const char 			*pbufpos = _pbuf;
	const char 			*pbufend = _pbuf + _packet_header.size();
	
	
	//decompress
	char				tmpbuf[MaxPacketDataSize];
	
	if(_packet_header.isCompressed()){
		size_t uncompressed_size = _rconfig.uncompress_fnc(tmpbuf, pbufpos, pbufend - pbufpos, _rerror);
		
		if(!_rerror){
			pbufpos = tmpbuf;
			pbufend = tmpbuf + uncompressed_size;
		}else{
			return;
		}
	}
	
	uint8					crt_msg_type = _packet_header.type();
	//DeserializerPointerT	tmp_deserializer;
	
	while(pbufpos < pbufend){
		switch(crt_msg_type){
			case PacketHeader::SwitchToNewMessageTypeE:
				if(message_q.front().message_ptr.get()){
					if(message_q.size() == _rconfig.max_reader_multiplex_message_count){
						cassert(false);
						_rerror.assign(-1, _rerror.category());//TODO:
						return;
					}
					//reschedule the current message for later
					message_q.push(std::move(message_q.front()));
				}
				
				if(not message_q.front().deserializer_ptr.get()){
					message_q.front().deserializer_ptr.reset(new Deserializer(_ridmap));
				}
				
				message_q.front().deserializer_ptr->pushReinit<MessageReader, 0>(this, 1, "message_type_id_reinit");
				message_q.front().deserializer_ptr->pushCross(current_message_type_id, "message_type_id");
				
				break;
			case PacketHeader::SwitchToOldMessageTypeE:
				if(message_q.front().message_ptr.get()){
					if(message_q.size() == _rconfig.max_reader_multiplex_message_count){
						cassert(false);
						_rerror.assign(-1, _rerror.category());//TODO:
						return;
					}
					message_q.push(std::move(message_q.front()));
				}
				message_q.pop();
				break;
			case PacketHeader::ContinuedMessageTypeE:
				cassert(message_q.size() and message_q.front().deserializer_ptr.get() and message_q.front().message_ptr.get());
				break;
			default:
				cassert(false);
				_rerror.assign(-1, _rerror.category());//TODO:
				return;
		}
		
		int rv = message_q.front().deserializer_ptr->run(pbufpos, pbufend - pbufpos, _rctx);
		
		if(rv > 0){
			pbufpos += rv;
			if(message_q.front().deserializer_ptr->empty()){
				//done with the message
				message_q.front().deserializer_ptr->clear();
				
				//complete the message waiting for this response
				_complete_fnc(MessageCompleteE, message_q.front().message_ptr);
				
				//receive the message
				_ridmap[message_q.front().message_type_idx].receive_fnc(_rctx, message_q.front().message_ptr);
				message_q.front().message_ptr.clear();
				message_q.front().message_type_idx = -1;
			}
		}else{
			_rerror = message_q.front().deserializer_ptr->error();
			break;
		}
		
		if(pbufpos < pbufend){
			crt_msg_type = 0;
			pbufpos = DeserializerT::loadValue(pbufpos, crt_msg_type);
		}
	}//while
}

}//namespace ipc
}//namespace frame
}//namespace solid
