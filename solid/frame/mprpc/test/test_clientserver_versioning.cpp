#include "solid/frame/mprpc/mprpcsocketstub_openssl.hpp"

#include "solid/frame/mprpc/mprpccompression_snappy.hpp"
#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v2.hpp"
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

#include "solid/system/exception.hpp"

#include "solid/system/log.hpp"

#include <iostream>

using namespace std;
using namespace solid;

using AioSchedulerT  = frame::Scheduler<frame::aio::Reactor>;
using SecureContextT = frame::aio::openssl::Context;
using ProtocolT      = frame::mprpc::serialization_v2::Protocol<uint8_t>;

namespace {

const solid::LoggerT logger("test");

mutex              mtx;
condition_variable cnd;
size_t             wait_response_count = 4;

namespace v1 {
void configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch, frame::aio::Resolver& _rrsv, const string& _server_port);
void send_request(frame::mprpc::ServiceT& _rsvc);
} //namespace v1

namespace v2 {
void configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch, frame::aio::Resolver& _rrsv, const string& _server_port);
void send_request(frame::mprpc::ServiceT& _rsvc);
} //namespace v2

namespace v3 {
string configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch);
void   configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch, frame::aio::Resolver& _rrsv, const string& _server_port);
void   send_request(frame::mprpc::ServiceT& _rsvc);
} //namespace v3

namespace v4 {
void configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch, frame::aio::Resolver& _rrsv, const string& _server_port);
void send_request(frame::mprpc::ServiceT& _rsvc);
} //namespace v4

} //namespace

int test_clientserver_versioning(int argc, char* argv[])
{

    solid::log_start(std::cerr, {"test:VIEW"});

    AioSchedulerT          scheduler;
    frame::Manager         manager;
    CallPool<void()>       cwp{WorkPoolConfiguration(), 1};
    frame::aio::Resolver   resolver(cwp);
    frame::mprpc::ServiceT service(manager);
    frame::mprpc::ServiceT service_v1(manager);
    frame::mprpc::ServiceT service_v2(manager);
    frame::mprpc::ServiceT service_v3(manager);
    frame::mprpc::ServiceT service_v4(manager);

    scheduler.start(1);

    auto port = v3::configure_service(service, scheduler);

    v1::configure_service(service_v1, scheduler, resolver, port);
    v1::send_request(service_v1);
    v2::configure_service(service_v2, scheduler, resolver, port);
    v2::send_request(service_v2);
    v3::configure_service(service_v3, scheduler, resolver, port);
    v3::send_request(service_v3);
    v4::configure_service(service_v4, scheduler, resolver, port);
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
    std::shared_ptr<M>& /*_rsent_msg_ptr*/,
    std::shared_ptr<M>& /*_rrecv_msg_ptr*/,
    ErrorConditionT const& /*_rerror*/)
{
    //catch all messages
    solid_check(false);
}

namespace v1 {
using namespace versioning::v1;

struct Setup {
    template <class T>
    void operator()(ProtocolT& _rprotocol, TypeToType<T> /*_t2t*/, const ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerMessage<T>(complete_message<T>, _rtid);
    }
};

void configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch, frame::aio::Resolver& _rrsv, const string& _server_port)
{
    auto                        proto = ProtocolT::create();
    frame::mprpc::Configuration cfg(_rsch, proto);

    protocol_setup(Setup(), *proto);

    cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(_rrsv, _server_port.c_str());

    auto connection_start_lambda = [](frame::mprpc::ConnectionContext& _ctx) {
        solid_log(logger, Info, "Connection start");
        auto req_ptr = std::make_shared<InitRequest>();
        auto lambda  = [](
                          frame::mprpc::ConnectionContext& _rctx,
                          std::shared_ptr<InitRequest>&    _rsent_msg_ptr,
                          std::shared_ptr<InitResponse>&   _rrecv_msg_ptr,
                          ErrorConditionT const&           _rerror) {
            if (_rrecv_msg_ptr) {
                if (_rrecv_msg_ptr->error_ == 0) {
                    solid_log(logger, Info, "Activate v1 connection: peer version " << _rctx.peerVersionMajor() << '.' << _rctx.peerVersionMinor());
                    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId());
                } else {
                    solid_log(logger, Error, "Initiating v1 connection: version " << _rctx.peerVersionMajor() << '.' << _rctx.peerVersionMinor() << " error " << _rrecv_msg_ptr->error_ << ':' << _rrecv_msg_ptr->message_);
                }
            }
        };

        _ctx.service().sendRequest(_ctx.recipientId(), req_ptr, lambda);
    };
    cfg.client.connection_start_fnc   = std::move(connection_start_lambda);
    cfg.client.connection_start_state = frame::mprpc::ConnectionState::Passive;

    _rsvc.start(std::move(cfg));
}
void send_request(frame::mprpc::ServiceT& _rsvc)
{
    auto req_ptr = std::make_shared<v1::Request>();
    _rsvc.sendRequest(
        "localhost", req_ptr,
        [](
            frame::mprpc::ConnectionContext& _rctx,
            std::shared_ptr<Request>&        _rreqmsgptr,
            std::shared_ptr<Response>&       _rresmsgptr,
            ErrorConditionT const&           _rerr) -> void {
            solid_check(_rresmsgptr);
            solid_check(_rresmsgptr->error_ == 10);
            solid_check(_rctx.peerVersionMajor() == 1);
            solid_check(_rctx.peerVersionMinor() == 2);
            solid_check(_rresmsgptr->version_ == 1);
            solid_log(logger, Info, "v1 received response");
            unique_lock<mutex> lock(mtx);
            --wait_response_count;
            if (wait_response_count == 0) {
                cnd.notify_one();
            }
        },
        {});
}
} //namespace v1

namespace v2 {
using namespace versioning::v2;

struct Setup {
    template <class T>
    void operator()(ProtocolT& _rprotocol, TypeToType<T> /*_t2t*/, const ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerMessage<T>(complete_message<T>, _rtid);
    }
};

void configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch, frame::aio::Resolver& _rrsv, const string& _server_port)
{
    auto                        proto = ProtocolT::create();
    frame::mprpc::Configuration cfg(_rsch, proto);

    protocol_setup(Setup(), *proto);

    cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(_rrsv, _server_port.c_str());

    auto connection_start_lambda = [](frame::mprpc::ConnectionContext& _ctx) {
        solid_log(logger, Info, "Connection start");
        auto req_ptr = std::make_shared<InitRequest>();
        auto lambda  = [](
                          frame::mprpc::ConnectionContext& _rctx,
                          std::shared_ptr<InitRequest>&    _rsent_msg_ptr,
                          std::shared_ptr<InitResponse>&   _rrecv_msg_ptr,
                          ErrorConditionT const&           _rerror) {
            if (_rrecv_msg_ptr) {
                if (_rrecv_msg_ptr->error_ == 0) {
                    solid_log(logger, Info, "Activate v2 connection: peer version " << _rctx.peerVersionMajor() << '.' << _rctx.peerVersionMinor());
                    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId());
                } else {
                    solid_log(logger, Error, "Initiating v2 connection: version " << _rctx.peerVersionMajor() << '.' << _rctx.peerVersionMinor() << " error " << _rrecv_msg_ptr->error_ << ':' << _rrecv_msg_ptr->message_);
                }
            }
        };

        _ctx.service().sendRequest(_ctx.recipientId(), req_ptr, lambda);
    };
    cfg.client.connection_start_fnc   = std::move(connection_start_lambda);
    cfg.client.connection_start_state = frame::mprpc::ConnectionState::Passive;

    _rsvc.start(std::move(cfg));
}
void send_request(frame::mprpc::ServiceT& _rsvc)
{
    auto req_ptr    = std::make_shared<Request>();
    req_ptr->value_ = 11;
    _rsvc.sendRequest(
        "localhost", req_ptr,
        [](
            frame::mprpc::ConnectionContext& _rctx,
            std::shared_ptr<Request>&        _rreqmsgptr,
            std::shared_ptr<Response>&       _rresmsgptr,
            ErrorConditionT const&           _rerr) -> void {
            solid_check(_rresmsgptr);
            solid_check(_rresmsgptr->error_ == _rreqmsgptr->value_);
            solid_check(_rctx.peerVersionMajor() == 1);
            solid_check(_rctx.peerVersionMinor() == 2);
            solid_check(_rresmsgptr->version_ == 2);

            solid_log(logger, Info, "v2 received response");

            unique_lock<mutex> lock(mtx);
            --wait_response_count;
            if (wait_response_count == 0) {
                cnd.notify_one();
            }
        },
        {});
}
} //namespace v2

namespace v3 {
using namespace versioning::v3;

struct Setup {
    template <class T>
    void operator()(ProtocolT& _rprotocol, TypeToType<T> /*_t2t*/, const ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerMessage<T>(complete_message<T>, _rtid);
    }
};

void configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch, frame::aio::Resolver& _rrsv, const string& _server_port)
{
    auto                        proto = ProtocolT::create();
    frame::mprpc::Configuration cfg(_rsch, proto);

    protocol_setup(Setup(), *proto);

    cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(_rrsv, _server_port.c_str());

    auto connection_start_lambda = [](frame::mprpc::ConnectionContext& _ctx) {
        solid_log(logger, Info, "Connection start");
        auto req_ptr = std::make_shared<InitRequest>();
        auto lambda  = [](
                          frame::mprpc::ConnectionContext& _rctx,
                          std::shared_ptr<InitRequest>&    _rsent_msg_ptr,
                          std::shared_ptr<InitResponse>&   _rrecv_msg_ptr,
                          ErrorConditionT const&           _rerror) {
            if (_rrecv_msg_ptr) {
                if (_rrecv_msg_ptr->error_ == 0) {
                    solid_log(logger, Info, "Activate v3 connection: peer version " << _rctx.peerVersionMajor() << '.' << _rctx.peerVersionMinor());
                    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId());
                } else {
                    solid_log(logger, Error, "Initiating v3 connection: version " << _rctx.peerVersionMajor() << '.' << _rctx.peerVersionMinor() << " error " << _rrecv_msg_ptr->error_ << ':' << _rrecv_msg_ptr->message_);
                }
            }
        };

        _ctx.service().sendRequest(_ctx.recipientId(), req_ptr, lambda);
    };
    cfg.client.connection_start_fnc   = std::move(connection_start_lambda);
    cfg.client.connection_start_state = frame::mprpc::ConnectionState::Passive;

    _rsvc.start(std::move(cfg));
}
void send_request(frame::mprpc::ServiceT& _rsvc)
{
    auto req_ptr     = std::make_shared<Request>();
    req_ptr->values_ = "test";
    _rsvc.sendRequest(
        "localhost", req_ptr,
        [](
            frame::mprpc::ConnectionContext& _rctx,
            std::shared_ptr<Request>&        _rreqmsgptr,
            std::shared_ptr<Response2>&      _rresmsgptr,
            ErrorConditionT const&           _rerr) -> void {
            solid_check(_rresmsgptr);
            solid_check(_rresmsgptr->error_ == _rreqmsgptr->values_.size());
            solid_check(_rctx.peerVersionMajor() == 1);
            solid_check(_rctx.peerVersionMinor() == 2);
            solid_check(_rresmsgptr->version_ == 1);

            solid_log(logger, Info, "v3 received response");

            unique_lock<mutex> lock(mtx);
            --wait_response_count;
            if (wait_response_count == 0) {
                cnd.notify_one();
            }
        },
        {});
}
} //namespace v3

namespace v4 {
using namespace versioning::v4;

struct Setup {
    template <class T>
    void operator()(ProtocolT& _rprotocol, TypeToType<T> /*_t2t*/, const ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerMessage<T>(complete_message<T>, _rtid);
    }
};

void configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch, frame::aio::Resolver& _rrsv, const string& _server_port)
{
    auto                        proto = ProtocolT::create();
    frame::mprpc::Configuration cfg(_rsch, proto);

    protocol_setup(Setup(), *proto);

    cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(_rrsv, _server_port.c_str());

    auto connection_start_lambda = [](frame::mprpc::ConnectionContext& _ctx) {
        solid_log(logger, Info, "Connection start");
        auto req_ptr = std::make_shared<InitRequest>();
        auto lambda  = [](
                          frame::mprpc::ConnectionContext& _rctx,
                          std::shared_ptr<InitRequest>&    _rsent_msg_ptr,
                          std::shared_ptr<InitResponse>&   _rrecv_msg_ptr,
                          ErrorConditionT const&           _rerror) {
            if (_rrecv_msg_ptr) {
                if (_rrecv_msg_ptr->error_ == 0) {
                    solid_log(logger, Info, "Activate v4 connection: peer version " << _rctx.peerVersionMajor() << '.' << _rctx.peerVersionMinor());
                    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId());
                } else {
                    solid_log(logger, Error, "Initiating v4 connection: version " << _rctx.peerVersionMajor() << '.' << _rctx.peerVersionMinor() << " error " << _rrecv_msg_ptr->error_ << ':' << _rrecv_msg_ptr->message_);
                    _rctx.service().forceCloseConnectionPool(_rctx.recipientId(), [](frame::mprpc::ConnectionContext&) {});
                }
                solid_check(_rrecv_msg_ptr->error_);
            }
        };

        _ctx.service().sendRequest(_ctx.recipientId(), req_ptr, lambda);
    };
    cfg.client.connection_start_fnc   = std::move(connection_start_lambda);
    cfg.client.connection_start_state = frame::mprpc::ConnectionState::Passive;

    _rsvc.start(std::move(cfg));
}
void send_request(frame::mprpc::ServiceT& _rsvc)
{
    auto req_ptr = std::make_shared<Request2>();
    _rsvc.sendRequest(
        "localhost", req_ptr,
        [](
            frame::mprpc::ConnectionContext& _rctx,
            std::shared_ptr<Request2>&       _rreqmsgptr,
            std::shared_ptr<Response2>&      _rresmsgptr,
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
} //namespace v4

namespace v3 {
using namespace versioning::v3;

template <class M>
void complete_message(
    frame::mprpc::ConnectionContext& /*_rctx*/,
    std::shared_ptr<M>& /*_rsent_msg_ptr*/,
    std::shared_ptr<M>& /*_rrecv_msg_ptr*/,
    ErrorConditionT const& /*_rerror*/);

template <>
void complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<InitRequest>& /*_rsent_msg_ptr*/,
    std::shared_ptr<InitRequest>& _rrecv_msg_ptr,
    ErrorConditionT const& /*_rerror*/)
{
    solid_log(logger, Info, "Init request: peer [" << _rctx.peerVersionMajor() << '.' << _rctx.peerVersionMinor() << "] mine [" << _rctx.versionMajor() << '.' << _rctx.versionMinor() << ']');

    auto res_ptr = std::make_shared<InitResponse>(*_rrecv_msg_ptr);
    if (
        _rctx.peerVersionMajor() > _rctx.versionMajor() || (_rctx.peerVersionMajor() == _rctx.versionMajor() && _rctx.peerVersionMinor() > _rctx.versionMinor())

    ) {
        res_ptr->error_   = 1;
        res_ptr->message_ = "protocol version not supported";
        _rctx.service().sendResponse(_rctx.recipientId(), res_ptr);
        _rctx.service().delayCloseConnectionPool(_rctx.recipientId(), [](frame::mprpc::ConnectionContext& /*_rctx*/) {});
    } else {
        res_ptr->error_ = 0;

        auto lambda = [response_ptr = std::move(res_ptr)](frame::mprpc::ConnectionContext& _rctx, ErrorConditionT const& _rerror) mutable {
            solid_check(!_rerror, "error activating connection: " << _rerror.message());

            _rctx.service().sendResponse(_rctx.recipientId(), response_ptr);
        };
        _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId(), lambda);
    }
}

template <>
void complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Request>& /*_rsent_msg_ptr*/,
    std::shared_ptr<Request>& _rrecv_msg_ptr,
    ErrorConditionT const& /*_rerror*/)
{
    if (_rctx.peerVersionMinor() == 2) {
        //need to send back Response2
        auto res_ptr    = std::make_shared<Response2>(*_rrecv_msg_ptr);
        res_ptr->error_ = _rrecv_msg_ptr->values_.size();
        _rctx.service().sendResponse(_rctx.recipientId(), res_ptr);
    } else if (_rrecv_msg_ptr->version_ == 1) {
        auto res_ptr    = std::make_shared<Response>(*_rrecv_msg_ptr, 1 /*version*/);
        res_ptr->error_ = 10;
        _rctx.service().sendResponse(_rctx.recipientId(), res_ptr);
    } else if (_rrecv_msg_ptr->version_ == 2) {
        auto res_ptr    = std::make_shared<Response>(*_rrecv_msg_ptr, 2 /*version*/);
        res_ptr->error_ = _rrecv_msg_ptr->valuei_;
        _rctx.service().sendResponse(_rctx.recipientId(), res_ptr);
    }
}

template <class M>
void complete_message(
    frame::mprpc::ConnectionContext& /*_rctx*/,
    std::shared_ptr<M>& /*_rsent_msg_ptr*/,
    std::shared_ptr<M>& /*_rrecv_msg_ptr*/,
    ErrorConditionT const& /*_rerror*/)
{
    //catch all
}

struct ServerSetup {
    template <class T>
    void operator()(ProtocolT& _rprotocol, TypeToType<T> /*_t2t*/, const ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerMessage<T>(complete_message<T>, _rtid);
    }
};

string configure_service(frame::mprpc::ServiceT& _rsvc, AioSchedulerT& _rsch)
{
    auto                        proto = ProtocolT::create();
    frame::mprpc::Configuration cfg(_rsch, proto);

    protocol_setup(ServerSetup(), *proto);

    cfg.server.listener_address_str   = "0.0.0.0:0";
    cfg.client.connection_start_state = frame::mprpc::ConnectionState::Passive;

    _rsvc.start(std::move(cfg));

    string server_port;
    {
        std::ostringstream oss;
        oss << _rsvc.configuration().server.listenerPort();
        server_port = oss.str();
        solid_dbg(generic_logger, Info, "server listens on port: " << server_port);
    }
    return server_port;
}
} //namespace v3

} //namespace
