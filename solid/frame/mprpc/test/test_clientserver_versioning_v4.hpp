#pragma once

#include "solid/frame/mprpc/mprpcprotocol_serialization_v4.hpp"
#include "solid/system/cassert.hpp"
#include <deque>
#include <string>
#include <vector>

namespace versioning {
namespace v4 {

struct Version {
    static constexpr uint32_t version       = 3;
    static constexpr uint32_t init_request  = 1;
    static constexpr uint32_t init_response = 1;
    static constexpr uint32_t request       = 3;
    static constexpr uint32_t response      = 2;
    static constexpr uint32_t response2     = 1;
    static constexpr uint32_t request2      = 1;

    uint32_t version_       = version;
    uint32_t init_request_  = init_request;
    uint32_t init_response_ = init_response;
    uint32_t request_       = request;
    uint32_t response_      = response;
    uint32_t request2_      = request2;
    uint32_t response2_     = response2;

    void clear()
    {
    }

    bool operator<=(const Version& _rthat) const
    {
        return version_ <= _rthat.version_ && init_request_ <= _rthat.init_request_ && init_response_ <= _rthat.init_response_ && request2_ <= _rthat.request2_ && response2_ <= _rthat.response2_;
    }

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _s, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _s(id<0>, make("version", _rthis.version_), _rctx);
        _s(id<101>, make("callable", [&_rthis](auto&& _s, Context& _rctx) -> void {
            if constexpr (!std::is_const_v<std::remove_reference_t<decltype(_rthis)>>) {
                if (_rthis.version > Version::version) {
                    _rthis.clear();
                    return;
                }
            }
            _s(id<3>, make("init_request", _rthis.init_request_), _rctx);
            _s(id<4>, make("init_request", _rthis.init_response_), _rctx);
            if (_rthis.version_ == 1) {
                _s(id<5>, make("request", _rthis.request_), _rctx);
                _s(id<6>, make("response", _rthis.response_), _rctx);
            } else if (_rthis.version_ == 2) {
                _s(id<5>, make("request", _rthis.request_), _rctx);
                _s(id<6>, make("response", _rthis.response_), _rctx);
                _s(id<7>, make("response2", _rthis.response2_), _rctx);
            } else if (_rthis.version_ == 3) {
                _s(id<8>, make("request2", _rthis.request2_), _rctx);
                _s(id<7>, make("response2", _rthis.response2_), _rctx);
            }
        }),
            _rctx);
    }
};

constexpr Version version;

struct InitRequest : solid::frame::mprpc::Message {
    Version version_;

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _s, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _s(id<1>, make("version", _rthis.version_), _rctx);
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

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _s, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _s(id<1>, make("error", _rthis.error_), _rctx);
        _s(id<2>, make("message", _rthis.message_), _rctx);
    }
};

struct Request2 : solid::frame::mprpc::Message {

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _s, Context& _rctx)
    {
        using namespace solid::reflection::v2;
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

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _s, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _s(id<1>, make("error", _rthis.error_), _rctx);
    }
};

template <class Reg>
inline void configure_protocol(Reg _rreg)
{
    _rreg(1, "InitRequest", std::type_identity<InitRequest>());
    _rreg(2, "InitResponse", std::type_identity<InitResponse>());
    //_rreg(4, "Request", std::type_identity<Request>());
    //_rreg(5, "Response", std::type_identity<Response>());
    _rreg(6, "Response2", std::type_identity<Response2>());
    _rreg(7, "Request2", std::type_identity<Request2>());
}

} // namespace v4
} // namespace versioning
