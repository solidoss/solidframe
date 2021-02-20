
#pragma once

#include "../common.hpp"
#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcmessage.hpp"

namespace gamma_protocol {

struct FirstMessage : solid::frame::mprpc::Message {
    uint32_t    v;
    std::string str;

    FirstMessage() {}
    FirstMessage(uint32_t _v, std::string&& _str)
        : v(_v)
        , str(std::move(_str))
    {
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.str, _rctx, 1, "str").add(_rthis.v, _rctx, 2, "v");
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

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.str, _rctx, 1, "str").add(_rthis.v, _rctx, 2, "v");
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

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.str, _rctx, 1, "str").add(_rthis.v, _rctx, 2, "v");
    }
};

template <class Reg>
inline void configure_protocol(Reg _rreg)
{
    _rreg({2, 1}, "FirstMessage", solid::TypeToType<FirstMessage>());
    _rreg({2, 2}, "SecondMessage", solid::TypeToType<SecondMessage>());
    _rreg({2, 3}, "ThirdMessage", solid::TypeToType<ThirdMessage>());
}
} // namespace gamma_protocol
