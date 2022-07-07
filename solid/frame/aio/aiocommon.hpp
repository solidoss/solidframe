// solid/frame/aio/aiocommon.hpp
//
// Copyright (c) 2013, 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/common.hpp"

namespace solid {
namespace frame {
namespace aio {

enum ReactorEventsE {
    ReactorEventNone       = 0,
    ReactorEventRecv       = 1,
    ReactorEventSend       = 2,
    ReactorEventRecvSend   = ReactorEventRecv | ReactorEventSend,
    ReactorEventSendRecv   = 4,
    ReactorEventError      = 8,
    ReactorEventHangup     = 16,
    ReactorEventOOB        = 32, // receive Out Of Band Data
    ReactorEventOOBSend    = ReactorEventOOB | ReactorEventSend,
    ReactorEventRecvHangup = 64,
    ReactorEventClear      = 128,
    ReactorEventInit       = 256,
    ReactorEventTimer      = 512,
};

enum ReactorWaitRequestsE {
    ReactorWaitNone = 0,
    ReactorWaitRead,
    ReactorWaitWrite,
    ReactorWaitReadOrWrite,
    ReactorWaitUser,
    // Add above!
    ReactorWaitError
};

} // namespace aio
} // namespace frame
} // namespace solid
