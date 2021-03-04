
#pragma once

#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcmessage.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v3.hpp"
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

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.userid_regex, _rctx, 1, "userid_regex");
    }
};

struct Date {
    uint8_t  day;
    uint8_t  month;
    uint16_t year;

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.day, _rctx, 1, "day").add(_rthis.month, _rctx, 2, "month").add(_rthis.year, _rctx, 3, "year");
    }
};

struct UserData {
    std::string full_name;
    std::string email;
    std::string country;
    std::string city;
    Date        birth_date;

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.full_name, _rctx, 1, "full_name").add(_rthis.email, _rctx, 2, "email").add(_rthis.country, _rctx, 3, "country");
        _rr.add(_rthis.city, _rctx, 4, "city").add(_rthis.birth_date, _rctx, 5, "birth_date");
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

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.user_data_map, _rctx, 1, "user_data_map");
    }
};

template <class Reg>
inline void configure_protocol(Reg _rreg)
{
    _rreg(1, "Request", solid::TypeToType<Request>());
    _rreg(2, "Response", solid::TypeToType<Response>());
}

} //namespace rpc_request
