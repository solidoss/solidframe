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
RelayEngine::~RelayEngine(){
    
}
//-----------------------------------------------------------------------------
void RelayEngine::connectionStop(ConnectionContext& _rctx)
{
}
//-----------------------------------------------------------------------------
void RelayEngine::connectionRegister(ConnectionContext& _rctx, std::string&& _uname)
{
}
//-----------------------------------------------------------------------------
bool RelayEngine::relay(
    ConnectionContext& _rctx,
    MessageHeader&     _rmsghdr,
    RelayData&&        _rrelmsg,
    ObjectIdT&         _rrelay_id,
    ErrorConditionT&   _rerror)
{
    return false;
}
//-----------------------------------------------------------------------------
ErrorConditionT RelayEngine::doPoll(ConnectionContext& _rctx, PushFunctionT& _try_push_fnc, bool& _rmore)
{
    return ErrorConditionT{};
}
//-----------------------------------------------------------------------------
} //namespace mpipc
} //namespace frame
} //namespace solid
