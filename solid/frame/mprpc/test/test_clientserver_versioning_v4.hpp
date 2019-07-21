#pragma once

#include "solid/frame/mprpc/mprpcprotocol_serialization_v2.hpp"
#include "solid/system/cassert.hpp"
#include <deque>
#include <string>
#include <vector>

namespace versioning {
namespace v4 {

struct InitRequest : solid::frame::mprpc::Message {
    static constexpr uint32_t version = 1;

    uint32_t version_ = version;

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        solid::serialization::addVersion<InitRequest>(_s, _rthis.version_, "version");
        _rctx.addVersion(_s);
    }
};

struct InitResponse : solid::frame::mprpc::Message {
    static constexpr uint32_t version = 1;

    uint32_t    version_ = version;
    uint32_t    error_   = -1;
    std::string message_;

    InitResponse() {}

    InitResponse(
        const InitRequest& _rreq, const uint32_t _version = version)
        : solid::frame::mprpc::Message(_rreq)
        , version_(_version)
        , error_(-1)
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        solid::serialization::addVersion<InitResponse>(_s, _rthis.version_, "version");
        _rctx.addVersion(_s);

        _s.add([&_rthis](S& _s, solid::frame::mprpc::ConnectionContext& _rctx, const char* /*_name*/) {
            _s.add(_rthis.error_, _rctx, "error").add(_rthis.message_, _rctx, "message");
        },
            _rctx, _name);
    }
};


struct Request2 : solid::frame::mprpc::Message {
    static constexpr uint32_t version = 1;

    uint32_t version_ = version;

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        solid::serialization::addVersion<Request2>(_s, _rthis.version_, "version");
    }
};


struct Response2 : solid::frame::mprpc::Message {
    static constexpr uint32_t version = 1;

    uint32_t    version_ = version;
    uint32_t    error_   = -1;

    Response2() {}

    Response2(
        const solid::frame::mprpc::Message& _rreq)
        : solid::frame::mprpc::Message(_rreq)
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        solid::serialization::addVersion<Response2>(_s, _rthis.version_, "version");

        _s.add([&_rthis](S& _s, solid::frame::mprpc::ConnectionContext& _rctx, const char* /*_name*/) {
            _s.add(_rthis.error_, _rctx, "error");
        },
            _rctx, _name);
    }
};


using ProtocolT = solid::frame::mprpc::serialization_v2::Protocol<uint8_t>;

template <class R>
inline void protocol_setup(R _r, ProtocolT& _rproto)
{
    _rproto.version(1, 3);

    _rproto.null(static_cast<ProtocolT::TypeIdT>(0));

    _r(_rproto, solid::TypeToType<InitRequest>(), 1);
    _r(_rproto, solid::TypeToType<InitResponse>(), 2);

    //_r(_rproto, solid::TypeToType<Request>(), 4);//Deprecated
    //_r(_rproto, solid::TypeToType<Response>(), 5);//Deprecated
    _r(_rproto, solid::TypeToType<Response2>(), 6);
    _r(_rproto, solid::TypeToType<Request2>(), 7);
}

}//namespace v4
}//namespace versioning


