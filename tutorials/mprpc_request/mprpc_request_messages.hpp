
#pragma once

#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcmessage.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v4.hpp"
#include <map>
#include <vector>

namespace rpc_request {

struct Request : solid::frame::mprpc::Message {
    std::string userid_regex;

    Request() {}

    Request(std::string&& _ustr)
        : userid_regex(std::move(_ustr))
    {
    }

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("userid_regex", _rthis.userid_regex), _rctx);
    }
};

struct Date {
    uint8_t  day;
    uint8_t  month;
    uint16_t year;

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("day", _rthis.day), _rctx);
        _rr(id<2>, make("month", _rthis.month), _rctx);
        _rr(id<3>, make("year", _rthis.year), _rctx);
    }
};

struct UserData {
    std::string full_name;
    std::string email;
    std::string country;
    std::string city;
    Date        birth_date;

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("full_name", _rthis.full_name), _rctx);
        _rr(id<2>, make("email", _rthis.email), _rctx);
        _rr(id<3>, make("country", _rthis.country), _rctx);
        _rr(id<4>, make("city", _rthis.city), _rctx);
        _rr(id<5>, make("birth_date", _rthis.birth_date), _rctx);
    }
};

struct Response : solid::frame::mprpc::Message {
    using UserDataMapT = std::map<std::string, UserData>;

    UserDataMapT user_data_map;

    Response() {}

    Response(const solid::frame::mprpc::Message& _rmsg)
        : solid::frame::mprpc::Message(_rmsg)
    {
    }

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("user_data_map", _rthis.user_data_map), _rctx);
    }
};

template <class Reg>
inline void configure_protocol(Reg _rreg)
{
    _rreg(1, "Request", std::type_identity<Request>());
    _rreg(2, "Response", std::type_identity<Response>());
}

} // namespace rpc_request
