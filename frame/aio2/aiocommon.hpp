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
#include "boost/function.hpp"

namespace solid{
namespace frame{
struct Event;

namespace aio{

enum ReactorEventsE{
	ReactorEventNone = 0,
	ReactorEventRecv = 1,
	ReactorEventSend = 2,
	ReactorEventRecvSend = ReactorEventRecv | ReactorEventSend,
	ReactorEventError = 4,
	ReactorEventHangup = 8,
	ReactorEventOOB = 16,//receive Out Of Band Data
	ReactorEventOOBSend = ReactorEventOOB | ReactorEventSend,
	ReactorEventRecvHangup = 32,
	ReactorEventClear = 64,
	ReactorEventInit = 128,
};

enum ReactorWaitRequestsE{
	ReactorWaitNone = 0,
	ReactorWaitRead,
	ReactorWaitWrite,
	ReactorWaitReadOrWrite
};


class ReactorContext;

typedef boost::function<void(ReactorContext&, Event const &)>		EventFunctionT;

}//namespace aio
}//namespace frame
}//namespace solid


#endif