
#pragma once
#include "solid/frame/mpipc/mpipccontext.hpp"
#include "solid/frame/mpipc/mpipcmessage.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v2.hpp"
#include "solid/system/common.hpp"

namespace ipc_echo {

struct Message : solid::frame::mpipc::Message {
    std::string str;

    Message() {}

    Message(std::string&& _ustr)
        : str(std::move(_ustr))
    {
    }

    template <class S>
    void solidSerializeV1(S& _s, solid::frame::mpipc::ConnectionContext& _rctx)
    {
        _s.push(str, "str");
    }

    template <class S>
    void solidSerializeV2(S& _s, solid::frame::mpipc::ConnectionContext& _rctx, const char* _name) const
    {
        _s.add(str, _rctx, "str");
    }
    template <class S>
    void solidSerializeV2(S& _s, solid::frame::mpipc::ConnectionContext& _rctx, const char* _name)
    {
        _s.add(str, _rctx, "str");
    }
};

using TypeId = uint8_t;

using ProtocolT = solid::frame::mpipc::serialization_v2::Protocol<ipc_echo::TypeId>;

template <class R>
inline void protocol_setup(R _r, ProtocolT& _rproto)
{
    _rproto.null(static_cast<TypeId>(0));

    _r(_rproto, solid::TypeToType<Message>(), 1);
}

} //namespace ipc_echo
