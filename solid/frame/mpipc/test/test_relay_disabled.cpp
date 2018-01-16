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
#include "solid/frame/mpipc/mpipcprotocol_serialization_v1.hpp"
#include "solid/frame/mpipc/mpipcservice.hpp"
#include "solid/frame/mpipc/mpipcsocketstub_openssl.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/system/exception.hpp"

#include "solid/system/debug.hpp"

#include <iostream>

using namespace std;
using namespace solid;

typedef frame::Scheduler<frame::aio::Reactor> AioSchedulerT;
typedef frame::aio::openssl::Context          SecureContextT;

namespace {

struct InitStub {
    size_t                      size;
    frame::mpipc::MessageFlagsT flags;
};

InitStub initarray[] = {
    {100000, 0},
    {100, 0},
    {200, 0},
    {400, 0},
    {800, 0},
    {1000, 0},
    {2000, 0},
    {4000, 0},
    {8000, 0},
    {16000, 0},
    {32000, 0},
    {64000, 0},
    {128000, 0},
    {256000, 0},
    {512000, 0},
    {1024000, 0},
    {2048000, 0},
    {4096000, 0},
    {8192000, 0},
    {16384000, 0}};

std::string  pattern;
const size_t initarraysize = sizeof(initarray) / sizeof(InitStub);

std::atomic<size_t> crtwriteidx(0);
std::atomic<size_t> crtreadidx(0);
std::atomic<size_t> crtbackidx(0);
std::atomic<size_t> crtackidx(0);
std::atomic<size_t> writecount(0);

size_t connection_count(0);

bool                   running = true;
mutex                  mtx;
condition_variable     cnd;
frame::mpipc::Service* pmpipcpeera = nullptr;
frame::mpipc::Service* pmpipcpeerb = nullptr;
std::atomic<uint64_t>  transfered_size(0);
std::atomic<size_t>    transfered_count(0);

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
        idbg("CREATE ---------------- " << (void*)this);
    }
    Register(uint32_t _err = -1)
        : err(_err)
    {
    }

    ~Register()
    {
        idbg("DELETE ---------------- " << (void*)this);
    }

    template <class S>
    void solidSerialize(S& _s, frame::mpipc::ConnectionContext& _rctx)
    {
        _s.push(str, "str");
        _s.push(err, "err");
    }
};

struct Message : frame::mpipc::Message {
    uint32_t    idx;
    std::string str;
    bool        serialized;

    Message(uint32_t _idx)
        : idx(_idx)
        , serialized(false)
    {
        idbg("CREATE ---------------- " << (void*)this << " idx = " << idx);
        init();
    }
    Message()
        : serialized(false)
    {
        idbg("CREATE ---------------- " << (void*)this);
    }
    ~Message()
    {
        idbg("DELETE ---------------- " << (void*)this);

        //SOLID_ASSERT(serialized or this->isBackOnSender());
    }

    template <class S>
    void solidSerialize(S& _s, frame::mpipc::ConnectionContext& _rctx)
    {
        _s.push(str, "str");
        _s.push(idx, "idx");

        if (S::IsSerializer) {
            serialized = true;
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
        idbg("str.size = " << str.size() << " should be equal to " << sz);
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
    idbg(_rctx.recipientId());
}

void peera_connection_stop(frame::mpipc::ConnectionContext& _rctx)
{
    idbg(_rctx.recipientId() << " error: " << _rctx.error().message());
    if (!running) {
        ++connection_count;
    }
}

void peera_complete_message(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    idbg(_rctx.recipientId() << " error: " << _rerror.message());
    SOLID_CHECK(!_rrecv_msg_ptr, "should not receive any message");
    SOLID_CHECK(_rsent_msg_ptr, "sent message should not be null");
    SOLID_CHECK(_rerror == frame::mpipc::error_message_canceled_peer, "message should be canceled by peer");

    ++crtackidx;
    ++crtbackidx;

    if (crtbackidx == writecount) {
        unique_lock<mutex> lock(mtx);
        running = false;
        cnd.notify_one();
    }
}

//-----------------------------------------------------------------------------
//      PeerB
//-----------------------------------------------------------------------------

void peerb_connection_start(frame::mpipc::ConnectionContext& _rctx)
{
    idbg(_rctx.recipientId());

    auto            msgptr = std::make_shared<Register>("b");
    ErrorConditionT err    = _rctx.service().sendMessage(_rctx.recipientId(), std::move(msgptr), {frame::mpipc::MessageFlagsE::WaitResponse});
    SOLID_CHECK(not err, "failed send Register");
}

void peerb_connection_stop(frame::mpipc::ConnectionContext& _rctx)
{
    idbg(_rctx.recipientId() << " error: " << _rctx.error().message());
}

void peerb_complete_register(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<Register>& _rsent_msg_ptr, std::shared_ptr<Register>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    idbg(_rctx.recipientId());
    SOLID_CHECK(not _rerror);

    if (_rrecv_msg_ptr and _rrecv_msg_ptr->err == 0) {
        auto lambda = [](frame::mpipc::ConnectionContext&, ErrorConditionT const& _rerror) {
            idbg("peerb --- enter active error: " << _rerror.message());
            return frame::mpipc::MessagePointerT();
        };
        _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId(), lambda);
    } else {
        idbg("");
    }
}

void peerb_complete_message(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    if (_rrecv_msg_ptr) {
        idbg(_rctx.recipientId() << " received message with id on sender " << _rrecv_msg_ptr->senderRequestId());

        if (not _rrecv_msg_ptr->check()) {
            SOLID_THROW("Message check failed.");
        }

        if (!_rrecv_msg_ptr->isOnPeer()) {
            SOLID_THROW("Message not on peer!.");
        }

        //send message back
        if (_rctx.recipientId().isInvalidConnection()) {
            SOLID_THROW("Connection id should not be invalid!");
        }
        ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

        SOLID_CHECK(!err, "Connection id should not be invalid! " << err.message());

        ++crtreadidx;
        idbg(crtreadidx);
        if (crtwriteidx < writecount) {
            err = pmpipcpeera->sendMessage(
                "localhost/b", std::make_shared<Message>(crtwriteidx++),
                initarray[crtwriteidx % initarraysize].flags | frame::mpipc::MessageFlagsE::WaitResponse);

            SOLID_CHECK(!err, "Connection id should not be invalid! " << err.message());
        }
    }
    if (_rsent_msg_ptr) {
        idbg(_rctx.recipientId() << " done sent message " << _rsent_msg_ptr.get());
    }
}
//-----------------------------------------------------------------------------
} //namespace

int test_relay_disabled(int argc, char** argv)
{
#ifdef SOLID_HAS_DEBUG
    Debug::the().levelMask("ew");
    Debug::the().moduleMask("frame_mpipc:view any:view");
    Debug::the().initStdErr(false, nullptr);
//Debug::the().initFile("test_clientserver_basic", false);
#endif

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
        if (*argv[2] == 's' or *argv[2] == 'S') {
            secure = true;
        }
        if (*argv[2] == 'c' or *argv[2] == 'C') {
            compress = true;
        }
        if (*argv[2] == 'b' or *argv[2] == 'B') {
            secure   = true;
            compress = true;
        }
    }

    for (int j = 0; j < 1; ++j) {
        for (int i = 0; i < 127; ++i) {
            int c = (i + j) % 127;
            if (isprint(c) and !isblank(c)) {
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
        AioSchedulerT          sch_peera;
        AioSchedulerT          sch_peerb;
        AioSchedulerT          sch_relay;
        frame::Manager         m;
        frame::mpipc::ServiceT mpipcrelay(m);
        frame::mpipc::ServiceT mpipcpeera(m);
        frame::mpipc::ServiceT mpipcpeerb(m);
        frame::aio::Resolver   resolver;
        ErrorConditionT        err;

        err = sch_peera.start(1);

        if (err) {
            edbg("starting aio peera scheduler: " << err.message());
            return 1;
        }

        err = sch_peerb.start(1);

        if (err) {
            edbg("starting aio peerb scheduler: " << err.message());
            return 1;
        }

        err = sch_relay.start(1);

        if (err) {
            edbg("starting aio relay scheduler: " << err.message());
            return 1;
        }

        err = resolver.start(1);

        if (err) {
            edbg("starting aio resolver: " << err.message());
            return 1;
        }

        std::string relay_port;

        { //mpipc relay initialization
            auto con_start    = [](frame::mpipc::ConnectionContext& _rctx) {};
            auto con_stop     = [](frame::mpipc::ConnectionContext& _rctx) {};
            auto con_register = [](
                                    frame::mpipc::ConnectionContext& _rctx,
                                    std::shared_ptr<Register>&       _rsent_msg_ptr,
                                    std::shared_ptr<Register>&       _rrecv_msg_ptr,
                                    ErrorConditionT const&           _rerror) {
            };

            auto                        proto = frame::mpipc::serialization_v1::Protocol::create();
            frame::mpipc::Configuration cfg(sch_relay, proto);

            proto->registerType<Register>(con_register, 0, 10);

            cfg.server.listener_address_str      = "0.0.0.0:0";
            cfg.pool_max_active_connection_count = 2 * max_per_pool_connection_count;
            cfg.connection_stop_fnc              = con_start;
            cfg.client.connection_start_fnc      = con_stop;
            cfg.client.connection_start_state    = frame::mpipc::ConnectionState::Active;
            cfg.relay_enabled                    = true; //enable relay but do not provide a propper relay engine

            if (secure) {
                idbg("Configure SSL server -------------------------------------");
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
                edbg("starting server mpipcservice: " << err.message());
                //exiting
                return 1;
            }

            {
                std::ostringstream oss;
                oss << mpipcrelay.configuration().server.listenerPort();
                relay_port = oss.str();
                idbg("relay listens on port: " << relay_port);
            }
        }

        pmpipcpeera = &mpipcpeera;
        pmpipcpeerb = &mpipcpeerb;

        { //mpipc peera initialization
            auto                        proto = frame::mpipc::serialization_v1::Protocol::create();
            frame::mpipc::Configuration cfg(sch_peera, proto);

            proto->registerType<Message>(peera_complete_message, 0 /*protocol id*/, 20 /*message id*/);

            cfg.connection_stop_fnc           = peera_connection_stop;
            cfg.client.connection_start_fnc   = peera_connection_start;
            cfg.client.connection_start_state = frame::mpipc::ConnectionState::Active;

            cfg.pool_max_active_connection_count = max_per_pool_connection_count;

            cfg.client.name_resolve_fnc = frame::mpipc::InternetResolverF(resolver, relay_port.c_str() /*, SocketInfo::Inet4*/);

            if (secure) {
                idbg("Configure SSL client ------------------------------------");
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
                edbg("starting peera mpipcservice: " << err.message());
                //exiting
                return 1;
            }
        }

        { //mpipc peerb initialization
            auto                        proto = frame::mpipc::serialization_v1::Protocol::create();
            frame::mpipc::Configuration cfg(sch_peerb, proto);

            proto->registerType<Register>(peerb_complete_register, 0, 10);
            proto->registerType<Message>(peerb_complete_message, 0, 20);

            cfg.connection_stop_fnc         = peerb_connection_stop;
            cfg.client.connection_start_fnc = peerb_connection_start;

            cfg.pool_max_active_connection_count = max_per_pool_connection_count;

            cfg.client.name_resolve_fnc = frame::mpipc::InternetResolverF(resolver, relay_port.c_str() /*, SocketInfo::Inet4*/);

            if (secure) {
                idbg("Configure SSL client ------------------------------------");
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
                edbg("starting peerb mpipcservice: " << err.message());
                //exiting
                return 1;
            }
        }

        //const size_t start_count = 1;

        writecount = initarraysize; //start_count;//

        //ensure we have provisioned connections on peerb
        //err = mpipcpeerb.createConnectionPool("localhost");
        //SOLID_CHECK(not err, "failed create connection from peerb: "<<err.message());

        if (1) {
            for (; crtwriteidx < writecount;) {
                mpipcpeera.sendMessage(
                    "localhost/b", std::make_shared<Message>(crtwriteidx++),
                    initarray[crtwriteidx % initarraysize].flags | frame::mpipc::MessageFlagsE::WaitResponse);
            }
        }

        idbg("send message count: " << writecount);

        unique_lock<mutex> lock(mtx);

        if (not cnd.wait_for(lock, std::chrono::seconds(220), []() { return not running; })) {
            SOLID_THROW("Process is taking too long.");
        }

        if (crtwriteidx != crtackidx) {
            SOLID_THROW("Not all messages were completed");
        }

        //m.stop();
    }

    //exiting

    std::cout << "Transfered size = " << (transfered_size * 2) / 1024 << "KB" << endl;
    std::cout << "Transfered count = " << transfered_count << endl;
    std::cout << "Connection count = " << connection_count << endl;

    return 0;
}
