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
#include "solid/frame/mpipc/mpipcerror.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v1.hpp"
#include "solid/frame/mpipc/mpipcservice.hpp"

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
    size_t size;
    ulong  flags;
};

InitStub initarray[] = {
    {100000, 0},
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
frame::mpipc::Service* pmpipcclient = nullptr;
std::atomic<uint64_t>  transfered_size(0);
std::atomic<size_t>    transfered_count(0);

int test_scenario = 0;

/*
 * test_scenario == 0: test for error_connection_too_many_keepalive_packets_received
 * test_scenario == 1: test for error_connection_inactivity_timeout
*/

size_t real_size(size_t _sz)
{
    //offset + (align - (offset mod align)) mod align
    return _sz + ((sizeof(uint64_t) - (_sz % sizeof(uint64_t))) % sizeof(uint64_t));
}

struct Message : frame::mpipc::Message {
    uint32_t    idx;
    std::string str;

    Message(uint32_t _idx)
        : idx(_idx)
    {
        idbg("CREATE ---------------- " << (void*)this << " idx = " << idx);
        init();
    }
    Message()
    {
        idbg("CREATE ---------------- " << (void*)this);
    }
    ~Message()
    {
        idbg("DELETE ---------------- " << (void*)this);
    }

    template <class S>
    void solidSerialize(S& _s, frame::mpipc::ConnectionContext& _rctx)
    {
        _s.push(str, "str");
        _s.push(idx, "idx");
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
            pu[i] = pup[i % pattern_size]; //pattern[i % pattern.size()];
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
            if (pu[i] != pup[i % pattern_size])
                return false;
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

    if (not _rmsgptr->check()) {
        SOLID_THROW("Message check failed.");
    }

    //cout<< _rmsgptr->str.size()<<'\n';
    transfered_size += _rmsgptr->str.size();
    ++transfered_count;

    if (!_rmsgptr->isBackOnSender()) {
        SOLID_THROW("Message not back on sender!.");
    }

    ++crtbackidx;

    if (crtbackidx == writecount) {
        idbg("done running " << crtackidx << " " << writecount);
        unique_lock<mutex> lock(mtx);
        running = false;
        cnd.notify_one();
    }
}

void client_complete_message(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    idbg(_rctx.recipientId());
    if (_rsent_msg_ptr.get()) {
        if (!_rerror) {
            ++crtackidx;
        }
    }

    if (_rrecv_msg_ptr.get()) {
        client_receive_message(_rctx, _rrecv_msg_ptr);
    }
}

void server_receive_message(frame::mpipc::ConnectionContext& _rctx, std::shared_ptr<Message>& _rmsgptr)
{
    idbg(_rctx.recipientId() << " message id on sender " << _rmsgptr->requestId());
    if (not _rmsgptr->check()) {
        SOLID_THROW("Message check failed.");
    }

    if (!_rmsgptr->isOnPeer()) {
        SOLID_THROW("Message not on peer!.");
    }

    //send message back
    _rctx.service().sendResponse(_rctx.recipientId(), _rmsgptr);
    /*
    ++crtreadidx;
    idbg(crtreadidx);
    if(crtwriteidx < writecount){
        frame::mpipc::MessagePointerT   msgptr(new Message(crtwriteidx));
        ++crtwriteidx;
        pmpipcclient->sendMessage(
            "localhost:6666", msgptr,
            initarray[crtwriteidx % initarraysize].flags | frame::mpipc::MessageFlags::WaitResponse
        );
    }*/
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

int test_keepalive_success(int argc, char** argv)
{
#ifdef SOLID_HAS_DEBUG
    Debug::the().levelMask("ew");
    Debug::the().moduleMask("all");
    Debug::the().initStdErr(false, nullptr);
#endif

    size_t max_per_pool_connection_count = 1;

    if (argc > 1) {
        test_scenario = atoi(argv[1]);
        if (test_scenario != 0) {
            SOLID_THROW("Invalid test scenario.");
        }
    }

    for (int i = 0; i < 127; ++i) {
        if (isprint(i) and !isblank(i)) {
            pattern += static_cast<char>(i);
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
            auto                        proto = frame::mpipc::serialization_v1::Protocol::create();
            frame::mpipc::Configuration cfg(sch_server, proto);

            proto->registerType<Message>(
                server_complete_message);

            //cfg.recv_buffer_capacity = 1024;
            //cfg.send_buffer_capacity = 1024;

            cfg.connection_stop_fnc         = server_connection_stop;
            cfg.server.connection_start_fnc = server_connection_start;

            cfg.server.listener_address_str   = "0.0.0.0:0";
            cfg.server.connection_start_state = frame::mpipc::ConnectionState::Active;

            if (test_scenario == 0) {
                cfg.connection_inactivity_timeout_seconds = 20;
                cfg.connection_inactivity_keepalive_count = 4;
            }

            cfg.writer.max_message_count_multiplex = 6;

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
            auto                        proto = frame::mpipc::serialization_v1::Protocol::create();
            frame::mpipc::Configuration cfg(sch_client, proto);

            proto->registerType<Message>(
                client_complete_message);

            //cfg.recv_buffer_capacity = 1024;
            //cfg.send_buffer_capacity = 1024;

            if (test_scenario == 0) {
                cfg.connection_keepalive_timeout_seconds = 10;
            }

            cfg.client.connection_start_state = frame::mpipc::ConnectionState::Active;
            cfg.connection_stop_fnc           = client_connection_stop;
            cfg.client.connection_start_fnc   = client_connection_start;

            cfg.pool_max_active_connection_count = max_per_pool_connection_count;

            cfg.writer.max_message_count_multiplex = 6;

            cfg.client.name_resolve_fnc = frame::mpipc::InternetResolverF(resolver, server_port.c_str() /*, SocketInfo::Inet4*/);

            err = mpipcclient.reconfigure(std::move(cfg));

            if (err) {
                edbg("starting client mpipcservice: " << err.message());
                //exiting
                return 1;
            }
        }

        pmpipcclient = &mpipcclient;

        writecount = 2;

        {
            frame::mpipc::MessagePointerT msgptr(new Message(crtwriteidx));
            ++crtwriteidx;
            mpipcclient.sendMessage(
                "localhost", msgptr,
                initarray[crtwriteidx % initarraysize].flags | frame::mpipc::MessageFlags::WaitResponse);
        }
        idbg("before sleep");

        sleep(60);

        idbg("after sleep");
        {
            frame::mpipc::MessagePointerT msgptr(new Message(crtwriteidx));
            ++crtwriteidx;
            mpipcclient.sendMessage(
                "localhost", msgptr,
                initarray[crtwriteidx % initarraysize].flags | frame::mpipc::MessageFlags::WaitResponse);
        }

        unique_lock<mutex> lock(mtx);

        if (not cnd.wait_for(lock, std::chrono::seconds(120), []() { return not running; })) {
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
