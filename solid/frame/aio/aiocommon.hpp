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

enum struct ReactorEventE : uint32_t {
    None       = 0,
    Recv       = 1,
    Send       = 2,
    RecvSend   = Recv | Send,
    SendRecv   = 4,
    Error      = 8,
    Hangup     = 16,
    OOB        = 32, // receive Out Of Band Data
    OOBSend    = OOB | Send,
    RecvHangup = 64,
    Clear      = 128,
    Init       = 256,
    Timer      = 512,
};

enum ReactorWaitRequestE {
    None = 0,
    Read,
    Write,
    ReadOrWrite,
    User,
    // Add above!
    Error
};

} // namespace aio
} // namespace frame
} // namespace solid
