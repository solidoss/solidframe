#include "solid/frame/mprpc/mprpcsocketstub_openssl.hpp"

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

#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/system/exception.hpp"

#include "solid/system/log.hpp"

#include <iostream>
#include <signal.h>

using namespace std;
using namespace solid;

using AioSchedulerT  = frame::Scheduler<frame::aio::Reactor>;
using SecureContextT = frame::aio::openssl::Context;
using ProtocolT      = frame::mprpc::serialization_v2::Protocol<uint8_t>;

namespace {

struct InitStub {
    size_t size;
    ulong  flags;
};

InitStub initarray[] = {
    {18192000, 0},
    {16384000, 0},
    {263840000, 0},
    {16384000, 0},
    {8192000, 0}};

std::string  pattern;
const size_t initarraysize = sizeof(initarray) / sizeof(InitStub);

std::atomic<size_t> crtwriteidx(0);
//std::atomic<size_t> crtreadidx(0);
std::atomic<size_t> crtbackidx(0);
std::atomic<size_t> crtackidx(0);
std::atomic<size_t> writecount(0);

size_t connection_count(0);

bool                   running     = true;
bool                   start_sleep = false;
mutex                  mtx;
condition_variable     cnd;
frame::mprpc::Service* pmprpcclient = nullptr;
std::atomic<uint64_t>  transfered_size(0);
std::atomic<size_t>    transfered_count(0);

size_t real_size(size_t _sz)
{
    //offset + (align - (offset mod align)) mod align
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
        solid_dbg(generic_logger, Info, "DELETE ---------------- " << (void*)this << " idx = " << idx << " str.size = " << str.size());
        //      if(!serialized && !this->isBackOnSender() && idx != 0){
        //          solid_throw("Message not serialized.");
        //      }
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, /*_name*/)
    {
        _s.add(_rthis.idx, _rctx, "idx").add(_rthis.str, _rctx, "str");
        if (_s.is_serializer) {
            _rthis.serialized = true;
        }

        if (_rthis.isOnPeer()) {
            lock_guard<mutex> lock(mtx);
            if (!start_sleep) {
                start_sleep = true;
                cnd.notify_one();
                std::this_thread::yield();
                return;
            }
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
    auto lambda = [](frame::mprpc::ConnectionContext&, ErrorConditionT const& _rerror) {
        solid_dbg(generic_logger, Info, "enter active error: " << _rerror.message());
    };
    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId(), lambda);
}

void server_connection_stop(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
}

void server_connection_start(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());
    auto lambda = [](frame::mprpc::ConnectionContext&, ErrorConditionT const& _rerror) {
        solid_dbg(generic_logger, Info, "enter active error: " << _rerror.message());
    };
    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId(), lambda);
}

void client_complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());

    ++crtackidx;

    if (_rsent_msg_ptr) {
        solid_dbg(generic_logger, Info, "idx = " << _rsent_msg_ptr->idx);
        if (!_rerror) {
        } else {
            solid_dbg(generic_logger, Warning, "send message complete: <" << _rerror.message() << "> <" << _rctx.error().message() << "> <" << _rctx.systemError().message() << ">");
            solid_check(_rsent_msg_ptr->idx == 0 || _rsent_msg_ptr->idx == 2);
            solid_assert(
                _rerror == frame::mprpc::error_message_connection && ((_rctx.error() == frame::aio::error_stream_shutdown && !_rctx.systemError()) || (_rctx.error() && _rctx.systemError())));
        }
    }
    if (_rrecv_msg_ptr) {
        solid_dbg(generic_logger, Info, "idx = " << _rrecv_msg_ptr->idx);
        if (!_rrecv_msg_ptr->check()) {
            solid_throw("Message check failed.");
        }

        solid_check(!_rerror);

        //cout<< _rmsgptr->str.size()<<'\n';

        if (!_rrecv_msg_ptr->isBackOnSender()) {
            solid_throw("Message not back on sender!.");
        }

        solid_check(_rrecv_msg_ptr->idx == 1 || _rrecv_msg_ptr->idx == 3 || _rrecv_msg_ptr->idx == 4);

        transfered_size += _rrecv_msg_ptr->str.size();
        ++transfered_count;
        ++crtbackidx;

        if (crtbackidx == writecount) {
            lock_guard<mutex> lock(mtx);
            running = false;
            cnd.notify_one();
        }
    }
}

void server_complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& /*_rerror*/)
{
    if (_rrecv_msg_ptr) {
        solid_dbg(generic_logger, Info, _rctx.recipientId() << " received message with id on sender " << _rrecv_msg_ptr->senderRequestId() << " idx = " << _rrecv_msg_ptr->idx);

        //solid_check(_rrecv_msg_ptr->idx != 0);

        if (!_rrecv_msg_ptr->check()) {
            solid_throw("Message check failed.");
        }

        if (!_rrecv_msg_ptr->isOnPeer()) {
            solid_throw("Message not on peer!.");
        }

        if (_rrecv_msg_ptr->idx == 0) {
            return; //ignore the first message
        }

        //send message back
        if (_rctx.recipientId().isInvalidConnection()) {
            solid_throw("Connection id should not be invalid!");
        }

        ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), _rrecv_msg_ptr);

        solid_check(!(err && err != frame::mprpc::error_service_stopping), "sendResponse should not fail: " << err.message());
    }
    if (_rsent_msg_ptr) {
        solid_dbg(generic_logger, Info, _rctx.recipientId() << " done sent message " << _rsent_msg_ptr.get());
    }
}

} //namespace

int test_clientserver_idempotent(int argc, char* argv[])
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

    bool secure = false;

    if (argc > 2) {
        if (*argv[2] == 's' || *argv[2] == 'S') {
            secure = true;
        }
    }

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

        frame::Manager         m;
        frame::mprpc::ServiceT mprpcserver(m);
        frame::mprpc::ServiceT mprpcclient(m);
        ErrorConditionT        err;
        FunctionWorkPool       fwp{WorkPoolConfiguration()};
        frame::aio::Resolver   resolver(fwp);

        sch_client.start(1);
        sch_server.start(1);

        std::string server_port("39999");

        { //mprpc server initialization
            auto                        proto = ProtocolT::create();
            frame::mprpc::Configuration cfg(sch_server, proto);

            proto->null(0);
            proto->registerMessage<Message>(server_complete_message, 1);

            //cfg.recv_buffer_capacity = 1024;
            //cfg.send_buffer_capacity = 1024;

            cfg.connection_stop_fnc         = &server_connection_stop;
            cfg.server.connection_start_fnc = &server_connection_start;

            cfg.server.listener_address_str = "0.0.0.0:";
            cfg.server.listener_address_str += server_port;

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

            if (err) {
                solid_dbg(generic_logger, Error, "starting server mprpcservice: " << err.message());
                //exiting
                return 1;
            }

            {
                std::ostringstream oss;
                oss << mprpcserver.configuration().server.listenerPort();
                server_port = oss.str();
                solid_dbg(generic_logger, Info, "server listens on port: " << server_port);
            }
        }

        { //mprpc client initialization
            auto                        proto = ProtocolT::create();
            frame::mprpc::Configuration cfg(sch_client, proto);

            proto->null(0);
            proto->registerMessage<Message>(client_complete_message, 1);

            //cfg.recv_buffer_capacity = 1024;
            //cfg.send_buffer_capacity = 1024;

            cfg.connection_stop_fnc         = &client_connection_stop;
            cfg.client.connection_start_fnc = &client_connection_start;

            cfg.pool_max_active_connection_count = max_per_pool_connection_count;

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

        std::vector<std::shared_ptr<Message>> msg_vec{
            std::make_shared<Message>(0),
            std::make_shared<Message>(1),
            std::make_shared<Message>(2),
            std::make_shared<Message>(3),
            std::make_shared<Message>(4)};
        {
            ++crtwriteidx;
            mprpcclient.sendMessage(
                "localhost", msg_vec[0],
                {frame::mprpc::MessageFlagsE::AwaitResponse, frame::mprpc::MessageFlagsE::OneShotSend});
        }

        {
            ++crtwriteidx;
            mprpcclient.sendMessage(
                "localhost", msg_vec[1],
                {frame::mprpc::MessageFlagsE::AwaitResponse, frame::mprpc::MessageFlagsE::Idempotent});
        }

        {
            ++crtwriteidx;
            mprpcclient.sendMessage(
                "localhost", msg_vec[2],
                {frame::mprpc::MessageFlagsE::OneShotSend});
        }

        {
            ++crtwriteidx;
            mprpcclient.sendMessage(
                "localhost", msg_vec[3],
                {frame::mprpc::MessageFlagsE::AwaitResponse, frame::mprpc::MessageFlagsE::Idempotent, frame::mprpc::MessageFlagsE::Synchronous});
        }

        {
            ++crtwriteidx;
            mprpcclient.sendMessage(
                "localhost", msg_vec[4],
                {frame::mprpc::MessageFlagsE::AwaitResponse, frame::mprpc::MessageFlagsE::Synchronous});
        }

        writecount = 3;

        {
            unique_lock<mutex> lock(mtx);

            if (!cnd.wait_for(lock, std::chrono::seconds(100), []() { return start_sleep; })) {
                solid_throw("Start is taking too long.");
            }
        }

        solid_dbg(generic_logger, Info, "---- Before server stopping ----");
        mprpcserver.stop();
        solid_dbg(generic_logger, Info, "---- After server stopped ----");

        std::this_thread::sleep_for(std::chrono::milliseconds(5 * 1000));

        solid_dbg(generic_logger, Info, "---- Before server start ----");
        mprpcserver.start();
        solid_dbg(generic_logger, Info, "---- After server started ----");

        unique_lock<mutex> lock(mtx);

        if (!cnd.wait_for(lock, std::chrono::seconds(120), []() { return !running; })) {
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
