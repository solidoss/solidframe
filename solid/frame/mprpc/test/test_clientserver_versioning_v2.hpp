#pragma once

#include "solid/frame/mprpc/mprpcprotocol_serialization_v2.hpp"
#include "solid/system/cassert.hpp"
#include <deque>
#include <string>
#include <vector>

namespace versioning {
namespace v2 {

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

struct Request : solid::frame::mprpc::Message {
    static constexpr uint32_t version = 2;

    uint32_t version_ = version;

    uint32_t value_;

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        solid::serialization::addVersion<Request>(_s, _rthis.version_, "version");

        _s.add([&_rthis](S& _s, solid::frame::mprpc::ConnectionContext& _rctx, const char* /*_name*/) {
            if (_s.version(_rthis) == 2) {
                _s.add(_rthis.value_, _rctx, "value");
            }
        },
            _rctx, _name);
    }
};

struct Response : solid::frame::mprpc::Message {
    static constexpr uint32_t version = 2;

    uint32_t    version_ = version;
    uint32_t    error_   = -1;
    std::string message_;

    Response() {}

    Response(
        const solid::frame::mprpc::Message& _rreq)
        : solid::frame::mprpc::Message(_rreq)
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        solid::serialization::addVersion<Response>(_s, _rthis.version_, "version");

        _s.add([&_rthis](S& _s, solid::frame::mprpc::ConnectionContext& _rctx, const char* /*_name*/) {
            _s.add(_rthis.error_, _rctx, "error");
            if (_rthis.version_ == 2) {
                _s.add(_rthis.message_, _rctx, "message");
            }
        },
            _rctx, _name);
    }
};

using ProtocolT = solid::frame::mprpc::serialization_v2::Protocol<uint8_t>;

template <class R>
inline void protocol_setup(R _r, ProtocolT& _rproto)
{
    _rproto.version(1, 1);

    _rproto.null(static_cast<ProtocolT::TypeIdT>(0));

    _r(_rproto, solid::TypeToType<InitRequest>(), 1);
    _r(_rproto, solid::TypeToType<InitResponse>(), 2);

    _r(_rproto, solid::TypeToType<Request>(), 4);
    _r(_rproto, solid::TypeToType<Response>(), 5);
}

} //namespace v2
} //namespace versioning
