// solid/frame/ipc/src/mprpcprotocol.cpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/frame/mprpc/mprpcprotocol.hpp"
#include "mprpcutility.hpp"

namespace solid {
namespace frame {
namespace mprpc {
//-----------------------------------------------------------------------------
/*virtual*/ Deserializer::~Deserializer() {}
//-----------------------------------------------------------------------------
/*virtual*/ Serializer::~Serializer() {}
//-----------------------------------------------------------------------------
/*virtual*/ Protocol::~Protocol() {}
//-----------------------------------------------------------------------------
bool PacketHeader::isOk() const
{
    bool rv = true;
    switch (static_cast<TypeE>(type_)) {
    case TypeE::Data:
    case TypeE::KeepAlive:
        break;
    default:
        rv = false;
        break;
    }

    if (size() > Protocol::MaxPacketDataSize) {
        rv = false;
    }

    return rv;
}
//-----------------------------------------------------------------------------
} //namespace mprpc
} //namespace frame
} //namespace solid
