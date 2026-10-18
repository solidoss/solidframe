
#pragma once

#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcmessage.hpp"

#include "../common.hpp"

namespace alpha_protocol {

struct FirstMessage : solid::frame::mprpc::Message {
    uint32_t    v = -1;
    std::string str;

    FirstMessage() {}
    FirstMessage(uint32_t _v, std::string&& _str)
        : v(_v)
        , str(std::move(_str))
    {
    }

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("str", _rthis.str), _rctx);
        _rr(id<2>, make("v", _rthis.v), _rctx);
    }
};

struct SecondMessage : solid::frame::mprpc::Message {
    uint64_t    v = -1;
    std::string str;

    SecondMessage() {}
    SecondMessage(uint64_t _v, std::string&& _str)
        : v(_v)
        , str(std::move(_str))
    {
    }

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("str", _rthis.str), _rctx);
        _rr(id<2>, make("v", _rthis.v), _rctx);
    }
};

struct ThirdMessage : solid::frame::mprpc::Message {
    uint16_t    v = -1;
    std::string str;

    ThirdMessage() {}
    ThirdMessage(uint16_t _v, std::string&& _str)
        : v(_v)
        , str(std::move(_str))
    {
    }

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("str", _rthis.str), _rctx);
        _rr(id<2>, make("v", _rthis.v), _rctx);
    }
};

template <class Reg>
inline void configure_protocol(Reg _rreg)
{
    _rreg({0, 1}, "FirstMessage", std::type_identity<FirstMessage>());
    _rreg({0, 2}, "SecondMessage", std::type_identity<SecondMessage>());
    _rreg({0, 3}, "ThirdMessage", std::type_identity<ThirdMessage>());
}

} // namespace alpha_protocol
