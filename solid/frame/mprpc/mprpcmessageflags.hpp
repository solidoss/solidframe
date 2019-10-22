// solid/frame/mprpc/mprpcmessageflags.hpp
//
// Copyright (c) 2017 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/system/flags.hpp"

namespace solid {
namespace frame {
namespace mprpc {

using MessageFlagsValueT = uint32_t;

enum struct MessageFlagsE : MessageFlagsValueT {
    AwaitResponse,
    Synchronous,
    Idempotent,
    OneShotSend,
    StartedSend,
    DoneSend,
    Canceled,
    Response,
    ResponsePart,
    ResponseLast,
    OnPeer,
    BackOnSender,
    Relayed,
    LastFlag
};

using MessageFlagsT = Flags<MessageFlagsE>;

} //namespace mprpc
} //namespace frame
} //namespace solid
