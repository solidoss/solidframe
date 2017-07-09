// solid/frame/mpipc/mpipcmessageflags.hpp
//
// Copyright (c) 2017 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/utility/flags.hpp"

namespace solid {
namespace frame {
namespace mpipc {

using MessageFlagsValueT = uint32_t;

enum struct MessageFlagsE : MessageFlagsValueT {
    WaitResponse,
    Synchronous,
    Idempotent,
    OneShotSend,
    StartedSend,
    DoneSend,
    Canceled,
    Response,
    OnPeer,
    BackOnSender,
    LastFlag
};

using MessageFlagsT = Flags<MessageFlagsE>;

} //namespace mpipc
} //namespace frame
} //namespace solid
