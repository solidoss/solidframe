// frame/aio/aiocommon.hpp
//
// Copyright (c) 2013, 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_AIO2_COMMON_HPP
#define SOLID_FRAME_AIO2_COMMON_HPP

#include "frame/common.hpp"

namespace solid{
namespace frame{
namespace aio{

enum ReactorEventsE{
	ReactorEventRecv = 1,
	ReactorEventSend = 2,
	ReactorEventRecvSend = ReactorEventRecv | ReactorEventSend,
	ReactorEventError,
	ReactorEventClear,
	ReactorEventInit,
};

enum ReactorWaitRequestsE{
	ReactorWaitRead,
	ReactorWaitWrite,
	ReactorWaitReadOrWrite
};

}//namespace aio
}//namespace frame
}//namespace solid


#endif