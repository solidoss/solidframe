
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

#if 0
    template <class S>
    void solidSerializeV2(S& _s, solid::frame::mpipc::ConnectionContext& _rctx, const char* _name) const
    {
        solidSerializeV2(_s, *this, _rctx, _name);
    }
    template <class S>
    void solidSerializeV2(S& _s, solid::frame::mpipc::ConnectionContext& _rctx, const char* _name)
    {
        solidSerializeV2(_s, *this, _rctx, _name);
    }
    
    template <class S, class T>
    static void solidSerializeV2(S &_s, T &_rt, solid::frame::mpipc::ConnectionContext& _rctx, const char* _name){
        _s.add(_rt.str, _rctx, "str");
    }
#else
    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.str, _rctx, "str");
    }
#endif
};

using ProtocolT = solid::frame::mpipc::serialization_v2::Protocol<uint8_t>;

template <class R>
inline void protocol_setup(R _r, ProtocolT& _rproto)
{
    _rproto.null(static_cast<ProtocolT::TypeIdT>(0));

    _r(_rproto, solid::TypeToType<Message>(), 1);
}

} //namespace ipc_echo
