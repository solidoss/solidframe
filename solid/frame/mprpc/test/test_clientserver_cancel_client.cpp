//
// 1. Send one message from client to server
// 2. On server receive message, send messages as in test_protocol_cancel
// 3. On client receive first message cancel all messages from the server side

#include "solid/frame/mprpc/mprpcsocketstub_openssl.hpp"

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

#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/utility/workpool.hpp"

#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"

#include <iostream>

using namespace std;
using namespace solid;

using AioSchedulerT  = frame::Scheduler<frame::aio::Reactor>;
using SecureContextT = frame::aio::openssl::Context;

namespace {

struct InitStub {
    size_t                      size;
    bool                        cancel;
    frame::mprpc::MessageFlagsT flags;
};

// NOTE: if making more messages non-cancelable, please consider to change the value of expected_transfered_count

InitStub initarray[] = {
    {100000, false, 0}, // first message must not be canceled
    {16384000, false, 0}, // not caceled
    {8192000, true, {frame::mprpc::MessageFlagsE::Synchronous}},
    {4096000, true, 0},
    {2048000, false, 0}, // not caceled
    {10240000, true, 0},
    {512000, false, {frame::mprpc::MessageFlagsE::Synchronous}}, // not canceled
    {2560000, true, 0},
    {1280000, true, 0},
    {6400000, true, 0},
    {32000, false, 0}, // not canceled
    {1600000, true, 0},
    {8000000, true, 0},
    {4000000, true, 0},
    {200000, true, 0},
};

const size_t expected_transfered_count = 5;

typedef std::vector<frame::mprpc::MessageId> MessageIdVectorT;

std::string  pattern;
const size_t initarraysize = sizeof(initarray) / sizeof(InitStub);

std::atomic<size_t> crtwriteidx(0);
std::atomic<size_t> crtreadidx(0);
// std::atomic<size_t> crtbackidx(0);
// std::atomic<size_t> crtackidx(0);
std::atomic<size_t> writecount(0);

size_t connection_count(0);

bool                      running = true;
mutex                     mtx;
condition_variable        cnd;
frame::mprpc::Service*    pmprpcclient = nullptr;
frame::mprpc::Service*    pmprpcserver = nullptr;
std::atomic<uint64_t>     transfered_size(0);
std::atomic<size_t>       transfered_count(0);
frame::mprpc::RecipientId recipient_id;

MessageIdVectorT message_uid_vec;

size_t real_size(size_t _sz)
{
    // offset + (align - (offset mod align)) mod align
    return _sz + ((sizeof(uint64_t) - (_sz % sizeof(uint64_t))) % sizeof(uint64_t));
}

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
    ~Message() override
    {
        solid_dbg(generic_logger, Info, "DELETE ---------------- " << (void*)this);
    }

    SOLID_REFLECT_V1(_s, _rthis, _rctx)
    {
        using ReflectorT = decay_t<decltype(_s)>;

        _s.add(_rthis.idx, _rctx, 0, "idx").add(_rthis.str, _rctx, 1, "str");

        if constexpr (ReflectorT::is_const_reflector) {
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

void client_connection_stop(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
    if (!running) {
        ++connection_count;
    }
}

void client_connection_start(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());
}

void server_connection_stop(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
}

void server_connection_start(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());
}

void client_receive_message(frame::mprpc::ConnectionContext& _rctx, std::shared_ptr<Message>& _rmsgptr)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());

    solid_check(_rmsgptr->check(), "Message check failed.");
    solid_throw("should not be called");
}

void client_complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& /*_rsent_msg_ptr*/, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error = " << _rerror.message());
    if (_rrecv_msg_ptr) {
        client_receive_message(_rctx, _rrecv_msg_ptr);
    }
}

void server_receive_message(frame::mprpc::ConnectionContext& _rctx, std::shared_ptr<Message>& _rmsgptr)
{

    solid_dbg(generic_logger, Info, _rctx.recipientId() << " message id on sender " << _rmsgptr->senderRequestId());

    if (!_rmsgptr->check()) {
        solid_throw("Message check failed.");
    }

    if (!_rmsgptr->isOnPeer()) {
        solid_throw("Message not on peer!.");
    }

    if (_rctx.recipientId().isInvalidConnection()) {
        solid_throw("Connection id should not be invalid!");
    }

    size_t idx = static_cast<Message&>(*_rmsgptr).idx;

    solid_check(!initarray[idx % initarraysize].cancel, "message " << idx << " should have been canceled");

    transfered_size += _rmsgptr->str.size();
    ++transfered_count;

    if (crtreadidx == 0) {
        solid_dbg(generic_logger, Info, "canceling all messages");
        lock_guard<mutex> lock(mtx);
        for (auto& msguid : message_uid_vec) {
            solid_dbg(generic_logger, Info, "Cancel message: " << msguid);
            pmprpcclient->cancelMessage(recipient_id, msguid);
        }
    }

    ++crtreadidx;

    if (crtreadidx == expected_transfered_count) {
        lock_guard<mutex> lock(mtx);
        running = false;
        cnd.notify_one();
    }
}

void server_complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& /*_rsent_msg_ptr*/, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& /*_rerror*/)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());
    if (_rrecv_msg_ptr) {
        server_receive_message(_rctx, _rrecv_msg_ptr);
    }
}

} // namespace

int test_clientserver_cancel_client(int argc, char* argv[])
{
    solid::log_start(std::cerr, {"solid::frame::mprpc.*:EWX", "\\*:EWX"});

    size_t max_per_pool_connection_count = 1;

    bool secure = false;

    if (argc > 1) {
        if (*argv[1] == 's' || *argv[1] == 'S') {
            secure = true;
        }
    }

    //  if(argc > 1){
    //      max_per_pool_connection_count = atoi(argv[1]);
    //  }

    for (int j = 0; j < 1; ++j) {
        for (int i = 0; i < 127; ++i) {
            int c = (i + j) % 127;
            if (isprint(c) != 0 && isblank(c) == 0) {
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
        AioSchedulerT sch_client;
        AioSchedulerT sch_server;

        frame::Manager                    m;
        frame::mprpc::ServiceT            mprpcserver(m);
        frame::mprpc::ServiceT            mprpcclient(m);
        ErrorConditionT                   err;
        lockfree::CallPoolT<void(), void> cwp{WorkPoolConfiguration(1)};
        frame::aio::Resolver              resolver([&cwp](std::function<void()>&& _fnc) { cwp.push(std::move(_fnc)); });

        sch_client.start(1);
        sch_server.start(1);

        std::string server_port;

        { // mprpc server initialization
            auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
                reflection::v1::metadata::factory,
                [&](auto& _rmap) {
                    _rmap.template registerMessage<Message>(1, "Message", server_complete_message);
                });
            frame::mprpc::Configuration cfg(sch_server, proto);

            // cfg.recv_buffer_capacity = 1024;
            // cfg.send_buffer_capacity = 1024;

            cfg.connection_stop_fnc         = &server_connection_stop;
            cfg.server.connection_start_fnc = &server_connection_start;

            cfg.server.listener_address_str   = "0.0.0.0:0";
            cfg.server.connection_start_state = frame::mprpc::ConnectionState::Active;

            cfg.writer.max_message_count_multiplex = 6;

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

            mprpcserver.start(std::move(cfg));

            {
                std::ostringstream oss;
                oss << mprpcserver.configuration().server.listenerPort();
                server_port = oss.str();
                solid_dbg(generic_logger, Info, "server listens on port: " << server_port);
            }
        }

        { // mprpc client initialization
            auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
                reflection::v1::metadata::factory,
                [&](auto& _rmap) {
                    _rmap.template registerMessage<Message>(1, "Message", client_complete_message);
                });
            frame::mprpc::Configuration cfg(sch_client, proto);

            // cfg.recv_buffer_capacity = 1024;
            // cfg.send_buffer_capacity = 1024;

            cfg.client.connection_start_state = frame::mprpc::ConnectionState::Active;
            cfg.connection_stop_fnc           = &client_connection_stop;
            cfg.client.connection_start_fnc   = &client_connection_start;

            cfg.pool_max_active_connection_count = max_per_pool_connection_count;

            cfg.writer.max_message_count_multiplex = 6;

            cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(resolver, server_port.c_str() /*, SocketInfo::Inet4*/);

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

            mprpcclient.start(std::move(cfg));
        }

        pmprpcclient = &mprpcclient;
        pmprpcserver = &mprpcserver;

        {
            writecount = initarraysize; // start_count;//
            lock_guard<mutex> lock(mtx);
            for (crtwriteidx = 0; crtwriteidx < writecount; ++crtwriteidx) {
                frame::mprpc::MessageId msguid;

                ErrorConditionT err = mprpcclient.sendMessage(
                    "localhost", frame::mprpc::MessagePointerT(std::make_shared<Message>(crtwriteidx)),
                    recipient_id,
                    msguid);

                solid_check(!err, "Connection id should not be invalid! " << err.message());

                if (recipient_id.isInvalidPool()) {
                    solid_throw("Connection pool should not be invalid!");
                }

                if (initarray[crtwriteidx % initarraysize].cancel) {
                    message_uid_vec.push_back(msguid); // we cancel this one
                    solid_dbg(generic_logger, Info, "schedule cancel message: " << message_uid_vec.back());
                }
            }
        }

        unique_lock<mutex> lock(mtx);

        if (!cnd.wait_for(lock, std::chrono::seconds(120), []() { return !running; })) {
            solid_throw("Process is taking too long.");
        }

        // if(crtbackidx != expected_transfered_count){
        //   solid_throw("Not all messages were completed");
        // }

        // m.stop();
    }

    // exiting

    std::cout << "Transfered size = " << (transfered_size * 2) / 1024 << "KB" << endl;
    std::cout << "Transfered count = " << transfered_count << endl;
    std::cout << "Connection count = " << connection_count << endl;

    return 0;
}
