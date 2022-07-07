#pragma once

#include "solid/frame/mprpc/mprpcprotocol_serialization_v3.hpp"
#include "solid/system/cassert.hpp"
#include <deque>
#include <string>
#include <vector>

namespace versioning {
namespace v3 {

struct Version {
    static constexpr uint32_t version       = 2;
    static constexpr uint32_t init_request  = 1;
    static constexpr uint32_t init_response = 1;
    static constexpr uint32_t request       = 3;
    static constexpr uint32_t response      = 2;
    static constexpr uint32_t response2     = 1;

    uint32_t version_       = version;
    uint32_t init_request_  = init_request;
    uint32_t init_response_ = init_response;
    uint32_t request_       = request;
    uint32_t response_      = response;
    uint32_t response2_     = response2;

    void clear()
    {
    }

    bool operator<=(const Version& _rthat) const
    {
        return version_ <= _rthat.version_ && init_request_ <= _rthat.init_request_ && init_response_ <= _rthat.init_response_ && request_ <= _rthat.request_ && response_ <= _rthat.response_;
    }

    SOLID_REFLECT_V1(_s, _rthis, _rctx)
    {
        _s.add(_rthis.version_, _rctx, 0, "version");
        _s.add([&_rthis](Reflector& _s, Context& _rctx) {
            if constexpr (!Reflector::is_const_reflector) {
                if (_rthis.version > Version::version) {
                    _rthis.clear();
                    return;
                }
            }
            _s.add(_rthis.init_request_, _rctx, 3, "init_request");
            _s.add(_rthis.init_response_, _rctx, 4, "init_request");
            _s.add(_rthis.request_, _rctx, 5, "request");
            _s.add(_rthis.response_, _rctx, 6, "response");
            if (_rthis.version_ == version) {
                _s.add(_rthis.response2_, _rctx, 7, "response2");
            }
        },
            _rctx);
    }
};

constexpr Version version;

struct InitRequest : solid::frame::mprpc::Message {
    Version version_;

    SOLID_REFLECT_V1(_s, _rthis, _rctx)
    {
        _s.add(_rthis.version_, _rctx, 1, "version");
    }
};

struct InitResponse : solid::frame::mprpc::Message {
    uint32_t    error_ = -1;
    std::string message_;

    InitResponse() {}

    InitResponse(
        const InitRequest& _rreq)
        : solid::frame::mprpc::Message(_rreq)
        , error_(-1)
    {
    }

    SOLID_REFLECT_V1(_s, _rthis, _rctx)
    {
        _s.add(_rthis.error_, _rctx, 1, "error").add(_rthis.message_, _rctx, 2, "message");
    }
};

struct Request : solid::frame::mprpc::Message {
    uint32_t    valuei_; // deprecated
    std::string values_;

    SOLID_REFLECT_V1(_s, _rthis, _rctx)
    {
        if (_rctx.any().template get_if<Version>()->request_ == 2) {
            _s.add(_rthis.valuei_, _rctx, 1, "value");
        } else if (_rctx.any().template get_if<Version>()->request_ == 3) {
            _s.add(_rthis.values_, _rctx, 1, "value");
        }
    }
};

struct Response : solid::frame::mprpc::Message {
    uint32_t    error_ = -1;
    std::string message_;

    Response() {}

    Response(
        const solid::frame::mprpc::Message& _rreq)
        : solid::frame::mprpc::Message(_rreq)
    {
    }
    SOLID_REFLECT_V1(_s, _rthis, _rctx)
    {
        _s.add(_rthis.error_, _rctx, 1, "error");
        if (_rctx.any().template get_if<Version>()->response_ == 2) {
            _s.add(_rthis.message_, _rctx, 2, "message");
        }
    }
};

struct Response2 : solid::frame::mprpc::Message {
    uint32_t error_ = -1;

    Response2() {}

    Response2(
        const solid::frame::mprpc::Message& _rreq)
        : solid::frame::mprpc::Message(_rreq)
    {
    }

    SOLID_REFLECT_V1(_s, _rthis, _rctx)
    {
        _s.add(_rthis.error_, _rctx, 1, "error");
    }
};

template <class Reg>
inline void configure_protocol(Reg _rreg)
{
    _rreg(1, "InitRequest", solid::TypeToType<InitRequest>());
    _rreg(2, "InitResponse", solid::TypeToType<InitResponse>());
    _rreg(4, "Request", solid::TypeToType<Request>());
    _rreg(5, "Response", solid::TypeToType<Response>());
    _rreg(6, "Response2", solid::TypeToType<Response2>());
}

} // namespace v3
} // namespace versioning
