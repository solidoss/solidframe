
#pragma once

#include "../common.hpp"
#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcmessage.hpp"

namespace beta_protocol {

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

struct FirstMessage : solid::frame::mprpc::Message {
    uint32_t    v = -1;
    std::string str;

    FirstMessage() {}

    FirstMessage(ThirdMessage&& _rmsg)
        : solid::frame::mprpc::Message(_rmsg)
        , v(_rmsg.v)
        , str(std::move(_rmsg.str))
    {
    }

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
    SecondMessage(FirstMessage&& _rmsg)
        : solid::frame::mprpc::Message(_rmsg)
        , v(_rmsg.v)
        , str(std::move(_rmsg.str))
    {
    }
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

template <class Reg>
inline void configure_protocol(Reg _rreg)
{
    _rreg({1, 1}, "FirstMessage", std::type_identity<FirstMessage>());
    _rreg({1, 2}, "SecondMessage", std::type_identity<SecondMessage>());
    _rreg({1, 3}, "ThirdMessage", std::type_identity<ThirdMessage>());
}

} // namespace beta_protocol
