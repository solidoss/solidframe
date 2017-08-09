// solid/frame/mpipc/mpipcrelayengine.hpp
//
// Copyright (c) 2017 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/system/error.hpp"
#include "solid/system/pimpl.hpp"

namespace solid {
namespace frame {
namespace mpipc {

struct ConnectionContext;

struct RelayStub {
    RelayData  data_;
    RelayStub* pnext;
};

class RelayEngine : public RelayEngineBase {
public:
    RelayEngine();

    void connectionStop(ConnectionContext& _rctx);

    void connectionRegister(ConnectionContext& _rctx, std::string&& _uname);

private:
    bool relay(
        ConnectionContext& _rctx,
        MessageHeader&     _rmsghdr,
        RelayData&&        _rrelmsg,
        ObjectIdT&         _rrelay_id,
        const bool         _is_last,
        ErrorConditionT&   _rerror) override;

    ErrorConditionT doPoll(ConnectionContext& _rctx, PushFunctionT& _try_push_fnc, bool& _rmore) override;

private:
    struct Data;
    PimplT<Data> impl_;
};

} //namespace mpipc
} //namespace frame
} //namespace solid
