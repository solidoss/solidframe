#include "solid/frame/mprpc/mprpcsocketstub_openssl.hpp"

#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioactor.hpp"
#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioresolver.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v3.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include "solid/system/exception.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/utility/threadpool.hpp"

#include "solid/system/log.hpp"

#include <iostream>

using namespace std;
using namespace solid;

namespace {

using AioSchedulerT  = frame::Scheduler<frame::aio::Reactor<frame::mprpc::EventT>>;
using SecureContextT = frame::aio::openssl::Context;
using CallPoolT      = ThreadPool<Function<void()>, Function<void()>>;

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
// std::atomic<size_t> crtbackidx(0);
std::atomic<size_t> crtackidx(0);
std::atomic<size_t> writecount(0);

size_t connection_count(0);

bool                      running = true;
std::mutex                mtx;
std::condition_variable   cnd;
frame::mprpc::Service*    pmprpcclient = nullptr;
std::atomic<uint64_t>     transfered_size(0);
std::atomic<size_t>       transfered_count(0);
frame::mprpc::RecipientId recipinet_id;

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
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << this << " idx = " << idx);
        init();
    }
    Message()
        : serialized(false)
    {
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << this);
    }
    ~Message()
    {
        solid_dbg(generic_logger, Info, "DELETE ---------------- " << this);
        //      if(!serialized && !this->isBackOnSender()){
        //          solid_throw("Message not serialized.");
        //      }
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        using ReflectorT = decay_t<decltype(_rr)>;

        _rr.add(_rthis.idx, _rctx, 0, "idx").add(_rthis.str, _rctx, 1, "str");

        if constexpr (ReflectorT::is_const_reflector) {
            _rthis.serialized = true;
        } else {
            if (_rthis.isOnPeer()) {
                ++crtreadidx;
                solid_dbg(generic_logger, Info, crtreadidx);
                if (crtreadidx == 1) {

                    pmprpcclient->forceCloseConnectionPool(
                        recipinet_id,
                        [](frame::mprpc::ConnectionContext& _rctx) {
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

using MessagePointerT = solid::frame::mprpc::MessagePointerT<Message>;

void client_connection_stop(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
    if (_rctx.isConnectionActive()) {
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

void client_complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    MessagePointerT& _rsent_msg_ptr, MessagePointerT& _rrecv_msg_ptr,
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
    frame::mprpc::ConnectionContext& _rctx,
    MessagePointerT& _rsent_msg_ptr, MessagePointerT& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    solid_check(false);
}

} // namespace

int test_pool_force_close(int argc, char* argv[])
{
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
        frame::mprpc::ServiceT mprpcserver(m);
        frame::mprpc::ServiceT mprpcclient(m);
        ErrorConditionT        err;
        CallPoolT              cwp{{1, 100, 0}, [](const size_t) {}, [](const size_t) {}};
        frame::aio::Resolver   resolver([&cwp](std::function<void()>&& _fnc) { cwp.pushOne(std::move(_fnc)); });

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

            cfg.connection_stop_fnc           = &server_connection_stop;
            cfg.server.connection_start_fnc   = &server_connection_start;
            cfg.server.connection_start_state = frame::mprpc::ConnectionState::Active;

            cfg.server.listener_address_str = "0.0.0.0:0";

            cfg.writer.max_message_count_multiplex = 4;

            {
                frame::mprpc::ServiceStartStatus start_status;
                mprpcserver.start(start_status, std::move(cfg));

                std::ostringstream oss;
                oss << start_status.listen_addr_vec_.back().port();
                server_port = oss.str();
                solid_dbg(generic_logger, Info, "server listens on: " << start_status.listen_addr_vec_.back());
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

            cfg.connection_stop_fnc           = &client_connection_stop;
            cfg.client.connection_start_fnc   = &client_connection_start;
            cfg.client.connection_start_state = frame::mprpc::ConnectionState::Active;

            cfg.pool_max_active_connection_count = max_per_pool_connection_count;

            cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(resolver, server_port.c_str() /*, SocketInfo::Inet4*/);

            cfg.writer.max_message_count_multiplex = 2;

            mprpcclient.start(std::move(cfg));
        }

        pmprpcclient = &mprpcclient;

        const size_t start_count = initarraysize;

        writecount = start_count; //
        {
            std::vector<MessagePointerT> msg_vec;
            ErrorConditionT              err;

            for (size_t i = 0; i < start_count; ++i) {
                msg_vec.push_back(MessagePointerT(frame::mprpc::make_message<Message>(i)));
            }
            {
                std::vector<MessagePointerT>::iterator it = msg_vec.begin();

                {
                    ++crtwriteidx;
                    mprpcclient.sendMessage(
                        {"localhost"}, *it, recipinet_id, 0);
                }

                ++it;

                for (; crtwriteidx < start_count; ++it) {
                    ++crtwriteidx;
                    err = mprpcclient.sendMessage(
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

        // m.stop();
    }

    std::cout << "Transfered size = " << (transfered_size * 2) / 1024 << "KB" << endl;
    std::cout << "Transfered count = " << transfered_count << endl;
    std::cout << "Connection count = " << connection_count << endl;

    return 0;
}
