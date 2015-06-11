// frame/ipc/src/ipcconfiguration.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "frame/ipc/ipcconfiguration.hpp"

namespace solid{
namespace frame{
namespace ipc{
//-----------------------------------------------------------------------------
namespace{
	char * default_allocate_buffer(uint16 &_cp){
		char *rv = new char[4096];
		_cp = 4096;
		return rv;
	}
	void default_free_buffer(char *_pbuf){
		delete []_pbuf;
	}
}//namespace

Configuration::Configuration(
	AioSchedulerT &_rsch
): psch(&_rsch), session_mutex_count(16)
{
	allocate_recv_buffer_fnc = default_allocate_buffer;
	allocate_send_buffer_fnc = default_allocate_buffer;
	
	free_recv_buffer_fnc = default_free_buffer;
	free_send_buffer_fnc = default_free_buffer;
}
//-----------------------------------------------------------------------------
}//namespace ipc
}//namespace frame
}//namespace solid
