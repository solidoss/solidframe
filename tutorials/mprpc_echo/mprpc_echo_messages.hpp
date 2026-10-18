
#pragma once
#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcmessage.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v4.hpp"
#include "solid/system/common.hpp"

namespace rpc_echo {

struct Message : solid::frame::mprpc::Message {
    std::string str;

    Message() {}

    Message(std::string&& _ustr)
        : str(std::move(_ustr))
    {
    }

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("str", _rthis.str), _rctx);
    }
};

template <class Reg>
inline void configure_protocol(Reg _rreg)
{
    _rreg(1, "Message", std::type_identity<Message>());
}

} // namespace rpc_echo
