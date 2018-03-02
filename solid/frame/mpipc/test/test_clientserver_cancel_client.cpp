//
// 1. Send one message from client to server
// 2. On server receive message, send messages as in test_protocol_cancel
// 3. On client receive first message cancel all messages from the server side

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

#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v2.hpp"
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

using AioSchedulerT = frame::Scheduler<frame::aio::Reactor>;
using SecureContextT = frame::aio::openssl::Context;
using ProtocolT = frame::mpipc::serialization_v2::Protocol<uint8_t>;

namespace {

struct InitStub {
    size_t                      size;
    bool                        cancel;
    frame::mpipc::MessageFlagsT flags;
};

//NOTE: if making more messages non-cancelable, please consider to change the value of expected_transfered_count

InitStub initarray[] = {
    {100000, false, 0}, //first message must not be canceled
    {16384000, false, 0}, //not caceled
    {8192000, true, {frame::mpipc::MessageFlagsE::Synchronous}},
    {4096000, true, 0},
    {2048000, false, 0}, //not caceled
    {1024000, true, 0},
    {512000, false, {frame::mpipc::MessageFlagsE::Synchronous}}, //not canceled
    {256000, true, 0},
    {1280000, true, 0},
    {6400000, true, 0},
    {32000, false, 0}, //not canceled
    {1600000, true, 0},
    {8000000, true, 0},
    {4000000, true, 0},
    {200000, true, 0},
};

const size_t expected_transfered_count = 5;

typedef std::vector<frame::mpipc::MessageId> MessageIdVectorT;

std::string  pattern;
const size_t initarraysize = sizeof(initarray) / sizeof(InitStub);

std::atomic<size_t> crtwriteidx(0);
std::atomic<size_t> crtreadidx(0);
//std::atomic<size_t> crtbackidx(0);
//std::atomic<size_t> crtackidx(0);
std::atomic<size_t> writecount(0);

size_t connection_count(0);

bool                      running = true;
mutex                     mtx;
condition_variable        cnd;
frame::mpipc::Service*    pmpipcclient = nullptr;
frame::mpipc::Service*    pmpipcserver = nullptr;
std::atomic<uint64_t>     transfered_size(0);
std::atomic<size_t>       transfered_count(0);
frame::mpipc::RecipientId recipient_id;

MessageIdVectorT message_uid_vec;

size_t real_size(size_t _sz)
{
    //offset + (align - (offset mod align)) mod align
    return _sz + ((sizeof(uint64_t) - (_sz % sizeof(uint64_t))) % sizeof(uint64_t));
}

struct Message : frame::mpipc::Message {
    uint32_t    idx;
    std::string str;
    mutable bool        serialized;

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
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name){
        _s.add(_rthis.idx, _rctx, "idx").add(_rthis.str, _rctx, "str");
        if(_s.is_serializer){
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

void client_connection_stop(frame::mpipc::ConnectionContext& _rctx)
{
    idbg(_rctx.recipientId() << " error: " << _rctx.error().message());
    if (!running) {
        ++connection_count;
    }
}

void client_connection_start(frame::mpipc::ConnectionContext& _rctx)
{
    idbg(_rctx.recipientId());
}

void server_connection_stop(frame::mpipc::ConnectionContext& _rctx)
{
    idbg(_rctx.recipientId() << " error: " << _rctx.error().message());
}

void server_connection_start(frame::mpipc::ConnectionContext& _rctx)
{
    idbg(_rctx.recipientId());
}

void client_receive_message(frame::mpipc::ConnectionContext& _rctx, std::shared_ptr<Message>& _rmsgptr)
{
    idbg(_rctx.recipientId());

    SOLID_CHECK(_rmsgptr->check(), "Message check failed.");
    SOLID_THROW("should not be called");
}

void client_complete_message(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    idbg(_rctx.recipientId() << " error = " << _rerror.message());
    if (_rrecv_msg_ptr.get()) {
        client_receive_message(_rctx, _rrecv_msg_ptr);
    }
}

void server_receive_message(frame::mpipc::ConnectionContext& _rctx, std::shared_ptr<Message>& _rmsgptr)
{

    idbg(_rctx.recipientId() << " message id on sender " << _rmsgptr->senderRequestId());

    if (not _rmsgptr->check()) {
        SOLID_THROW("Message check failed.");
    }

    if (!_rmsgptr->isOnPeer()) {
        SOLID_THROW("Message not on peer!.");
    }

    if (_rctx.recipientId().isInvalidConnection()) {
        SOLID_THROW("Connection id should not be invalid!");
    }

    size_t idx = static_cast<Message&>(*_rmsgptr).idx;
    SOLID_CHECK(not initarray[idx % initarraysize].cancel);

    transfered_size += _rmsgptr->str.size();
    ++transfered_count;

    if (!crtreadidx) {
        idbg("canceling all messages");
        unique_lock<mutex> lock(mtx);
        for (auto& msguid : message_uid_vec) {
            idbg("Cancel message: " << msguid);
            pmpipcclient->cancelMessage(recipient_id, msguid);
        }
    }

    ++crtreadidx;

    if (crtreadidx == expected_transfered_count) {
        unique_lock<mutex> lock(mtx);
        running = false;
        cnd.notify_one();
    }
}

void server_complete_message(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    idbg(_rctx.recipientId());
    if (_rrecv_msg_ptr.get()) {
        server_receive_message(_rctx, _rrecv_msg_ptr);
    }
}

} //namespace

int test_clientserver_cancel_client(int argc, char** argv)
{
#ifdef SOLID_HAS_DEBUG
    Debug::the().levelMask("ew");
    Debug::the().moduleMask("frame_mpipc:ew frame_aio:ew any:ew");
    Debug::the().initStdErr(false, nullptr);
#endif

    size_t max_per_pool_connection_count = 1;

    bool secure = false;

    if (argc > 1) {
        if (*argv[1] == 's' or *argv[1] == 'S') {
            secure = true;
        }
    }

    //  if(argc > 1){
    //      max_per_pool_connection_count = atoi(argv[1]);
    //  }

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
        AioSchedulerT sch_client;
        AioSchedulerT sch_server;

        frame::Manager         m;
        frame::mpipc::ServiceT mpipcserver(m);
        frame::mpipc::ServiceT mpipcclient(m);
        ErrorConditionT        err;

        frame::aio::Resolver resolver;

        err = sch_client.start(1);

        if (err) {
            edbg("starting aio client scheduler: " << err.message());
            return 1;
        }

        err = sch_server.start(1);

        if (err) {
            edbg("starting aio server scheduler: " << err.message());
            return 1;
        }

        err = resolver.start(1);

        if (err) {
            edbg("starting aio resolver: " << err.message());
            return 1;
        }

        std::string server_port;

        { //mpipc server initialization
            auto                        proto = ProtocolT::create();
            frame::mpipc::Configuration cfg(sch_server, proto);

            proto->null(0);
            proto->registerMessage<Message>(server_complete_message, 1);

            //cfg.recv_buffer_capacity = 1024;
            //cfg.send_buffer_capacity = 1024;

            cfg.connection_stop_fnc         = &server_connection_stop;
            cfg.server.connection_start_fnc = &server_connection_start;

            cfg.server.listener_address_str   = "0.0.0.0:0";
            cfg.server.connection_start_state = frame::mpipc::ConnectionState::Active;

            cfg.writer.max_message_count_multiplex = 6;

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

            err = mpipcserver.reconfigure(std::move(cfg));

            if (err) {
                edbg("starting server mpipcservice: " << err.message());
                //exiting
                return 1;
            }

            {
                std::ostringstream oss;
                oss << mpipcserver.configuration().server.listenerPort();
                server_port = oss.str();
                idbg("server listens on port: " << server_port);
            }
        }

        { //mpipc client initialization
            auto                        proto = ProtocolT::create();
            frame::mpipc::Configuration cfg(sch_client, proto);

            proto->null(0);
            proto->registerMessage<Message>(client_complete_message, 1);

            //cfg.recv_buffer_capacity = 1024;
            //cfg.send_buffer_capacity = 1024;

            cfg.client.connection_start_state = frame::mpipc::ConnectionState::Active;
            cfg.connection_stop_fnc           = &client_connection_stop;
            cfg.client.connection_start_fnc   = &client_connection_start;

            cfg.pool_max_active_connection_count = max_per_pool_connection_count;

            cfg.writer.max_message_count_multiplex = 6;

            cfg.client.name_resolve_fnc = frame::mpipc::InternetResolverF(resolver, server_port.c_str() /*, SocketInfo::Inet4*/);

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

            err = mpipcclient.reconfigure(std::move(cfg));

            if (err) {
                edbg("starting client mpipcservice: " << err.message());
                //exiting
                return 1;
            }
        }

        pmpipcclient = &mpipcclient;
        pmpipcserver = &mpipcserver;

        {
            writecount = initarraysize; //start_count;//
            unique_lock<mutex> lock(mtx);
            for (crtwriteidx = 0; crtwriteidx < writecount; ++crtwriteidx) {
                frame::mpipc::MessageId msguid;

                ErrorConditionT err = mpipcclient.sendMessage(
                    "localhost", frame::mpipc::MessagePointerT(new Message(crtwriteidx)),
                    recipient_id,
                    msguid);

                SOLID_CHECK(!err, "Connection id should not be invalid! " << err.message());

                if (recipient_id.isInvalidPool()) {
                    SOLID_THROW("Connection pool should not be invalid!");
                }

                if (initarray[crtwriteidx % initarraysize].cancel) {
                    message_uid_vec.push_back(msguid); //we cancel this one
                    idbg("schedule cancel message: " << message_uid_vec.back());
                }
            }
        }

        unique_lock<mutex> lock(mtx);

        if (not cnd.wait_for(lock, std::chrono::seconds(120), []() { return not running; })) {
            SOLID_THROW("Process is taking too long.");
        }

        //if(crtbackidx != expected_transfered_count){
        //  SOLID_THROW("Not all messages were completed");
        //}

        //m.stop();
    }

    //exiting

    std::cout << "Transfered size = " << (transfered_size * 2) / 1024 << "KB" << endl;
    std::cout << "Transfered count = " << transfered_count << endl;
    std::cout << "Connection count = " << connection_count << endl;

    return 0;
}
