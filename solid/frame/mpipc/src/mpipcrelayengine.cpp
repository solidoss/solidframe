// solid/frame/ipc/src/mpipcrelayengine.cpp
//
// Copyright (c) 2016 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/frame/mpipc/mpipcrelayengine.hpp"
#include "mpipcutility.hpp"

namespace solid {
namespace frame {
namespace mpipc {
//-----------------------------------------------------------------------------
struct RelayEngine::Data {
};
//-----------------------------------------------------------------------------
RelayEngine::RelayEngine()
    : impl_(make_pimpl<Data>())
{
}
//-----------------------------------------------------------------------------
RelayEngine::~RelayEngine()
{
}
//-----------------------------------------------------------------------------
void RelayEngine::connectionStop(const ObjectIdT& _rconuid)
{
}
//-----------------------------------------------------------------------------
void RelayEngine::connectionRegister(const ObjectIdT& _rconuid, std::string&& _uname)
{
}
//-----------------------------------------------------------------------------
bool RelayEngine::relay(
    const ObjectIdT& _rconuid,
    MessageHeader&   _rmsghdr,
    RelayData&&      _rrelmsg,
    MessageId&       _rrelay_id,
    ErrorConditionT& _rerror)
{
    return false;
}
//-----------------------------------------------------------------------------
void RelayEngine::doPoll(const ObjectIdT& _rconuid, PushFunctionT& _try_push_fnc, bool& _rmore)
{
    //TODO:
}
//-----------------------------------------------------------------------------
void RelayEngine::doPoll(const ObjectIdT& _rconuid, PushFunctionT& _try_push_fnc, RelayData* _prelay_data, MessageId const& _rengine_msg_id, bool& _rmore)
{
    //TODO:
}
//-----------------------------------------------------------------------------
} //namespace mpipc
} //namespace frame
} //namespace solid
