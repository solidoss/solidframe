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
uint16 MessageReader::read(
	const char *_pbuf,
	uint16 _bufsz,
	CompleteFunctionT &_complete_fnc,
	Configuration const &_rconfig,
	TypeIdMapT const &_ridmap,
	ConnectionContext &_rctx,
	ErrorConditionT &_rerror
){
	return 0;
}
//-----------------------------------------------------------------------------

}//namespace ipc
}//namespace frame
}//namespace solid
