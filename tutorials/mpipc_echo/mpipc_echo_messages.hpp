
#pragma once

#include "solid/frame/mpipc/mpipccontext.hpp"
#include "solid/frame/mpipc/mpipcmessage.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v1.hpp"

namespace ipc_echo {

struct Message : solid::frame::mpipc::Message {
    std::string str;

    Message() {}

    Message(std::string&& _ustr)
        : str(std::move(_ustr))
    {
    }

    template <class S>
    void solidSerialize(S& _s, solid::frame::mpipc::ConnectionContext& _rctx)
    {
        _s.push(str, "str");
    }
};

using ProtoSpecT = solid::frame::mpipc::serialization_v1::ProtoSpec<0, Message>;

} //namespace ipc_echo
