
#pragma once

#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcmessage.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v4.hpp"

struct Register : solid::frame::mprpc::Message {
    uint32_t group_id_   = 0;
    uint16_t replica_id_ = 0;

    Register() {}

    Register(uint32_t _group_id, uint16_t _replica_id = 0)
        : group_id_(_group_id)
        , replica_id_(_replica_id)
    {
    }

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("group_id", _rthis.group_id_), _rctx);
        _rr(id<2>, make("replica_id", _rthis.replica_id_), _rctx);
    }
};
