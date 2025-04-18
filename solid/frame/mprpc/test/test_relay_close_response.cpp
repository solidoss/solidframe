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
using AioSchedulerT  = frame::Scheduler<frame::aio::Reactor<frame::mprpc::EventT>>;
using SecureContextT = frame::aio::openssl::Context;
using CallPoolT      = ThreadPool<Function<void()>, Function<void()>>;

struct InitStub {
    size_t                      size;
    frame::mprpc::MessageFlagsT flags;
    bool                        cancel;
};

InitStub initarray[] = {
    {81000000, 0, false}, //
    {61240000, 0, false}, //
    {41960000, 0, false}, //
    {21384000, 0, true}}; //

using MessageIdT       = std::pair<frame::mprpc::RecipientId, frame::mprpc::MessageId>;
using MessageIdVectorT = std::deque<MessageIdT>;

std::string            pattern;
const size_t           initarraysize = sizeof(initarray) / sizeof(InitStub);
std::atomic<size_t>    crtwriteidx(0);
std::atomic<size_t>    writecount(0);
std::atomic<size_t>    created_count(0);
std::atomic<size_t>    canceled_count(0);
std::atomic<size_t>    deleted_count(0);
std::atomic<size_t>    back_on_sender_count(0);
std::atomic<size_t>    success_count(0);
size_t                 connection_count(0);
std::atomic<bool>      running = true;
mutex                  mtx;
condition_variable     cnd;
frame::mprpc::Service* pmprpcpeera = nullptr;
frame::mprpc::Service* pmprpcpeerb = nullptr;
std::atomic<uint64_t>  transfered_size(0);
std::atomic<size_t>    transfered_count(0);
MessageIdVectorT       msgid_vec;

bool try_stop()
{
    // writecount messages were sent by peera
    // cancelable_created_count were realy canceld on peera
    // 2xcancelable_created_count == cancelable_deleted_count
    //
    solid_dbg(generic_logger, Error, "writeidx = " << crtwriteidx << " writecnt = " << writecount << " canceled_cnt = " << canceled_count << " create_cnt = " << created_count << " deleted_cnt = " << deleted_count);
    if (
        crtwriteidx >= writecount && (canceled_count + success_count) == writecount && created_count == deleted_count && created_count == 3 * writecount) {
        lock_guard<mutex> lock(mtx);
        running = false;
        cnd.notify_one();
        solid_check(canceled_count != 0);
        return true;
    }
    return false;
}

size_t real_size(size_t _sz)
{
    // offset + (align - (offset mod align)) mod align
    return _sz + ((sizeof(uint64_t) - (_sz % sizeof(uint64_t))) % sizeof(uint64_t));
}

struct Register : frame::mprpc::Message {
    uint32_t err_        = 0;
    uint32_t group_id_   = 0;
    uint16_t replica_id_ = 0;

    Register(const uint32_t _group_id, uint32_t _err = 0)
        : err_(_err)
        , group_id_(_group_id)
    {
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << this);
    }
    Register()
        : err_(-1)
    {
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << this);
    }

    ~Register()
    {
        solid_dbg(generic_logger, Info, "DELETE ---------------- " << this);
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.err_, _rctx, 0, "err").add(_rthis.group_id_, _rctx, 1, "group_id");
        _rr.add(_rthis.replica_id_, _rctx, 2, "replica_id");
    }
};

struct Message : frame::mprpc::Message {
    uint32_t     idx;
    std::string  str;
    mutable bool serialized;

    Message(uint32_t _idx)
        : idx(_idx)
        , serialized(false)
    {
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << this << " idx = " << idx);
        init();
        ++created_count;
    }
    Message()
        : serialized(false)
    {
        ++created_count;
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << this);
    }
    ~Message()
    {
        solid_dbg(generic_logger, Info, "DELETE ---------------- " << this);

        ++deleted_count;
        try_stop();
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.idx, _rctx, 1, "idx");
        _rr.add([&_rthis](Reflector& _rr, frame::mprpc::ConnectionContext& _rctx) {
            if (_rthis.isBackOnSender()) {
                ++back_on_sender_count;
                solid_assert(back_on_sender_count <= writecount);
                if (back_on_sender_count == writecount) {
                    lock_guard<mutex> lock(mtx);
                    solid_dbg(generic_logger, Info, "Close connection: " << _rthis.idx << " " << msgid_vec[_rthis.idx].first);
                    // we're on the peerb,
                    // we now cancel the message on peer a
                    pmprpcpeera->forceCloseConnectionPool(
                        msgid_vec[_rthis.idx].first,
                        [](frame::mprpc::ConnectionContext& _rctx) {
                            solid_dbg(generic_logger, Error, "Close pool callback");
                        });
                }
            }
        },
            _rctx);

        _rr.add(_rthis.str, _rctx, 2, "str");

        if constexpr (Reflector::is_const_reflector) {
            _rthis.serialized = true;
        }
    }

    void init()
    {
        const size_t sz = real_size(initarray[idx % initarraysize].size);
        str.resize(sz);
        const size_t    count        = sz / sizeof(uint64_t);
        uint64_t*       pu           = reinterpret_cast<uint64_t*>(const_cast<char*>(str.data()));
        const uint64_t* pup          = reinterpret_cast<const uint64_t*>(pattern.data());
        const size_t    pattern_size = pattern.size() / sizeof(uint64_t);
        for (uint64_t i = 0; i < count; ++i) {
            pu[i] = pup[(idx + i) % pattern_size]; // pattern[i % pattern.size()];
        }
    }

    bool check() const
    {
        const size_t sz = real_size(initarray[idx % initarraysize].size);
        solid_dbg(generic_logger, Info, "str.size = " << str.size() << " should be equal to " << sz);
        if (sz != str.size()) {
            return false;
        }
        // return true;
        const size_t    count        = sz / sizeof(uint64_t);
        const uint64_t* pu           = reinterpret_cast<const uint64_t*>(str.data());
        const uint64_t* pup          = reinterpret_cast<const uint64_t*>(pattern.data());
        const size_t    pattern_size = pattern.size() / sizeof(uint64_t);

        for (uint64_t i = 0; i < count; ++i) {
            if (pu[i] != pup[(i + idx) % pattern_size]) {
                solid_throw("Message check failed.");
                return false;
            }
        }
        return true;
    }
};

using RegisterPointerT = solid::frame::mprpc::MessagePointerT<Register>;
using MessagePointerT  = solid::frame::mprpc::MessagePointerT<Message>;

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
    if (!running) {
        ++connection_count;
    }
}

void peera_complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    MessagePointerT& _rsent_msg_ptr, MessagePointerT& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rerror.message());

    solid_check(_rsent_msg_ptr, "Error: there should be a request message");
    if (_rerror) {
        solid_check(!_rrecv_msg_ptr, "Error: there should be no response");
        ++canceled_count;
    } else {
        ++success_count;
        solid_check(_rrecv_msg_ptr, "Error: there should be a response");
        solid_check(_rrecv_msg_ptr->check());
    }

    try_stop();
}

//-----------------------------------------------------------------------------
//      PeerB
//-----------------------------------------------------------------------------

void peerb_connection_start(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());

    auto            msgptr = frame::mprpc::make_message<Register>(0);
    ErrorConditionT err    = _rctx.service().sendMessage(_rctx.recipientId(), std::move(msgptr), {frame::mprpc::MessageFlagsE::AwaitResponse});
    solid_check(!err, "failed send Register");
}

void peerb_connection_stop(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
}

void peerb_complete_register(
    frame::mprpc::ConnectionContext& _rctx,
    RegisterPointerT& _rsent_msg_ptr, RegisterPointerT& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());
    solid_check(!_rerror);

    if (_rrecv_msg_ptr && _rrecv_msg_ptr->err_ == 0) {
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
    MessagePointerT& _rsent_msg_ptr, MessagePointerT& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    if (_rrecv_msg_ptr) {
        solid_dbg(generic_logger, Info, _rctx.recipientId() << " received message with id on sender " << _rrecv_msg_ptr->senderRequestId() << " datasz = " << _rrecv_msg_ptr->str.size());

        if (!_rrecv_msg_ptr->check()) {
            solid_assert(false);
            solid_throw("Message check failed.");
        }

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

        ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));
        (void)err;
    }
    if (_rsent_msg_ptr) {
        solid_dbg(generic_logger, Info, _rctx.recipientId() << " done sent message " << _rsent_msg_ptr.get());
    }
}
//-----------------------------------------------------------------------------
} // namespace

int test_relay_close_response(int argc, char* argv[])
{
#ifndef SOLID_ON_WINDOWS
    signal(SIGPIPE, SIG_IGN);
#endif

    solid::log_start(std::cerr, {".*:EWX"});

    size_t max_per_pool_connection_count = 1;

    if (argc > 1) {
        max_per_pool_connection_count = atoi(argv[1]);
        if (max_per_pool_connection_count == 0) {
            max_per_pool_connection_count = 1;
        }
        if (max_per_pool_connection_count > 100) {
            max_per_pool_connection_count = 100;
        }
    }

    bool secure   = false;
    bool compress = false;

    if (argc > 2) {
        if (*argv[2] == 's' || *argv[2] == 'S') {
            secure = true;
        }
        if (*argv[2] == 'c' || *argv[2] == 'C') {
            compress = true;
        }
        if (*argv[2] == 'b' || *argv[2] == 'B') {
            secure   = true;
            compress = true;
        }
    }

    for (int j = 0; j < 1; ++j) {
        for (int i = 0; i < 127; ++i) {
            int c = (i + j) % 127;
            if (isprint(c) && !isblank(c)) {
                pattern += static_cast<char>(c);
            }
        }
    }

    size_t sz = real_size(pattern.size());

    if (sz > pattern.size()) {
        pattern.resize(sz - sizeof(uint64_t));
    } else if (sz < pattern.size()) {
        pattern.resize(sz);
    }

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
        CallPoolT                             cwp{{1, 100, 0}, [](const size_t) {}, [](const size_t) {}};
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
                if (!running) {
                    ++connection_count;
                }
            };
            auto con_register = [&relay_engine](
                                    frame::mprpc::ConnectionContext& _rctx,
                                    RegisterPointerT&                _rsent_msg_ptr,
                                    RegisterPointerT&                _rrecv_msg_ptr,
                                    ErrorConditionT const&           _rerror) {
                solid_check(!_rerror);
                if (_rrecv_msg_ptr) {
                    solid_check(!_rsent_msg_ptr);
                    solid_dbg(generic_logger, Info, "recv register request: " << _rrecv_msg_ptr->group_id_ << ", " << _rrecv_msg_ptr->replica_id_);

                    relay_engine.registerConnection(_rctx, _rrecv_msg_ptr->group_id_, _rrecv_msg_ptr->replica_id_);
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
            cfg.pool_max_active_connection_count = 2 * max_per_pool_connection_count;
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

        pmprpcpeera = &mprpcpeera;
        pmprpcpeerb = &mprpcpeerb;

        { // mprpc peera initialization
            auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
                reflection::v1::metadata::factory,
                [&](auto& _rmap) {
                    _rmap.template registerMessage<Message>(2, "Message", peera_complete_message);
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

        const size_t start_count = initarraysize;

        writecount = initarraysize; // initarraysize * 2; //start_count;//

        // ensure we have provisioned connections on peerb
        err = mprpcpeerb.createConnectionPool("localhost");
        solid_check(!err, "failed create connection from peerb: " << err.message());

        for (; crtwriteidx < start_count;) {
            mtx.lock();
            msgid_vec.emplace_back();
            auto& back_msg_id = msgid_vec.back();
            mtx.unlock();
            mprpcpeera.sendMessage(
                {"localhost", 0}, frame::mprpc::make_message<Message>(crtwriteidx++),
                back_msg_id.first,
                back_msg_id.second,
                initarray[crtwriteidx % initarraysize].flags | frame::mprpc::MessageFlagsE::AwaitResponse);
        }

        unique_lock<mutex> lock(mtx);

        if (!cnd.wait_for(lock, std::chrono::seconds(60 * 2), []() { return !running; })) {
            relay_engine.debugDump();
            solid_throw("Process is taking too long.");
        }
    }

    // exiting

    std::cout << "Transfered size = " << (transfered_size * 2) / 1024 << "KB" << endl;
    std::cout << "Transfered count = " << transfered_count << endl;
    std::cout << "Connection count = " << connection_count << endl;

    return 0;
}
