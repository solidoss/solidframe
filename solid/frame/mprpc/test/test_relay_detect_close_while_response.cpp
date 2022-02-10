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

#include "solid/utility/workpool.hpp"

#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"

#include <iostream>

#include <signal.h>

using namespace std;
using namespace solid;

using AioSchedulerT  = frame::Scheduler<frame::aio::Reactor>;
using SecureContextT = frame::aio::openssl::Context;

namespace {

bool                    running = true;
mutex                   mtx;
condition_variable      cnd;
frame::mprpc::ServiceT* pmprpcpeera = nullptr;

void done()
{
    lock_guard<mutex> lock(mtx);
    running = false;
    cnd.notify_all();
}

string generate_big_data(const size_t _size)
{
    static struct Pattern {
        string pattern;

        Pattern()
        {
            for (int i = 0; i < 127; ++i) {
                if (isprint(i) && !isblank(i)) {
                    pattern += static_cast<char>(i);
                }
            }
        }
    } pattern;

    string str;
    str.reserve(_size);
    while (str.size() < _size) {
        size_t tocopy = _size - str.size();
        if (tocopy > pattern.pattern.size()) {
            tocopy = pattern.pattern.size();
        }
        str.append(pattern.pattern.data(), tocopy);
    }
    return str;
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
    string   data_;

    DetectCloseMessage() = default;

    DetectCloseMessage(frame::mprpc::Message& _rreq, uint32_t _idx)
        : frame::mprpc::Message(_rreq)
        , idx(_idx)
    {
    }
    DetectCloseMessage(frame::mprpc::Message& _rreq, uint32_t _idx, string&& _data)
        : frame::mprpc::Message(_rreq)
        , idx(_idx)
        , data_(std::move(_data))
    {
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.idx, _rctx, 0, "idx");
        _rr.add(_rthis.data_, _rctx, 1, "data");
    }
};

struct Message : frame::mprpc::Message {
    uint32_t idx = 0;
    string   data_;

    Message() = default;

    Message(uint32_t _index, string&& _data)
        : idx(_index)
        , data_(std::move(_data))
    {
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.idx, _rctx, 0, "idx");
        _rr.add(_rthis.data_, _rctx, 1, "data");
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
        pmprpcpeera->sendMessage("localhost/b", std::make_shared<Message>(_rrecv_msg_ptr->idx, generate_big_data(1024 * 10)));
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

        //send message back
        if (_rctx.recipientId().isInvalidConnection()) {
            solid_assert(false);
            solid_throw("Connection id should not be invalid!");
        }
        if (_rrecv_msg_ptr->idx >= 4) {
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
        _rctx.service().sendResponse(_rctx.recipientId(), make_shared<DetectCloseMessage>(*_rrecv_msg_ptr, 2, generate_big_data(1024 * 1000)), {frame::mprpc::MessageFlagsE::ResponsePart});
        _rctx.service().sendResponse(_rctx.recipientId(), make_shared<DetectCloseMessage>(*_rrecv_msg_ptr, 3, generate_big_data(1024 * 100)), {frame::mprpc::MessageFlagsE::ResponsePart});
        _rctx.service().sendResponse(_rctx.recipientId(), make_shared<DetectCloseMessage>(*_rrecv_msg_ptr, 4, generate_big_data(1024 * 10)), {frame::mprpc::MessageFlagsE::ResponsePart});
        _rrecv_msg_ptr->idx   = 5;
        _rrecv_msg_ptr->data_ = generate_big_data(1024 * 1024 * 100); //100MB
        _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr), {frame::mprpc::MessageFlagsE::ResponsePart});
    }
}
//-----------------------------------------------------------------------------
} //namespace

int test_relay_detect_close_while_response(int argc, char* argv[])
{
#ifndef SOLID_ON_WINDOWS
    signal(SIGPIPE, SIG_IGN);
#endif

    solid::log_start(std::cerr, {"solid::frame::mprpc::.*:EWX", "\\*:VIEWX"});

    size_t max_per_pool_connection_count = 1;
    bool   secure                        = false;
    bool   compress                      = false;

    {
        AioSchedulerT                         sch_peera;
        AioSchedulerT                         sch_peerb;
        AioSchedulerT                         sch_relay;
        frame::Manager                        m;
        frame::mprpc::relay::SingleNameEngine relay_engine(m); //before relay service because it must overlive it
        frame::mprpc::ServiceT                mprpcrelay(m);
        frame::mprpc::ServiceT                mprpcpeera(m);
        frame::mprpc::ServiceT                mprpcpeerb(m);
        ErrorConditionT                       err;
        lockfree::CallPoolT<void(), void>     cwp{WorkPoolConfiguration(1)};
        frame::aio::Resolver                  resolver([&cwp](std::function<void()>&& _fnc) { cwp.push(std::move(_fnc)); });

        sch_peera.start(1);
        sch_peerb.start(1);
        sch_relay.start(1);

        std::string relay_port;

        { //mprpc relay initialization
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

            mprpcrelay.start(std::move(cfg));

            {
                std::ostringstream oss;
                oss << mprpcrelay.configuration().server.listenerPort();
                relay_port = oss.str();
                solid_dbg(generic_logger, Info, "relay listens on port: " << relay_port);
            }
        }

        { //mprpc peera initialization
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
                //exiting
                return 1;
            }
        }

        { //mprpc peerb initialization
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

        //ensure we have provisioned connections on peerb
        err = mprpcpeerb.createConnectionPool("localhost");
        solid_check(!err, "failed create connection from peerb: " << err.message());

        pmprpcpeera = &mprpcpeera;
        mprpcpeera.sendMessage("localhost/b", std::make_shared<DetectCloseMessage>(), {frame::mprpc::MessageFlagsE::AwaitResponse});

        unique_lock<mutex> lock(mtx);

        if (!cnd.wait_for(lock, std::chrono::seconds(60 * 2), []() { return !running; })) {
            relay_engine.debugDump();
            solid_throw("Process is taking too long.");
        }
    }

    //exiting

    return 0;
}
