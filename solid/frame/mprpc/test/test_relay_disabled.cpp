#include "solid/frame/mprpc/mprpcsocketstub_openssl.hpp"

#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioresolver.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include "solid/frame/mprpc/mprpccompression_snappy.hpp"
#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v2.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

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

struct InitStub {
    size_t                      size;
    frame::mprpc::MessageFlagsT flags;
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
frame::mprpc::Service* pmprpcpeera = nullptr;
frame::mprpc::Service* pmprpcpeerb = nullptr;
std::atomic<uint64_t>  transfered_size(0);
std::atomic<size_t>    transfered_count(0);

size_t real_size(size_t _sz)
{
    //offset + (align - (offset mod align)) mod align
    return _sz + ((sizeof(uint64_t) - (_sz % sizeof(uint64_t))) % sizeof(uint64_t));
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

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.err, _rctx, "err").add(_rthis.str, _rctx, "str");
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
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << (void*)this << " idx = " << idx);
        init();
    }
    Message()
        : serialized(false)
    {
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << (void*)this);
    }
    ~Message()
    {
        solid_dbg(generic_logger, Info, "DELETE ---------------- " << (void*)this);

        //solid_assert(serialized || this->isBackOnSender());
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.idx, _rctx, "idx").add(_rthis.str, _rctx, "str");
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
                solid_throw("Message check failed.");
                return false;
            }
        }
        return true;
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
    if (!running) {
        ++connection_count;
    }
}

void peera_complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rerror.message());
    solid_check(!_rrecv_msg_ptr, "should not receive any message");
    solid_check(_rsent_msg_ptr, "sent message should not be null");
    solid_check(_rerror == frame::mprpc::error_message_canceled_peer, "message should be canceled by peer");

    ++crtackidx;
    ++crtbackidx;

    if (crtbackidx == writecount) {
        lock_guard<mutex> lock(mtx);
        running = false;
        cnd.notify_one();
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
        solid_dbg(generic_logger, Info, _rctx.recipientId() << " received message with id on sender " << _rrecv_msg_ptr->senderRequestId());

        if (!_rrecv_msg_ptr->check()) {
            solid_throw("Message check failed.");
        }

        if (!_rrecv_msg_ptr->isOnPeer()) {
            solid_throw("Message not on peer!.");
        }

        //send message back
        if (_rctx.recipientId().isInvalidConnection()) {
            solid_throw("Connection id should not be invalid!");
        }
        ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

        solid_check(!err, "Connection id should not be invalid! " << err.message());

        ++crtreadidx;
        solid_dbg(generic_logger, Info, crtreadidx);
        if (crtwriteidx < writecount) {
            err = pmprpcpeera->sendMessage(
                "localhost/b", std::make_shared<Message>(crtwriteidx++),
                initarray[crtwriteidx % initarraysize].flags | frame::mprpc::MessageFlagsE::AwaitResponse);

            solid_check(!err, "Connection id should not be invalid! " << err.message());
        }
    }
    if (_rsent_msg_ptr) {
        solid_dbg(generic_logger, Info, _rctx.recipientId() << " done sent message " << _rsent_msg_ptr.get());
    }
}
//-----------------------------------------------------------------------------
} //namespace

int test_relay_disabled(int argc, char* argv[])
{
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
        AioSchedulerT          sch_peera;
        AioSchedulerT          sch_peerb;
        AioSchedulerT          sch_relay;
        frame::Manager         m;
        frame::mprpc::ServiceT mprpcrelay(m);
        frame::mprpc::ServiceT mprpcpeera(m);
        frame::mprpc::ServiceT mprpcpeerb(m);
        ErrorConditionT        err;
        FunctionWorkPool       fwp{WorkPoolConfiguration()};
        frame::aio::Resolver   resolver(fwp);

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

        { //mprpc relay initialization
            auto con_start    = [](frame::mprpc::ConnectionContext& _rctx) {};
            auto con_stop     = [](frame::mprpc::ConnectionContext& _rctx) {};
            auto con_register = [](
                                    frame::mprpc::ConnectionContext& _rctx,
                                    std::shared_ptr<Register>&       _rsent_msg_ptr,
                                    std::shared_ptr<Register>&       _rrecv_msg_ptr,
                                    ErrorConditionT const&           _rerror) {
            };

            auto                        proto = ProtocolT::create();
            frame::mprpc::Configuration cfg(sch_relay, proto);

            proto->null(0);
            proto->registerMessage<Register>(con_register, 1);

            cfg.server.listener_address_str      = "0.0.0.0:0";
            cfg.pool_max_active_connection_count = 2 * max_per_pool_connection_count;
            cfg.connection_stop_fnc              = std::move(con_start);
            cfg.client.connection_start_fnc      = std::move(con_stop);
            cfg.client.connection_start_state    = frame::mprpc::ConnectionState::Active;
            cfg.relay_enabled                    = true; //enable relay but do not provide a propper relay engine

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

            err = mprpcrelay.reconfigure(std::move(cfg));

            if (err) {
                solid_dbg(generic_logger, Error, "starting server mprpcservice: " << err.message());
                //exiting
                return 1;
            }

            {
                std::ostringstream oss;
                oss << mprpcrelay.configuration().server.listenerPort();
                relay_port = oss.str();
                solid_dbg(generic_logger, Info, "relay listens on port: " << relay_port);
            }
        }

        pmprpcpeera = &mprpcpeera;
        pmprpcpeerb = &mprpcpeerb;

        { //mprpc peera initialization
            auto                        proto = ProtocolT::create();
            frame::mprpc::Configuration cfg(sch_peera, proto);

            proto->null(0);
            proto->registerMessage<Message>(peera_complete_message, 2);

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

            err = mprpcpeera.reconfigure(std::move(cfg));

            if (err) {
                solid_dbg(generic_logger, Error, "starting peera mprpcservice: " << err.message());
                //exiting
                return 1;
            }
        }

        { //mprpc peerb initialization
            auto                        proto = ProtocolT::create();
            frame::mprpc::Configuration cfg(sch_peerb, proto);

            proto->null(0);
            proto->registerMessage<Register>(peerb_complete_register, 1);
            proto->registerMessage<Message>(peerb_complete_message, 2);

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

            err = mprpcpeerb.reconfigure(std::move(cfg));

            if (err) {
                solid_dbg(generic_logger, Error, "starting peerb mprpcservice: " << err.message());
                //exiting
                return 1;
            }
        }

        //const size_t start_count = 1;

        writecount = initarraysize; //start_count;//

        //ensure we have provisioned connections on peerb
        //err = mprpcpeerb.createConnectionPool("localhost");
        //solid_check(!err, "failed create connection from peerb: "<<err.message());

        if (1) {
            for (; crtwriteidx < writecount;) {
                mprpcpeera.sendMessage(
                    "localhost/b", std::make_shared<Message>(crtwriteidx++),
                    initarray[crtwriteidx % initarraysize].flags | frame::mprpc::MessageFlagsE::AwaitResponse);
            }
        }

        solid_dbg(generic_logger, Info, "send message count: " << writecount);

        unique_lock<mutex> lock(mtx);

        if (!cnd.wait_for(lock, std::chrono::seconds(220), []() { return !running; })) {
            solid_throw("Process is taking too long.");
        }

        if (crtwriteidx != crtackidx) {
            solid_throw("Not all messages were completed");
        }

        //m.stop();
    }

    //exiting

    std::cout << "Transfered size = " << (transfered_size * 2) / 1024 << "KB" << endl;
    std::cout << "Transfered count = " << transfered_count << endl;
    std::cout << "Connection count = " << connection_count << endl;

    return 0;
}
