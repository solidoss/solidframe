
#pragma once

#include "solid/frame/mpipc/mpipccontext.hpp"
#include "solid/frame/mpipc/mpipcmessage.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v1.hpp"

struct Register : solid::frame::mpipc::Message {
    std::string name;

    Register() {}

    Register(const std::string& _name)
        : name(_name)
    {
    }

    template <class S>
    void solidSerializeV1(S& _s, solid::frame::mpipc::ConnectionContext& _rctx)
    {
        _s.push(name, "name");
    }
};
