#include "solid/frame/mprpc/mprpcsocketstub_openssl.hpp"

#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioactor.hpp"
#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioresolver.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include "solid/frame/mprpc/mprpccompression_snappy.hpp"
#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v3.hpp"
#include "solid/frame/mprpc/mprpcrelayengines.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "solid/utility/threadpool.hpp"

#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"

#include <iostream>

#include <signal.h>

using namespace std;
using namespace solid;

namespace {
using AioSchedulerT  = frame::Scheduler<frame::aio::Reactor>;
using SecureContextT = frame::aio::openssl::Context;
using CallPoolT      = ThreadPool<Function<void()>, Function<void()>>;

bool               running = true;
mutex              mtx;
condition_variable cnd;

void done()
{
    lock_guard<mutex> lock(mtx);
    running = false;
    cnd.notify_all();
}

struct Register : frame::mprpc::Message {
    std::string str;
    uint32_t    err;

    Register(const std::string& _rstr, uint32_t _err = 0)
        : str(_rstr)
        , err(_err)
    {
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << (void*)this);
    }
    Register(uint32_t _err = -1)
        : err(_err)
    {
    }

    ~Register()
    {
        solid_dbg(generic_logger, Info, "DELETE ---------------- " << (void*)this);
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.err, _rctx, 0, "err").add(_rthis.str, _rctx, 1, "str");
    }
};

struct DetectCloseMessage : frame::mprpc::Message {
    uint32_t idx = 0;

    DetectCloseMessage() = default;

    DetectCloseMessage(frame::mprpc::Message& _rreq, uint32_t _idx)
        : frame::mprpc::Message(_rreq)
        , idx(_idx)
    {
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.idx, _rctx, 0, "idx");
    }
};

struct Message : frame::mprpc::Message {
    uint32_t idx = 0;

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.idx, _rctx, 0, "idx");
    }
};

//-----------------------------------------------------------------------------
//      PeerA
//-----------------------------------------------------------------------------

void peera_connection_start(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());
}

void peera_connection_stop(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
}

void peera_complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rerror.message());

    solid_check(_rsent_msg_ptr, "Error: there should be a request message");
    if (_rrecv_msg_ptr) {
        _rrecv_msg_ptr->clearHeader();
        _rctx.service().sendMessage("localhost/b", std::move(_rrecv_msg_ptr), {frame::mprpc::MessageFlagsE::AwaitResponse});
    }
}

void peera_complete_detect_close(
    frame::mprpc::ConnectionContext&     _rctx,
    std::shared_ptr<DetectCloseMessage>& _rsent_msg_ptr, std::shared_ptr<DetectCloseMessage>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    static size_t call_count = 0;
    ++call_count;
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rerror.message());
    if (_rrecv_msg_ptr) {
        solid_dbg(generic_logger, Info, _rctx.recipientId() << " peera received DetectCloseMessage " << _rrecv_msg_ptr->idx);
        solid_check(!_rrecv_msg_ptr->isResponseLast());
    }

    if (_rerror == frame::mprpc::error_message_canceled_peer) {
        done();
    }
}
//-----------------------------------------------------------------------------
//      PeerB
//-----------------------------------------------------------------------------

void peerb_connection_start(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());

    auto            msgptr = std::make_shared<Register>("b");
    ErrorConditionT err    = _rctx.service().sendMessage(_rctx.recipientId(), std::move(msgptr), {frame::mprpc::MessageFlagsE::AwaitResponse});
    solid_check(!err, "failed send Register");
}

void peerb_connection_stop(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
}

void peerb_complete_register(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Register>& _rsent_msg_ptr, std::shared_ptr<Register>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());
    solid_check(!_rerror);

    if (_rrecv_msg_ptr && _rrecv_msg_ptr->err == 0) {
        auto lambda = [](frame::mprpc::ConnectionContext&, ErrorConditionT const& _rerror) {
            solid_dbg(generic_logger, Info, "peerb --- enter active error: " << _rerror.message());
        };
        _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId(), lambda);
    } else {
        solid_dbg(generic_logger, Info, "");
    }
}

void peerb_complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    if (_rrecv_msg_ptr) {
        solid_dbg(generic_logger, Info, _rctx.recipientId() << " received message with id on sender " << _rrecv_msg_ptr->senderRequestId() << ' ' << _rrecv_msg_ptr->idx);

        if (!_rrecv_msg_ptr->isOnPeer()) {
            solid_assert(false);
            solid_throw("Message not on peer!.");
        }

        if (!_rrecv_msg_ptr->isRelayed()) {
            solid_assert(false);
            solid_throw("Message not relayed!.");
        }

        // send message back
        if (_rctx.recipientId().isInvalidConnection()) {
            solid_assert(false);
            solid_throw("Connection id should not be invalid!");
        }
        ++_rrecv_msg_ptr->idx;
        if (_rrecv_msg_ptr->idx < 4) {
            ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));
            (void)err;
        } else {
            ErrorConditionT err = _rctx.service().forceCloseConnectionPool(_rctx.recipientId(), [](frame::mprpc::ConnectionContext& _rctx) {});
            (void)err;
        }
    }
    if (_rsent_msg_ptr) {
        solid_dbg(generic_logger, Info, _rctx.recipientId() << " done sent message " << _rsent_msg_ptr.get());
    }
}

void peerb_complete_detect_close(
    frame::mprpc::ConnectionContext&     _rctx,
    std::shared_ptr<DetectCloseMessage>& _rsent_msg_ptr, std::shared_ptr<DetectCloseMessage>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    if (_rrecv_msg_ptr) {
        solid_dbg(generic_logger, Info, _rctx.recipientId() << " received DetectCloseMessage on peerb");

        _rctx.service().sendResponse(_rctx.recipientId(), make_shared<DetectCloseMessage>(*_rrecv_msg_ptr, 1), {frame::mprpc::MessageFlagsE::ResponsePart});
        _rctx.service().sendResponse(_rctx.recipientId(), make_shared<DetectCloseMessage>(*_rrecv_msg_ptr, 2), {frame::mprpc::MessageFlagsE::ResponsePart});
        _rctx.service().sendResponse(_rctx.recipientId(), make_shared<DetectCloseMessage>(*_rrecv_msg_ptr, 3), {frame::mprpc::MessageFlagsE::ResponsePart});
        _rctx.service().sendResponse(_rctx.recipientId(), make_shared<DetectCloseMessage>(*_rrecv_msg_ptr, 4), {frame::mprpc::MessageFlagsE::ResponsePart});
        _rctx.service().sendResponse(_rctx.recipientId(), make_shared<DetectCloseMessage>(*_rrecv_msg_ptr, 5), {frame::mprpc::MessageFlagsE::ResponsePart});
        _rrecv_msg_ptr->idx = 6;
        _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr), {frame::mprpc::MessageFlagsE::ResponsePart});
    }
}
//-----------------------------------------------------------------------------
} // namespace

int test_relay_detect_close(int argc, char* argv[])
{
#ifndef SOLID_ON_WINDOWS
    signal(SIGPIPE, SIG_IGN);
#endif

    solid::log_start(std::cerr, {"solid::frame::mprpc::.*:VIEWX", "\\*:VIEWX"});

    size_t max_per_pool_connection_count = 1;
    bool   secure                        = false;
    bool   compress                      = false;

    {
        AioSchedulerT                         sch_peera;
        AioSchedulerT                         sch_peerb;
        AioSchedulerT                         sch_relay;
        frame::Manager                        m;
        frame::mprpc::relay::SingleNameEngine relay_engine(m); // before relay service because it must overlive it
        frame::mprpc::ServiceT                mprpcrelay(m);
        frame::mprpc::ServiceT                mprpcpeera(m);
        frame::mprpc::ServiceT                mprpcpeerb(m);
        ErrorConditionT                       err;
        CallPoolT                             cwp{1, 100, 0, [](const size_t) {}, [](const size_t) {}};
        frame::aio::Resolver                  resolver([&cwp](std::function<void()>&& _fnc) { cwp.pushOne(std::move(_fnc)); });

        sch_peera.start(1);
        sch_peerb.start(1);
        sch_relay.start(1);

        std::string relay_port;

        { // mprpc relay initialization
            auto con_start = [](frame::mprpc::ConnectionContext& _rctx) {
                solid_dbg(generic_logger, Info, _rctx.recipientId());
            };
            auto con_stop = [](frame::mprpc::ConnectionContext& _rctx) {
            };
            auto con_register = [&relay_engine](
                                    frame::mprpc::ConnectionContext& _rctx,
                                    std::shared_ptr<Register>&       _rsent_msg_ptr,
                                    std::shared_ptr<Register>&       _rrecv_msg_ptr,
                                    ErrorConditionT const&           _rerror) {
                solid_check(!_rerror);
                if (_rrecv_msg_ptr) {
                    solid_check(!_rsent_msg_ptr);
                    solid_dbg(generic_logger, Info, "recv register request: " << _rrecv_msg_ptr->str);

                    relay_engine.registerConnection(_rctx, std::move(_rrecv_msg_ptr->str));

                    _rrecv_msg_ptr->str.clear();
                    ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

                    solid_check(!err, "Failed sending register response: " << err.message());

                } else {
                    solid_check(!_rrecv_msg_ptr);
                    solid_dbg(generic_logger, Info, "sent register response");
                }
            };

            auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
                reflection::v1::metadata::factory,
                [&](auto& _rmap) {
                    _rmap.template registerMessage<Register>(1, "Register", con_register);
                });
            frame::mprpc::Configuration cfg(sch_relay, relay_engine, proto);

            cfg.server.listener_address_str      = "0.0.0.0:0";
            cfg.pool_max_active_connection_count = 10;
            cfg.connection_stop_fnc              = std::move(con_stop);
            cfg.client.connection_start_fnc      = std::move(con_start);
            cfg.client.connection_start_state    = frame::mprpc::ConnectionState::Active;
            cfg.relay_enabled                    = true;

            if (secure) {
                solid_dbg(generic_logger, Info, "Configure SSL server -------------------------------------");
                frame::mprpc::openssl::setup_server(
                    cfg,
                    [](frame::aio::openssl::Context& _rctx) -> ErrorCodeT {
                        _rctx.loadVerifyFile("echo-ca-cert.pem" /*"/etc/pki/tls/certs/ca-bundle.crt"*/);
                        _rctx.loadCertificateFile("echo-server-cert.pem");
                        _rctx.loadPrivateKeyFile("echo-server-key.pem");
                        return ErrorCodeT();
                    },
                    frame::mprpc::openssl::NameCheckSecureStart{"echo-client"});
            }

            if (compress) {
                frame::mprpc::snappy::setup(cfg);
            }

            {
                frame::mprpc::ServiceStartStatus start_status;
                mprpcrelay.start(start_status, std::move(cfg));

                std::ostringstream oss;
                oss << start_status.listen_addr_vec_.back().port();
                relay_port = oss.str();
                solid_dbg(generic_logger, Info, "relay listens on: " << start_status.listen_addr_vec_.back());
            }
        }

        { // mprpc peera initialization
            auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
                reflection::v1::metadata::factory,
                [&](auto& _rmap) {
                    _rmap.template registerMessage<Message>(2, "Message", peera_complete_message);
                    _rmap.template registerMessage<DetectCloseMessage>(3, "DetectCloseMessage", peera_complete_detect_close);
                });
            frame::mprpc::Configuration cfg(sch_peera, proto);

            cfg.connection_stop_fnc           = &peera_connection_stop;
            cfg.client.connection_start_fnc   = &peera_connection_start;
            cfg.client.connection_start_state = frame::mprpc::ConnectionState::Active;

            cfg.pool_max_active_connection_count = max_per_pool_connection_count;

            cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(resolver, relay_port.c_str() /*, SocketInfo::Inet4*/);

            if (secure) {
                solid_dbg(generic_logger, Info, "Configure SSL client ------------------------------------");
                frame::mprpc::openssl::setup_client(
                    cfg,
                    [](frame::aio::openssl::Context& _rctx) -> ErrorCodeT {
                        _rctx.loadVerifyFile("echo-ca-cert.pem" /*"/etc/pki/tls/certs/ca-bundle.crt"*/);
                        _rctx.loadCertificateFile("echo-client-cert.pem");
                        _rctx.loadPrivateKeyFile("echo-client-key.pem");
                        return ErrorCodeT();
                    },
                    frame::mprpc::openssl::NameCheckSecureStart{"echo-server"});
            }

            if (compress) {
                frame::mprpc::snappy::setup(cfg);
            }

            mprpcpeera.start(std::move(cfg));

            if (err) {
                solid_dbg(generic_logger, Error, "starting peera mprpcservice: " << err.message());
                // exiting
                return 1;
            }
        }

        { // mprpc peerb initialization
            auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
                reflection::v1::metadata::factory,
                [&](auto& _rmap) {
                    _rmap.template registerMessage<Register>(1, "Register", peerb_complete_register);
                    _rmap.template registerMessage<Message>(2, "Message", peerb_complete_message);
                    _rmap.template registerMessage<DetectCloseMessage>(3, "DetectCloseMessage", peerb_complete_detect_close);
                });
            frame::mprpc::Configuration cfg(sch_peerb, proto);

            cfg.connection_stop_fnc         = &peerb_connection_stop;
            cfg.client.connection_start_fnc = &peerb_connection_start;

            cfg.pool_max_active_connection_count = max_per_pool_connection_count;

            cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(resolver, relay_port.c_str() /*, SocketInfo::Inet4*/);

            if (secure) {
                solid_dbg(generic_logger, Info, "Configure SSL client ------------------------------------");
                frame::mprpc::openssl::setup_client(
                    cfg,
                    [](frame::aio::openssl::Context& _rctx) -> ErrorCodeT {
                        _rctx.loadVerifyFile("echo-ca-cert.pem" /*"/etc/pki/tls/certs/ca-bundle.crt"*/);
                        _rctx.loadCertificateFile("echo-client-cert.pem");
                        _rctx.loadPrivateKeyFile("echo-client-key.pem");
                        return ErrorCodeT();
                    },
                    frame::mprpc::openssl::NameCheckSecureStart{"echo-server"});
            }

            if (compress) {
                frame::mprpc::snappy::setup(cfg);
            }

            mprpcpeerb.start(std::move(cfg));
        }

        // ensure we have provisioned connections on peerb
        err = mprpcpeerb.createConnectionPool("localhost");
        solid_check(!err, "failed create connection from peerb: " << err.message());

        mprpcpeera.sendMessage("localhost/b", std::make_shared<DetectCloseMessage>(), {frame::mprpc::MessageFlagsE::AwaitResponse});

        mprpcpeera.sendMessage(
            "localhost/b", std::make_shared<Message>(),
            {frame::mprpc::MessageFlagsE::AwaitResponse});

        unique_lock<mutex> lock(mtx);

        if (!cnd.wait_for(lock, std::chrono::seconds(60 * 2), []() { return !running; })) {
            relay_engine.debugDump();
            solid_throw("Process is taking too long.");
        }
    }

    // exiting

    return 0;
}
