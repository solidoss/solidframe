#pragma once

#include "solid/frame/mprpc/mprpcprotocol_serialization_v4.hpp"
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

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _s, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _s(id<0>, make("version", _rthis.version_), _rctx);
        _s(id<103>, make("callable", [&_rthis](auto&& _s, Context& _rctx) -> void {
            if constexpr (!std::is_const_v<std::remove_reference_t<decltype(_rthis)>>) {
                if (_rthis.version > Version::version) {
                    _rthis.clear();
                    return;
                }
            }
            _s(id<3>, make("init_request", _rthis.init_request_), _rctx);
            _s(id<4>, make("init_request", _rthis.init_response_), _rctx);
            _s(id<5>, make("request", _rthis.request_), _rctx);
            _s(id<6>, make("response", _rthis.response_), _rctx);
            if (_rthis.version_ == version) {
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

struct Request : solid::frame::mprpc::Message {
    uint32_t    valuei_; // deprecated
    std::string values_;

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _s, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        if (_rctx.any().template get_if<Version>()->request_ == 2) {
            _s(id<1>, make("value", _rthis.valuei_), _rctx);
        } else if (_rctx.any().template get_if<Version>()->request_ == 3) {
            _s(id<1>, make("value", _rthis.values_), _rctx);
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
    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _s, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _s(id<1>, make("error", _rthis.error_), _rctx);
        if (_rctx.any().template get_if<Version>()->response_ == 2) {
            _s(id<2>, make("message", _rthis.message_), _rctx);
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
    _rreg(4, "Request", std::type_identity<Request>());
    _rreg(5, "Response", std::type_identity<Response>());
    _rreg(6, "Response2", std::type_identity<Response2>());
}

} // namespace v3
} // namespace versioning
