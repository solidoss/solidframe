#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioresolver.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include "solid/frame/aio/openssl/aiosecurecontext.hpp"
#include "solid/frame/aio/openssl/aiosecuresocket.hpp"

#include "solid/frame/mpipc/mpipccompression_snappy.hpp"
#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v2.hpp"
#include "solid/frame/mpipc/mpipcrelayengines.hpp"
#include "solid/frame/mpipc/mpipcservice.hpp"
#include "solid/frame/mpipc/mpipcsocketstub_openssl.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "solid/system/exception.hpp"

#include "solid/system/log.hpp"

#include <iostream>

#include <signal.h>

using namespace std;
using namespace solid;

using AioSchedulerT  = frame::Scheduler<frame::aio::Reactor>;
using SecureContextT = frame::aio::openssl::Context;
using ProtocolT      = frame::mpipc::serialization_v2::Protocol<uint8_t>;

namespace {

struct InitStub {
    size_t                      size;
    frame::mpipc::MessageFlagsT flags;
    bool                        cancel;
};

InitStub initarray[] = {
    {80000000, 0, false}, //
    {80240000, 0, false}, //
    {80960000, 0, false}, //
    {16384000, 0, true}}; //

using MessageIdT       = std::pair<frame::mpipc::RecipientId, frame::mpipc::MessageId>;
using MessageIdVectorT = std::deque<MessageIdT>;

std::string            pattern;
const size_t           initarraysize = sizeof(initarray) / sizeof(InitStub);
std::atomic<size_t>    crtwriteidx(0);
std::atomic<size_t>    writecount(0);
std::atomic<size_t>    created_count(0);
std::atomic<size_t>    canceled_count(0);
std::atomic<size_t>    deleted_count(0);
size_t                 connection_count(0);
bool                   running = true;
mutex                  mtx;
condition_variable     cnd;
frame::mpipc::Service* pmpipcpeera = nullptr;
frame::mpipc::Service* pmpipcpeerb = nullptr;
std::atomic<uint64_t>  transfered_size(0);
std::atomic<size_t>    transfered_count(0);
MessageIdVectorT       msgid_vec;

bool try_stop()
{
    //writecount messages were sent by peera
    //cancelable_created_count were realy canceld on peera
    //2xcancelable_created_count == cancelable_deleted_count
    //
    solid_dbg(generic_logger, Error, "writeidx = " << crtwriteidx << " writecnt = " << writecount << " canceled_cnt = " << canceled_count << " create_cnt = " << created_count << " deleted_cnt = " << deleted_count);
    if (
        crtwriteidx >= writecount && canceled_count == writecount && created_count == deleted_count && created_count == 2 * writecount) {
        lock_guard<mutex> lock(mtx);
        running = false;
        cnd.notify_one();
        return true;
    }
    return false;
}

size_t real_size(size_t _sz)
{
    //offset + (align - (offset mod align)) mod align
    return _sz + ((sizeof(uint64_t) - (_sz % sizeof(uint64_t))) % sizeof(uint64_t));
}

struct Register : frame::mpipc::Message {
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

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.err, _rctx, "err").add(_rthis.str, _rctx, "str");
    }
};

struct Message : frame::mpipc::Message {
    uint32_t     idx;
    std::string  str;
    mutable bool serialized;

    Message(uint32_t _idx)
        : idx(_idx)
        , serialized(false)
    {
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << (void*)this << " idx = " << idx);
        init();
        ++created_count;
    }
    Message()
        : serialized(false)
    {
        ++created_count;
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << (void*)this);
    }
    ~Message()
    {
        solid_dbg(generic_logger, Info, "DELETE ---------------- " << (void*)this << " idx = " << idx);

        ++deleted_count;
        try_stop();
    }

    bool cancelable() const
    {
        return initarray[idx % initarraysize].cancel;
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.idx, _rctx, "idx");

        if (_s.is_deserializer) {
            _s.add([&_rthis](S& _rs, frame::mpipc::ConnectionContext& _rctx, const char* _name) {
                if (_rthis.cancelable()) {
                    solid_dbg(generic_logger, Error, "Close connection: " << _rthis.idx << " " << msgid_vec[_rthis.idx].first);
                    //we're on the peerb,
                    //we now cancel the message on peer a
                    pmpipcpeera->forceCloseConnectionPool(
                        msgid_vec[_rthis.idx].first,
                        [](frame::mpipc::ConnectionContext& _rctx) {
                            static int cnt = 0;
                            ++cnt;
                            SOLID_CHECK(cnt == 1, "connection pool callback called twice");
                            SOLID_ASSERT(cnt == 1);
                            solid_dbg(generic_logger, Error, "Close pool callback");
                        });
                }
            },
                _rctx, _name);
        }

        _s.add(_rthis.str, _rctx, "str");

        if (_s.is_serializer) {
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
            pu[i] = pup[(idx + i) % pattern_size]; //pattern[i % pattern.size()];
        }
    }

    bool check() const
    {
        const size_t sz = real_size(initarray[idx % initarraysize].size);
        solid_dbg(generic_logger, Info, "str.size = " << str.size() << " should be equal to " << sz);
        if (sz != str.size()) {
            return false;
        }
        //return true;
        const size_t    count        = sz / sizeof(uint64_t);
        const uint64_t* pu           = reinterpret_cast<const uint64_t*>(str.data());
        const uint64_t* pup          = reinterpret_cast<const uint64_t*>(pattern.data());
        const size_t    pattern_size = pattern.size() / sizeof(uint64_t);

        for (uint64_t i = 0; i < count; ++i) {
            if (pu[i] != pup[(i + idx) % pattern_size]) {
                SOLID_THROW("Message check failed.");
                return false;
            }
        }
        return true;
    }
};

//-----------------------------------------------------------------------------
//      PeerA
//-----------------------------------------------------------------------------

void peera_connection_start(frame::mpipc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());
}

void peera_connection_stop(frame::mpipc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
    if (!running) {
        ++connection_count;
    }
}

void peera_complete_message(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rerror.message());
    SOLID_CHECK(_rsent_msg_ptr, "Error: no request message");

    SOLID_CHECK(!_rrecv_msg_ptr, "Error: there should be no response");
    ++canceled_count;
    SOLID_CHECK(_rerror, "Error: there should be an error");

    try_stop();
}

//-----------------------------------------------------------------------------
//      PeerB
//-----------------------------------------------------------------------------

void peerb_connection_start(frame::mpipc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());

    auto            msgptr = std::make_shared<Register>("b");
    ErrorConditionT err    = _rctx.service().sendMessage(_rctx.recipientId(), std::move(msgptr), {frame::mpipc::MessageFlagsE::WaitResponse});
    SOLID_CHECK(!err, "failed send Register");
}

void peerb_connection_stop(frame::mpipc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
}

void peerb_complete_register(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<Register>& _rsent_msg_ptr, std::shared_ptr<Register>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());
    SOLID_CHECK(!_rerror);

    if (_rrecv_msg_ptr && _rrecv_msg_ptr->err == 0) {
        auto lambda = [](frame::mpipc::ConnectionContext&, ErrorConditionT const& _rerror) {
            solid_dbg(generic_logger, Info, "peerb --- enter active error: " << _rerror.message());
            return frame::mpipc::MessagePointerT();
        };
        _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId(), lambda);
    } else {
        solid_dbg(generic_logger, Info, "");
    }
}

void peerb_complete_message(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    if (_rrecv_msg_ptr) {
        solid_dbg(generic_logger, Info, _rctx.recipientId() << " received message with id on sender " << _rrecv_msg_ptr->senderRequestId() << " datasz = " << _rrecv_msg_ptr->str.size());

        if (!_rrecv_msg_ptr->check()) {
            SOLID_ASSERT(false);
            SOLID_THROW("Message check failed.");
        }

        if (!_rrecv_msg_ptr->isOnPeer()) {
            SOLID_ASSERT(false);
            SOLID_THROW("Message not on peer!.");
        }

        if (!_rrecv_msg_ptr->isRelayed()) {
            SOLID_ASSERT(false);
            SOLID_THROW("Message not relayed!.");
        }

        //send message back
        if (_rctx.recipientId().isInvalidConnection()) {
            SOLID_ASSERT(false);
            SOLID_THROW("Connection id should not be invalid!");
        }
        //no need to send back the response
        //ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

        //SOLID_CHECK(!err, "Connection id should not be invalid! " << err.message());
    }
    if (_rsent_msg_ptr) {
        solid_dbg(generic_logger, Info, _rctx.recipientId() << " done sent message " << _rsent_msg_ptr.get());
    }
}
//-----------------------------------------------------------------------------
} //namespace

int test_relay_close_request(int argc, char* argv[])
{
#ifndef SOLID_ON_WINDOWS
    signal(SIGPIPE, SIG_IGN);
#endif

    solid::log_start(std::cerr, {".*:EW"});

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
        frame::mpipc::relay::SingleNameEngine relay_engine(m); //before relay service because it must overlive it
        frame::mpipc::ServiceT                mpipcrelay(m);
        frame::mpipc::ServiceT                mpipcpeera(m);
        frame::mpipc::ServiceT                mpipcpeerb(m);
        ErrorConditionT                       err;
        FunctionWorkPool                      fwp;
        frame::aio::Resolver                  resolver(fwp);

        fwp.start(WorkPoolConfiguration());

        err = sch_peera.start(1);

        if (err) {
            solid_dbg(generic_logger, Error, "starting aio peera scheduler: " << err.message());
            return 1;
        }

        err = sch_peerb.start(1);

        if (err) {
            solid_dbg(generic_logger, Error, "starting aio peerb scheduler: " << err.message());
            return 1;
        }

        err = sch_relay.start(1);

        if (err) {
            solid_dbg(generic_logger, Error, "starting aio relay scheduler: " << err.message());
            return 1;
        }

        std::string relay_port;

        { //mpipc relay initialization
            auto con_start = [&relay_engine](frame::mpipc::ConnectionContext& _rctx) {
                solid_dbg(generic_logger, Info, _rctx.recipientId());
            };
            auto con_stop = [&relay_engine](frame::mpipc::ConnectionContext& _rctx) {
                if (!running) {
                    ++connection_count;
                }
            };
            auto con_register = [&relay_engine](
                                    frame::mpipc::ConnectionContext& _rctx,
                                    std::shared_ptr<Register>&       _rsent_msg_ptr,
                                    std::shared_ptr<Register>&       _rrecv_msg_ptr,
                                    ErrorConditionT const&           _rerror) {
                SOLID_CHECK(!_rerror);
                if (_rrecv_msg_ptr) {
                    SOLID_CHECK(!_rsent_msg_ptr);
                    solid_dbg(generic_logger, Info, "recv register request: " << _rrecv_msg_ptr->str);

                    relay_engine.registerConnection(_rctx, std::move(_rrecv_msg_ptr->str));

                    _rrecv_msg_ptr->str.clear();
                    ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

                    SOLID_CHECK(!err, "Failed sending register response: " << err.message());

                } else {
                    SOLID_CHECK(!_rrecv_msg_ptr);
                    solid_dbg(generic_logger, Info, "sent register response");
                }
            };

            auto                        proto = ProtocolT::create();
            frame::mpipc::Configuration cfg(sch_relay, relay_engine, proto);

            proto->null(0);
            proto->registerMessage<Register>(con_register, 1);

            cfg.server.listener_address_str      = "0.0.0.0:0";
            cfg.pool_max_active_connection_count = 2 * max_per_pool_connection_count;
            cfg.connection_stop_fnc              = std::move(con_stop);
            cfg.client.connection_start_fnc      = std::move(con_start);
            cfg.client.connection_start_state    = frame::mpipc::ConnectionState::Active;
            cfg.relay_enabled                    = true;

            if (secure) {
                solid_dbg(generic_logger, Info, "Configure SSL server -------------------------------------");
                frame::mpipc::openssl::setup_server(
                    cfg,
                    [](frame::aio::openssl::Context& _rctx) -> ErrorCodeT {
                        _rctx.loadVerifyFile("echo-ca-cert.pem" /*"/etc/pki/tls/certs/ca-bundle.crt"*/);
                        _rctx.loadCertificateFile("echo-server-cert.pem");
                        _rctx.loadPrivateKeyFile("echo-server-key.pem");
                        return ErrorCodeT();
                    },
                    frame::mpipc::openssl::NameCheckSecureStart{"echo-client"});
            }

            if (compress) {
                frame::mpipc::snappy::setup(cfg);
            }

            err = mpipcrelay.reconfigure(std::move(cfg));

            if (err) {
                solid_dbg(generic_logger, Error, "starting server mpipcservice: " << err.message());
                //exiting
                return 1;
            }

            {
                std::ostringstream oss;
                oss << mpipcrelay.configuration().server.listenerPort();
                relay_port = oss.str();
                solid_dbg(generic_logger, Info, "relay listens on port: " << relay_port);
            }
        }

        pmpipcpeera = &mpipcpeera;
        pmpipcpeerb = &mpipcpeerb;

        { //mpipc peera initialization
            auto                        proto = ProtocolT::create();
            frame::mpipc::Configuration cfg(sch_peera, proto);

            proto->null(0);
            proto->registerMessage<Message>(peera_complete_message, 2);

            cfg.connection_stop_fnc           = &peera_connection_stop;
            cfg.client.connection_start_fnc   = &peera_connection_start;
            cfg.client.connection_start_state = frame::mpipc::ConnectionState::Active;

            cfg.pool_max_active_connection_count = max_per_pool_connection_count;

            cfg.client.name_resolve_fnc = frame::mpipc::InternetResolverF(resolver, relay_port.c_str() /*, SocketInfo::Inet4*/);

            if (secure) {
                solid_dbg(generic_logger, Info, "Configure SSL client ------------------------------------");
                frame::mpipc::openssl::setup_client(
                    cfg,
                    [](frame::aio::openssl::Context& _rctx) -> ErrorCodeT {
                        _rctx.loadVerifyFile("echo-ca-cert.pem" /*"/etc/pki/tls/certs/ca-bundle.crt"*/);
                        _rctx.loadCertificateFile("echo-client-cert.pem");
                        _rctx.loadPrivateKeyFile("echo-client-key.pem");
                        return ErrorCodeT();
                    },
                    frame::mpipc::openssl::NameCheckSecureStart{"echo-server"});
            }

            if (compress) {
                frame::mpipc::snappy::setup(cfg);
            }

            err = mpipcpeera.reconfigure(std::move(cfg));

            if (err) {
                solid_dbg(generic_logger, Error, "starting peera mpipcservice: " << err.message());
                //exiting
                return 1;
            }
        }

        { //mpipc peerb initialization
            auto                        proto = ProtocolT::create();
            frame::mpipc::Configuration cfg(sch_peerb, proto);

            proto->null(0);
            proto->registerMessage<Register>(peerb_complete_register, 1);
            proto->registerMessage<Message>(peerb_complete_message, 2);

            cfg.connection_stop_fnc         = &peerb_connection_stop;
            cfg.client.connection_start_fnc = &peerb_connection_start;

            cfg.pool_max_active_connection_count = max_per_pool_connection_count;

            cfg.client.name_resolve_fnc = frame::mpipc::InternetResolverF(resolver, relay_port.c_str() /*, SocketInfo::Inet4*/);

            if (secure) {
                solid_dbg(generic_logger, Info, "Configure SSL client ------------------------------------");
                frame::mpipc::openssl::setup_client(
                    cfg,
                    [](frame::aio::openssl::Context& _rctx) -> ErrorCodeT {
                        _rctx.loadVerifyFile("echo-ca-cert.pem" /*"/etc/pki/tls/certs/ca-bundle.crt"*/);
                        _rctx.loadCertificateFile("echo-client-cert.pem");
                        _rctx.loadPrivateKeyFile("echo-client-key.pem");
                        return ErrorCodeT();
                    },
                    frame::mpipc::openssl::NameCheckSecureStart{"echo-server"});
            }

            if (compress) {
                frame::mpipc::snappy::setup(cfg);
            }

            err = mpipcpeerb.reconfigure(std::move(cfg));

            if (err) {
                solid_dbg(generic_logger, Error, "starting peerb mpipcservice: " << err.message());
                //exiting
                return 1;
            }
        }

        const size_t start_count = initarraysize;

        writecount = initarraysize; //start_count;//

        //ensure we have provisioned connections on peerb
        err = mpipcpeerb.createConnectionPool("localhost");
        SOLID_CHECK(!err, "failed create connection from peerb: " << err.message());

        for (; crtwriteidx < start_count;) {
            mtx.lock();
            msgid_vec.emplace_back();
            auto& back_msg_id = msgid_vec.back();
            mtx.unlock();
            mpipcpeera.sendMessage(
                "localhost/b", std::make_shared<Message>(crtwriteidx++),
                back_msg_id.first,
                back_msg_id.second,
                initarray[crtwriteidx % initarraysize].flags | frame::mpipc::MessageFlagsE::WaitResponse);
        }

        unique_lock<mutex> lock(mtx);

        if (!cnd.wait_for(lock, std::chrono::seconds(50), []() { return !running; })) {
            relay_engine.debugDump();
            SOLID_THROW("Process is taking too long.");
        }
    }

    //exiting

    std::cout << "Transfered size = " << (transfered_size * 2) / 1024 << "KB" << endl;
    std::cout << "Transfered count = " << transfered_count << endl;
    std::cout << "Connection count = " << connection_count << endl;

    return 0;
}
