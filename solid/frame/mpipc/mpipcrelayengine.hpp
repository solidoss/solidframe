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

struct RelayStub {
    RelayData  data_;
    RelayStub* pnext;
};

class RelayEngine : public RelayEngineBase {
public:
    RelayEngine();
    ~RelayEngine();

    void connectionStop(const ObjectIdT& _rconuid);

    void connectionRegister(const ObjectIdT& _rconuid, std::string&& _uname);

private:
    bool relay(
        const ObjectIdT& _rconuid,
        MessageHeader&   _rmsghdr,
        RelayData&&      _rrelmsg,
        MessageId&       _rrelay_id,
        ErrorConditionT& _rerror) override;

    void doPoll(const ObjectIdT& _rconuid, PushFunctionT& _try_push_fnc, bool& _rmore) override;
    void doPoll(const ObjectIdT& _rconuid, PushFunctionT& _try_push_fnc, RelayData* _prelay_data, MessageId const& _rengine_msg_id, bool& _rmore) override;

private:
    struct Data;
    PimplT<Data> impl_;
};

} //namespace mpipc
} //namespace frame
} //namespace solid
