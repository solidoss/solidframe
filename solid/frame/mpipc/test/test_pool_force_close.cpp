#include "solid/frame/mpipc/mpipcsocketstub_openssl.hpp"

#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioresolver.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v2.hpp"
#include "solid/frame/mpipc/mpipcservice.hpp"

#include "solid/system/exception.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/system/log.hpp"

#include <iostream>

using namespace std;
using namespace solid;

using AioSchedulerT  = frame::Scheduler<frame::aio::Reactor>;
using SecureContextT = frame::aio::openssl::Context;
using ProtocolT      = frame::mpipc::serialization_v2::Protocol<uint8_t>;

namespace {

struct InitStub {
    size_t size;
    ulong  flags;
};

InitStub initarray[] = {
    {8192000, 0},
    {8024000, 0},
    {8048000, 0},
    {8096000, 0},
    {8192000, 0},
    {16384000, 0},
    {8192000, 0} /*,
    {8024000, 0},
    {8048000, 0},
    {8096000, 0},
    {8192000, 0},
    {16384000, 0}*/
};

std::string  pattern;
const size_t initarraysize = sizeof(initarray) / sizeof(InitStub);

std::atomic<size_t> crtwriteidx(0);
std::atomic<size_t> crtreadidx(0);
//std::atomic<size_t> crtbackidx(0);
std::atomic<size_t> crtackidx(0);
std::atomic<size_t> writecount(0);

size_t connection_count(0);

bool                      running = true;
std::mutex                mtx;
std::condition_variable   cnd;
frame::mpipc::Service*    pmpipcclient = nullptr;
std::atomic<uint64_t>     transfered_size(0);
std::atomic<size_t>       transfered_count(0);
frame::mpipc::RecipientId recipinet_id;

size_t real_size(size_t _sz)
{
    //offset + (align - (offset mod align)) mod align
    return _sz + ((sizeof(uint64_t) - (_sz % sizeof(uint64_t))) % sizeof(uint64_t));
}

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
    }
    Message()
        : serialized(false)
    {
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << (void*)this);
    }
    ~Message()
    {
        solid_dbg(generic_logger, Info, "DELETE ---------------- " << (void*)this);
        //      if(!serialized && !this->isBackOnSender()){
        //          solid_throw("Message not serialized.");
        //      }
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.str, _rctx, "str").add(_rthis.idx, _rctx, "idx");
        if (_s.is_serializer) {
            _rthis.serialized = true;
        } else {
            if (_rthis.isOnPeer()) {
                ++crtreadidx;
                solid_dbg(generic_logger, Info, crtreadidx);
                if (crtreadidx == 1) {

                    pmpipcclient->forceCloseConnectionPool(
                        recipinet_id,
                        [](frame::mpipc::ConnectionContext& _rctx) {
                            solid_dbg(generic_logger, Info, "------------------");
                            lock_guard<mutex> lock(mtx);
                            running = false;
                            cnd.notify_one();
                        });
                }
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

void client_connection_stop(frame::mpipc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
    if (_rctx.isConnectionActive()) {
        ++connection_count;
    }
}

void client_connection_start(frame::mpipc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());
}

void server_connection_stop(frame::mpipc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
}

void server_connection_start(frame::mpipc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());
}

void client_complete_message(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rerror.message());

    if (_rsent_msg_ptr.get()) {
        solid_check(_rerror);
        ++crtackidx;
    }
    solid_check(!_rrecv_msg_ptr);
}

void server_complete_message(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    solid_check(false);
}

} //namespace

int test_pool_force_close(int argc, char* argv[])
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
        AioSchedulerT sch_client;
        AioSchedulerT sch_server;

        frame::Manager         m;
        frame::mpipc::ServiceT mpipcserver(m);
        frame::mpipc::ServiceT mpipcclient(m);
        ErrorConditionT        err;
        FunctionWorkPool       fwp{WorkPoolConfiguration()};
        frame::aio::Resolver   resolver(fwp);

        err = sch_client.start(1);

        if (err) {
            solid_dbg(generic_logger, Error, "starting aio client scheduler: " << err.message());
            return 1;
        }

        err = sch_server.start(1);

        if (err) {
            solid_dbg(generic_logger, Error, "starting aio server scheduler: " << err.message());
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

            cfg.connection_stop_fnc           = &server_connection_stop;
            cfg.server.connection_start_fnc   = &server_connection_start;
            cfg.server.connection_start_state = frame::mpipc::ConnectionState::Active;

            cfg.server.listener_address_str = "0.0.0.0:0";

            cfg.writer.max_message_count_multiplex = 4;

            err = mpipcserver.reconfigure(std::move(cfg));

            if (err) {
                solid_dbg(generic_logger, Error, "starting server mpipcservice: " << err.message());
                return 1;
            }

            {
                std::ostringstream oss;
                oss << mpipcserver.configuration().server.listenerPort();
                server_port = oss.str();
                solid_dbg(generic_logger, Info, "server listens on port: " << server_port);
            }
        }

        { //mpipc client initialization
            auto                        proto = ProtocolT::create();
            frame::mpipc::Configuration cfg(sch_client, proto);

            proto->null(0);
            proto->registerMessage<Message>(client_complete_message, 1);

            //cfg.recv_buffer_capacity = 1024;
            //cfg.send_buffer_capacity = 1024;

            cfg.connection_stop_fnc           = &client_connection_stop;
            cfg.client.connection_start_fnc   = &client_connection_start;
            cfg.client.connection_start_state = frame::mpipc::ConnectionState::Active;

            cfg.pool_max_active_connection_count = max_per_pool_connection_count;

            cfg.client.name_resolve_fnc = frame::mpipc::InternetResolverF(resolver, server_port.c_str() /*, SocketInfo::Inet4*/);

            cfg.writer.max_message_count_multiplex = 2;

            err = mpipcclient.reconfigure(std::move(cfg));

            if (err) {
                solid_dbg(generic_logger, Error, "starting client mpipcservice: " << err.message());
                return 1;
            }
        }

        pmpipcclient = &mpipcclient;

        const size_t start_count = initarraysize;

        writecount = start_count; //
        {
            std::vector<frame::mpipc::MessagePointerT> msg_vec;
            ErrorConditionT                            err;

            for (size_t i = 0; i < start_count; ++i) {
                msg_vec.push_back(frame::mpipc::MessagePointerT(new Message(i)));
            }
            {
                std::vector<frame::mpipc::MessagePointerT>::iterator it = msg_vec.begin();

                {
                    ++crtwriteidx;
                    mpipcclient.sendMessage(
                        "localhost", *it, recipinet_id, 0);
                }

                ++it;

                for (; crtwriteidx < start_count; ++it) {
                    ++crtwriteidx;
                    err = mpipcclient.sendMessage(
                        recipinet_id, *it, 0);
                    if (err) {
                        solid_dbg(generic_logger, Error, "Message not sent: " << err.message());
                        ++crtackidx;
                    }
                }
            }
        }

        unique_lock<mutex> lock(mtx);

        if (!cnd.wait_for(lock, std::chrono::seconds(120), []() { return !running; })) {
            solid_throw("Process is taking too long.");
        }

        if (crtwriteidx != crtackidx) {
            solid_throw("Not all messages were completed");
        }

        //m.stop();
    }

    std::cout << "Transfered size = " << (transfered_size * 2) / 1024 << "KB" << endl;
    std::cout << "Transfered count = " << transfered_count << endl;
    std::cout << "Connection count = " << connection_count << endl;

    return 0;
}
