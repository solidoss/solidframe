
#pragma once

#include "solid/frame/mpipc/mpipccontext.hpp"
#include "solid/frame/mpipc/mpipcmessage.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v2.hpp"
#include <map>
#include <vector>

namespace ipc_request {

struct Request : solid::frame::mpipc::Message {
    std::string userid_regex;

    Request() {}

    Request(std::string&& _ustr)
        : userid_regex(std::move(_ustr))
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.userid_regex, _rctx, "userid_regex");
    }
};

struct Date {
    uint8_t  day;
    uint8_t  month;
    uint16_t year;

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.day, _rctx, "day").add(_rthis.month, _rctx, "month").add(_rthis.year, _rctx, "year");
    }
};

struct UserData {
    std::string full_name;
    std::string email;
    std::string country;
    std::string city;
    Date        birth_date;

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.full_name, _rctx, "full_name").add(_rthis.email, _rctx, "email").add(_rthis.country, _rctx, "country");
        _s.add(_rthis.city, _rctx, "city").add(_rthis.birth_date, _rctx, "birth_date");
    }
};

struct Response : solid::frame::mpipc::Message {
    using UserDataMapT = std::map<std::string, UserData>;

    UserDataMapT user_data_map;

    Response() {}

    Response(const solid::frame::mpipc::Message& _rmsg)
        : solid::frame::mpipc::Message(_rmsg)
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.user_data_map, _rctx, "user_data_map");
    }
};

using ProtocolT = solid::frame::mpipc::serialization_v2::Protocol<uint8_t>;

template <class R>
inline void protocol_setup(R _r, ProtocolT& _rproto)
{
    _rproto.null(ProtocolT::TypeIdT(0));

    _r(_rproto, solid::TypeToType<Request>(), 1);
    _r(_rproto, solid::TypeToType<Response>(), 2);
}

} //namespace ipc_request
