
#pragma once

#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcmessage.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v3.hpp"

struct Register : solid::frame::mprpc::Message {
    uint32_t group_id_   = 0;
    uint16_t replica_id_ = 0;

    Register() {}

    Register(uint32_t _group_id, uint16_t _replica_id = 0)
        : group_id_(_group_id)
        , replica_id_(_replica_id)
    {
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.group_id_, _rctx, 1, "group_id");
        _rr.add(_rthis.replica_id_, _rctx, 2, "replica_id");
    }
};
