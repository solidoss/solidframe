#include "solid/frame/mprpc/mprpcsocketstub_openssl.hpp"

#include "solid/frame/mprpc/mprpccompression_snappy.hpp"
#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v3.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioactor.hpp"
#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioresolver.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include "test_clientserver_versioning_v1.hpp"
#include "test_clientserver_versioning_v2.hpp"
#include "test_clientserver_versioning_v3.hpp"
#include "test_clientserver_versioning_v4.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/utility/threadpool.hpp"

#include "solid/system/exception.hpp"

#include "solid/system/log.hpp"

#include <iostream>

using namespace std;
using namespace solid;

namespace {

using AioSchedulerT  = frame::Scheduler<frame::aio::Reactor<frame::mprpc::EventT>>;
using SecureContextT = frame::aio::openssl::Context;
using CallPoolT      = ThreadPool<Function<void()>, Function<void()>>;

const solid::LoggerT logger("test");

mutex              mtx;
condition_variable cnd;
size_t             wait_response_count = 4;

namespace v1 {
void configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch, frame::aio::Resolver& _rrsv, const string& _server_port);
void send_request(frame::mprpc::ServiceT& _rsvc);
} // namespace v1

namespace v2 {
void configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch, frame::aio::Resolver& _rrsv, const string& _server_port);
void send_request(frame::mprpc::ServiceT& _rsvc);
} // namespace v2

namespace v3 {
string configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch);
void   configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch, frame::aio::Resolver& _rrsv, const string& _server_port);
void   send_request(frame::mprpc::ServiceT& _rsvc);
} // namespace v3

namespace v4 {
void configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch, frame::aio::Resolver& _rrsv, const string& _server_port);
void send_request(frame::mprpc::ServiceT& _rsvc);
} // namespace v4

} // namespace

int test_clientserver_versioning(int argc, char* argv[])
{

    solid::log_start(std::cerr, {"test:VIEW"});

    AioSchedulerT          scheduler;
    frame::Manager         manager;
    CallPoolT              cwp{{1, 100, 0}, [](const size_t) {}, [](const size_t) {}};
    frame::aio::Resolver   resolver([&cwp](std::function<void()>&& _fnc) { cwp.pushOne(std::move(_fnc)); });
    frame::mprpc::ServiceT service(manager);
    frame::mprpc::ServiceT service_v1(manager);
    frame::mprpc::ServiceT service_v2(manager);
    frame::mprpc::ServiceT service_v3(manager);
    frame::mprpc::ServiceT service_v4(manager);

    scheduler.start(1);

    auto port = v3::configure_service(service, scheduler);

    v1::configure_service(service_v1, scheduler, resolver, port);
    v2::configure_service(service_v2, scheduler, resolver, port);
    v3::configure_service(service_v3, scheduler, resolver, port);
    v4::configure_service(service_v4, scheduler, resolver, port);
    v1::send_request(service_v1);
    v2::send_request(service_v2);
    v3::send_request(service_v3);
    v4::send_request(service_v4);

    unique_lock<mutex> lock(mtx);

    if (!cnd.wait_for(lock, std::chrono::seconds(10), []() { return wait_response_count == 0; })) {
        solid_throw("Process is taking too long.");
    }

    return 0;
}

namespace {

template <class M>
void complete_message(
    frame::mprpc::ConnectionContext& /*_rctx*/,
    frame::mprpc::MessagePointerT<M>& /*_rsent_msg_ptr*/,
    frame::mprpc::MessagePointerT<M>& /*_rrecv_msg_ptr*/,
    ErrorConditionT const& /*_rerror*/)
{
    // catch all messages
    solid_check(false);
}

namespace v1 {
using namespace versioning::v1;

using RequestPointerT      = solid::frame::mprpc::MessagePointerT<Request>;
using ResponsePointerT     = solid::frame::mprpc::MessagePointerT<Response>;
using InitRequestPointerT  = solid::frame::mprpc::MessagePointerT<InitRequest>;
using InitResponsePointerT = solid::frame::mprpc::MessagePointerT<InitResponse>;

void configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch, frame::aio::Resolver& _rrsv, const string& _server_port)
{
    auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
        reflection::v1::metadata::factory,
        [&](auto& _rmap) {
            auto lambda = [&](const uint8_t _id, const std::string_view _name, auto const& _rtype) {
                using TypeT = typename std::decay_t<decltype(_rtype)>::TypeT;
                _rmap.template registerMessage<TypeT>(_id, _name, complete_message<TypeT>);
            };
            configure_protocol(lambda);
        });
    frame::mprpc::Configuration cfg(_rsch, proto);

    cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(_rrsv, _server_port.c_str());

    auto connection_start_lambda = [](frame::mprpc::ConnectionContext& _rctx) {
        solid_log(logger, Info, "Connection start");
        auto req_ptr = frame::mprpc::make_message<InitRequest>();
        auto lambda  = [](
                          frame::mprpc::ConnectionContext& _rctx,
                          InitRequestPointerT&             _rsent_msg_ptr,
                          InitResponsePointerT&            _rrecv_msg_ptr,
                          ErrorConditionT const&           _rerror) {
            if (_rrecv_msg_ptr) {
                if (_rrecv_msg_ptr->error_ == 0) {
                    solid_log(logger, Info, "Activate v1 connection");
                    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId());
                } else {
                    solid_log(logger, Error, "Initiating v1 connection: " << _rrecv_msg_ptr->error_ << ':' << _rrecv_msg_ptr->message_);
                }
            }
        };
        _rctx.any() = std::make_tuple(version);
        _rctx.service().sendRequest(_rctx.recipientId(), req_ptr, lambda);
    };
    cfg.client.connection_start_fnc   = std::move(connection_start_lambda);
    cfg.client.connection_start_state = frame::mprpc::ConnectionState::Passive;

    _rsvc.start(std::move(cfg));
}
void send_request(frame::mprpc::ServiceT& _rsvc)
{
    auto req_ptr = frame::mprpc::make_message<v1::Request>();
    _rsvc.sendRequest(
        {"localhost"}, req_ptr,
        [](
            frame::mprpc::ConnectionContext& _rctx,
            RequestPointerT&                 _rreqmsgptr,
            ResponsePointerT&                _rresmsgptr,
            ErrorConditionT const&           _rerr) -> void {
            solid_check(_rresmsgptr);
            solid_check(_rresmsgptr->error_ == 10);
            solid_log(logger, Info, "v1 received response");
            unique_lock<mutex> lock(mtx);
            --wait_response_count;
            if (wait_response_count == 0) {
                cnd.notify_one();
            }
        },
        {});
}
} // namespace v1

namespace v2 {
using namespace versioning::v2;

using RequestPointerT      = solid::frame::mprpc::MessagePointerT<Request>;
using ResponsePointerT     = solid::frame::mprpc::MessagePointerT<Response>;
using InitRequestPointerT  = solid::frame::mprpc::MessagePointerT<InitRequest>;
using InitResponsePointerT = solid::frame::mprpc::MessagePointerT<InitResponse>;

void configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch, frame::aio::Resolver& _rrsv, const string& _server_port)
{
    auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
        reflection::v1::metadata::factory,
        [&](auto& _rmap) {
            auto lambda = [&](const uint8_t _id, const std::string_view _name, auto const& _rtype) {
                using TypeT = typename std::decay_t<decltype(_rtype)>::TypeT;
                _rmap.template registerMessage<TypeT>(_id, _name, complete_message<TypeT>);
            };
            configure_protocol(lambda);
        });
    frame::mprpc::Configuration cfg(_rsch, proto);

    cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(_rrsv, _server_port.c_str());

    auto connection_start_lambda = [](frame::mprpc::ConnectionContext& _rctx) {
        solid_log(logger, Info, "Connection start");
        auto req_ptr = frame::mprpc::make_message<InitRequest>();
        auto lambda  = [](
                          frame::mprpc::ConnectionContext& _rctx,
                          InitRequestPointerT&             _rsent_msg_ptr,
                          InitResponsePointerT&            _rrecv_msg_ptr,
                          ErrorConditionT const&           _rerror) {
            if (_rrecv_msg_ptr) {
                if (_rrecv_msg_ptr->error_ == 0) {
                    solid_log(logger, Info, "Activate v2 connection");
                    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId());
                } else {
                    solid_log(logger, Error, "Initiating v2 connection: " << _rrecv_msg_ptr->error_ << ':' << _rrecv_msg_ptr->message_);
                }
            }
        };
        _rctx.any() = std::make_tuple(version);
        _rctx.service().sendRequest(_rctx.recipientId(), req_ptr, lambda);
    };
    cfg.client.connection_start_fnc   = std::move(connection_start_lambda);
    cfg.client.connection_start_state = frame::mprpc::ConnectionState::Passive;

    _rsvc.start(std::move(cfg));
}
void send_request(frame::mprpc::ServiceT& _rsvc)
{
    auto req_ptr    = frame::mprpc::make_message<Request>();
    req_ptr->value_ = 11;
    _rsvc.sendRequest(
        {"localhost"}, req_ptr,
        [](
            frame::mprpc::ConnectionContext& _rctx,
            RequestPointerT&                 _rreqmsgptr,
            ResponsePointerT&                _rresmsgptr,
            ErrorConditionT const&           _rerr) -> void {
            solid_check(_rresmsgptr);
            solid_check(_rresmsgptr->error_ == _rreqmsgptr->value_);

            solid_log(logger, Info, "v2 received response");

            unique_lock<mutex> lock(mtx);
            --wait_response_count;
            if (wait_response_count == 0) {
                cnd.notify_one();
            }
        },
        {});
}
} // namespace v2

namespace v3 {
using namespace versioning::v3;

using RequestPointerT      = solid::frame::mprpc::MessagePointerT<Request>;
using ResponsePointerT     = solid::frame::mprpc::MessagePointerT<Response>;
using Response2PointerT    = solid::frame::mprpc::MessagePointerT<Response2>;
using InitRequestPointerT  = solid::frame::mprpc::MessagePointerT<InitRequest>;
using InitResponsePointerT = solid::frame::mprpc::MessagePointerT<InitResponse>;

void configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch, frame::aio::Resolver& _rrsv, const string& _server_port)
{
    auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
        reflection::v1::metadata::factory,
        [&](auto& _rmap) {
            auto lambda = [&](const uint8_t _id, const std::string_view _name, auto const& _rtype) {
                using TypeT = typename std::decay_t<decltype(_rtype)>::TypeT;
                _rmap.template registerMessage<TypeT>(_id, _name, complete_message<TypeT>);
            };
            configure_protocol(lambda);
        });
    frame::mprpc::Configuration cfg(_rsch, proto);
    cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(_rrsv, _server_port.c_str());

    auto connection_start_lambda = [](frame::mprpc::ConnectionContext& _rctx) {
        solid_log(logger, Info, "Connection start");
        auto req_ptr = frame::mprpc::make_message<InitRequest>();
        auto lambda  = [](
                          frame::mprpc::ConnectionContext& _rctx,
                          InitRequestPointerT&             _rsent_msg_ptr,
                          InitResponsePointerT&            _rrecv_msg_ptr,
                          ErrorConditionT const&           _rerror) {
            if (_rrecv_msg_ptr) {
                if (_rrecv_msg_ptr->error_ == 0) {
                    solid_log(logger, Info, "Activate v3 connection");
                    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId());
                } else {
                    solid_log(logger, Error, "Initiating v3 connection: " << _rrecv_msg_ptr->error_ << ':' << _rrecv_msg_ptr->message_);
                }
            }
        };

        _rctx.any() = std::make_tuple(version);
        _rctx.service().sendRequest(_rctx.recipientId(), req_ptr, lambda);
    };
    cfg.client.connection_start_fnc   = std::move(connection_start_lambda);
    cfg.client.connection_start_state = frame::mprpc::ConnectionState::Passive;

    _rsvc.start(std::move(cfg));
}
void send_request(frame::mprpc::ServiceT& _rsvc)
{
    auto req_ptr     = frame::mprpc::make_message<Request>();
    req_ptr->values_ = "test";
    _rsvc.sendRequest(
        {"localhost"}, req_ptr,
        [](
            frame::mprpc::ConnectionContext& _rctx,
            RequestPointerT&                 _rreqmsgptr,
            Response2PointerT&               _rresmsgptr,
            ErrorConditionT const&           _rerr) -> void {
            solid_check(_rresmsgptr);
            solid_check(_rresmsgptr->error_ == _rreqmsgptr->values_.size());

            solid_log(logger, Info, "v3 received response");

            unique_lock<mutex> lock(mtx);
            --wait_response_count;
            if (wait_response_count == 0) {
                cnd.notify_one();
            }
        },
        {});
}
} // namespace v3

namespace v4 {
using namespace versioning::v4;

using InitRequestPointerT  = solid::frame::mprpc::MessagePointerT<InitRequest>;
using InitResponsePointerT = solid::frame::mprpc::MessagePointerT<InitResponse>;
using Request2PointerT     = solid::frame::mprpc::MessagePointerT<Request2>;
using Response2PointerT    = solid::frame::mprpc::MessagePointerT<Response2>;

void configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch, frame::aio::Resolver& _rrsv, const string& _server_port)
{
    auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
        reflection::v1::metadata::factory,
        [&](auto& _rmap) {
            auto lambda = [&](const uint8_t _id, const std::string_view _name, auto const& _rtype) {
                using TypeT = typename std::decay_t<decltype(_rtype)>::TypeT;
                _rmap.template registerMessage<TypeT>(_id, _name, complete_message<TypeT>);
            };
            configure_protocol(lambda);
        });
    frame::mprpc::Configuration cfg(_rsch, proto);

    cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(_rrsv, _server_port.c_str());

    auto connection_start_lambda = [](frame::mprpc::ConnectionContext& _rctx) {
        solid_log(logger, Info, "Connection start");
        auto req_ptr = frame::mprpc::make_message<InitRequest>();
        auto lambda  = [](
                          frame::mprpc::ConnectionContext& _rctx,
                          InitRequestPointerT&             _rsent_msg_ptr,
                          InitResponsePointerT&            _rrecv_msg_ptr,
                          ErrorConditionT const&           _rerror) {
            if (_rrecv_msg_ptr) {
                if (_rrecv_msg_ptr->error_ == 0) {
                    solid_log(logger, Info, "Activate v4 connection");
                    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId());
                } else {
                    solid_log(logger, Error, "Initiating v4 connection: " << _rrecv_msg_ptr->error_ << ':' << _rrecv_msg_ptr->message_);
                    _rctx.service().forceCloseConnectionPool(_rctx.recipientId(), [](frame::mprpc::ConnectionContext&) {});
                }
                solid_check(_rrecv_msg_ptr->error_);
            }
        };
        _rctx.any() = std::make_tuple(version);
        _rctx.service().sendRequest(_rctx.recipientId(), req_ptr, lambda);
    };
    cfg.client.connection_start_fnc   = std::move(connection_start_lambda);
    cfg.client.connection_start_state = frame::mprpc::ConnectionState::Passive;

    _rsvc.start(std::move(cfg));
}
void send_request(frame::mprpc::ServiceT& _rsvc)
{
    auto req_ptr = frame::mprpc::make_message<Request2>();
    _rsvc.sendRequest(
        {"localhost"}, req_ptr,
        [](
            frame::mprpc::ConnectionContext& _rctx,
            Request2PointerT&                _rreqmsgptr,
            Response2PointerT&               _rresmsgptr,
            ErrorConditionT const&           _rerr) -> void {
            solid_check(!_rresmsgptr);

            solid_log(logger, Info, "v4 no response");

            unique_lock<mutex> lock(mtx);
            --wait_response_count;
            if (wait_response_count == 0) {
                cnd.notify_one();
            }
        },
        {});
}
} // namespace v4

namespace v3 {
using namespace versioning::v3;

template <class M>
void complete_message(
    frame::mprpc::ConnectionContext& /*_rctx*/,
    frame::mprpc::MessagePointerT<M>& /*_rsent_msg_ptr*/,
    frame::mprpc::MessagePointerT<M>& /*_rrecv_msg_ptr*/,
    ErrorConditionT const& /*_rerror*/);

template <>
void complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    InitRequestPointerT& /*_rsent_msg_ptr*/,
    InitRequestPointerT& _rrecv_msg_ptr,
    ErrorConditionT const& /*_rerror*/)
{
    solid_log(logger, Info, "Init request: peer [" << _rrecv_msg_ptr->version_.version_ << ']');

    auto res_ptr = frame::mprpc::make_message<InitResponse>(*_rrecv_msg_ptr);

    _rctx.any() = std::make_tuple(_rrecv_msg_ptr->version_);

    if (
        _rrecv_msg_ptr->version_ <= version) {
        res_ptr->error_ = 0;
        auto lambda     = [response_ptr = std::move(res_ptr)](frame::mprpc::ConnectionContext& _rctx, ErrorConditionT const& _rerror) mutable {
            solid_check(!_rerror, "error activating connection: " << _rerror.message());

            _rctx.service().sendResponse(_rctx.recipientId(), response_ptr);
        };
        _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId(), lambda);
    } else {
        res_ptr->error_   = 1;
        res_ptr->message_ = "unsupported version";
        _rctx.service().sendResponse(_rctx.recipientId(), res_ptr);
        _rctx.service().delayCloseConnectionPool(_rctx.recipientId(), [](frame::mprpc::ConnectionContext& /*_rctx*/) {});
    }
}

template <>
void complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    RequestPointerT& /*_rsent_msg_ptr*/,
    RequestPointerT& _rrecv_msg_ptr,
    ErrorConditionT const& /*_rerror*/)
{

    if (_rctx.any().get_if<Version>()->version_ == 2) {
        // need to send back Response2
        auto res_ptr    = frame::mprpc::make_message<Response2>(*_rrecv_msg_ptr);
        res_ptr->error_ = _rrecv_msg_ptr->values_.size();
        _rctx.service().sendResponse(_rctx.recipientId(), res_ptr);
    } else if (_rctx.any().get_if<Version>()->request_ == 1) {
        auto res_ptr    = frame::mprpc::make_message<Response>(*_rrecv_msg_ptr);
        res_ptr->error_ = 10;
        _rctx.service().sendResponse(_rctx.recipientId(), res_ptr);
    } else if (_rctx.any().get_if<Version>()->request_ == 2) {
        auto res_ptr    = frame::mprpc::make_message<Response>(*_rrecv_msg_ptr);
        res_ptr->error_ = _rrecv_msg_ptr->valuei_;
        _rctx.service().sendResponse(_rctx.recipientId(), res_ptr);
    }
}

template <class M>
void complete_message(
    frame::mprpc::ConnectionContext& /*_rctx*/,
    frame::mprpc::MessagePointerT<M>& /*_rsent_msg_ptr*/,
    frame::mprpc::MessagePointerT<M>& /*_rrecv_msg_ptr*/,
    ErrorConditionT const& /*_rerror*/)
{
    // catch all
}

string configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch)
{
    auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
        reflection::v1::metadata::factory,
        [&](auto& _rmap) {
            auto lambda = [&](const uint8_t _id, const std::string_view _name, auto const& _rtype) {
                using TypeT = typename std::decay_t<decltype(_rtype)>::TypeT;
                _rmap.template registerMessage<TypeT>(_id, _name, complete_message<TypeT>);
            };
            configure_protocol(lambda);
        });
    frame::mprpc::Configuration cfg(_rsch, proto);

    cfg.server.listener_address_str   = "0.0.0.0:0";
    cfg.client.connection_start_state = frame::mprpc::ConnectionState::Passive;

    string server_port;
    {
        frame::mprpc::ServiceStartStatus start_status;
        _rsvc.start(start_status, std::move(cfg));

        std::ostringstream oss;
        oss << start_status.listen_addr_vec_.back().port();
        server_port = oss.str();
        solid_dbg(generic_logger, Info, "server listens on: " << start_status.listen_addr_vec_.back());
    }
    return server_port;
}
} // namespace v3

} // namespace
