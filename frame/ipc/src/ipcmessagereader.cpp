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
MessageReader::MessageReader(){
	
}
//-----------------------------------------------------------------------------
MessageReader::~MessageReader(){
	
}
//-----------------------------------------------------------------------------
void MessageReader::prepare(Configuration const &_rconfig){
	
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
		
		//parse the data
		pbufpos = doConsumePacket(
			pbufpos,
			packet_header,
			_complete_fnc,
			_rconfig,
			_ridmap,
			_rctx,
			_rerror
		);
		
		if(_rerror){
			break;
		}
		state = HeaderReadStateE;
	}
	
	
	return pbufpos - _pbuf;
}
//-----------------------------------------------------------------------------
const char* MessageReader::doConsumePacket(
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
	
	
	return pbufpos;
}

}//namespace ipc
}//namespace frame
}//namespace solid
