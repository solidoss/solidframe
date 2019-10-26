
#pragma once

#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcmessage.hpp"

#include "../common.hpp"

namespace alpha_protocol {

struct FirstMessage : solid::frame::mprpc::Message {
    uint32_t    v;
    std::string str;

    FirstMessage() {}
    FirstMessage(uint32_t _v, std::string&& _str)
        : v(_v)
        , str(std::move(_str))
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.str, _rctx, "str").add(_rthis.v, _rctx, "v");
    }
};

struct SecondMessage : solid::frame::mprpc::Message {
    uint64_t    v;
    std::string str;

    SecondMessage() {}
    SecondMessage(uint64_t _v, std::string&& _str)
        : v(_v)
        , str(std::move(_str))
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.str, _rctx, "str").add(_rthis.v, _rctx, "v");
    }
};

struct ThirdMessage : solid::frame::mprpc::Message {
    uint16_t    v;
    std::string str;

    ThirdMessage() {}
    ThirdMessage(uint16_t _v, std::string&& _str)
        : v(_v)
        , str(std::move(_str))
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.str, _rctx, "str").add(_rthis.v, _rctx, "v");
    }
};

template <class R>
inline void protocol_setup(R _r, ProtocolT& _rproto)
{
    _r(_rproto, solid::TypeToType<FirstMessage>(), TypeIdT(0, 1));
    _r(_rproto, solid::TypeToType<SecondMessage>(), TypeIdT(0, 2));
    _r(_rproto, solid::TypeToType<ThirdMessage>(), TypeIdT(0, 3));
}

} // namespace alpha_protocol
