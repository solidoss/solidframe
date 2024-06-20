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

#include "solid/utility/threadpool.hpp"

#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

using namespace std;
using namespace solid;

namespace {
using AioSchedulerT  = frame::Scheduler<frame::aio::Reactor<frame::mprpc::EventT>>;
using SecureContextT = frame::aio::openssl::Context;
using CallPoolT      = ThreadPool<Function<void()>, Function<void()>>;

struct InitStub {
    size_t                      size;
    frame::mprpc::MessageFlagsT flags;
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
frame::mprpc::Service* pmprpcclient = nullptr;
std::atomic<uint64_t>  transfered_size(0);
std::atomic<size_t>    transfered_count(0);
std::string            raw_data;

size_t real_size(size_t _sz)
{
    // offset + (align - (offset mod align)) mod align
    return _sz + ((sizeof(uint64_t) - (_sz % sizeof(uint64_t))) % sizeof(uint64_t));
}

void init_string(std::string& _rstr, const size_t _idx)
{
    const size_t sz = real_size(initarray[_idx % initarraysize].size);
    _rstr.resize(sz);
    const size_t    count        = sz / sizeof(uint64_t);
    uint64_t*       pu           = reinterpret_cast<uint64_t*>(const_cast<char*>(_rstr.data()));
    const uint64_t* pup          = reinterpret_cast<const uint64_t*>(pattern.data());
    const size_t    pattern_size = pattern.size() / sizeof(uint64_t);
    for (uint64_t i = 0; i < count; ++i) {
        pu[i] = pup[(_idx + i) % pattern_size]; // pattern[i % pattern.size()];
    }
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
    ~Message()
    {
        solid_dbg(generic_logger, Info, "DELETE ---------------- " << (void*)this);
        solid_assert(serialized || this->isBackOnSender());
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        using ReflectorT = decay_t<decltype(_rr)>;

        _rr.add(_rthis.idx, _rctx, 0, "idx").add(_rthis.str, _rctx, 1, "str");

        if constexpr (ReflectorT::is_const_reflector) {
            _rthis.serialized = true;
        }
    }

    void init()
    {
        init_string(str, idx);
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
    if (!running) {
        ++connection_count;
    }
}

template <class F>
struct RecvClosure {
    std::string data;
    F           finalize_fnc;

    RecvClosure(F&& _finalize_fnc)
        : finalize_fnc(std::move(_finalize_fnc))
    {
    }

    void operator()(frame::mprpc::ConnectionContext& _rctx, const char* _pdata, size_t& _rsz, ErrorConditionT const& _rerror)
    {
        solid_dbg(generic_logger, Info, "received raw data: sz = " << _rsz << " error = " << _rerror.message());
        solid_check(!_rerror);
        solid_check(_rsz != 0);

        size_t towrite = raw_data.size() - data.size();

        if (towrite > _rsz) {
            towrite = _rsz;
        }

        data.append(_pdata, towrite);

        _rsz = towrite;

        if (raw_data.size() == data.size()) {
            finalize_fnc(_rctx, _rerror, std::move(data));
        } else {
            // continue reading
            _rctx.service().connectionNotifyRecvSomeRawData(_rctx.recipientId(), std::move(*this));
        }
    }
};

void client_connection_start(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());

    auto lambda = [](frame::mprpc::ConnectionContext& _rctx, ErrorConditionT const& _rerror) {
        solid_dbg(generic_logger, Info, "sent raw data: " << _rerror.message());
        solid_check(!_rerror);
        // sent the raw_data, prepare receive the message back:

        auto lambda = [](frame::mprpc::ConnectionContext& _rctx, ErrorConditionT const& _rerror, std::string&& _rdata) {
            solid_dbg(generic_logger, Info, "received back raw data: " << _rerror.message() << " data.size = " << _rdata.size());
            solid_check(!_rerror);
            solid_check(_rdata == raw_data);
            // activate concetion
            _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId());
        };

        _rctx.service().connectionNotifyRecvSomeRawData(_rctx.recipientId(), RecvClosure<decltype(lambda)>(std::move(lambda)));
    };
    _rctx.service().connectionNotifySendAllRawData(_rctx.recipientId(), lambda, std::string(raw_data));
}

void server_connection_stop(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
}

void server_connection_start(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());

    auto lambda = [](frame::mprpc::ConnectionContext& _rctx, ErrorConditionT const& _rerror, std::string&& _rdata) {
        auto lambda = [](frame::mprpc::ConnectionContext& _rctx, ErrorConditionT const& _rerror) {
            solid_dbg(generic_logger, Info, "sent raw data: " << _rerror.message());
            solid_check(!_rerror);
            // now that we've sent the raw string back, activate the connection
            _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId());
        };

        solid_dbg(generic_logger, Info, "received raw data: " << _rerror.message() << " data_size: " << _rdata.size());

        _rctx.service().connectionNotifySendAllRawData(_rctx.recipientId(), lambda, std::move(_rdata));
    };

    _rctx.service().connectionNotifyRecvSomeRawData(_rctx.recipientId(), RecvClosure<decltype(lambda)>(std::move(lambda)));
}

void client_complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    MessagePointerT& _rsent_msg_ptr, MessagePointerT& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());

    if (_rsent_msg_ptr) {
        if (!_rerror) {
            ++crtackidx;
        }
    }
    if (_rrecv_msg_ptr) {
        if (!_rrecv_msg_ptr->check()) {
            solid_throw("Message check failed.");
        }

        // cout<< _rmsgptr->str.size()<<'\n';
        transfered_size += _rrecv_msg_ptr->str.size();
        ++transfered_count;

        if (!_rrecv_msg_ptr->isBackOnSender()) {
            solid_throw("Message not back on sender!.");
        }

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
    MessagePointerT& _rsent_msg_ptr, MessagePointerT& _rrecv_msg_ptr,
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

        // send message back
        if (_rctx.recipientId().isInvalidConnection()) {
            solid_throw("Connection id should not be invalid!");
        }
        ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

        solid_check(!err, "Connection id should not be invalid! " << err.message());

        ++crtreadidx;
        solid_dbg(generic_logger, Info, crtreadidx);
    }
    if (_rsent_msg_ptr) {
        solid_dbg(generic_logger, Info, _rctx.recipientId() << " done sent message " << _rsent_msg_ptr.get());
    }
}

} // namespace

int test_raw_basic(int argc, char* argv[])
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

    init_string(raw_data, 0);

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

            cfg.connection_recv_buffer_max_capacity_kb = 1;
            cfg.connection_send_buffer_max_capacity_kb = 1;

            cfg.server.connection_start_state = frame::mprpc::ConnectionState::Raw;
            cfg.connection_stop_fnc           = &server_connection_stop;
            cfg.server.connection_start_fnc   = &server_connection_start;

            cfg.server.listener_address_str = "0.0.0.0:0";

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

            cfg.connection_recv_buffer_max_capacity_kb = 1;
            cfg.connection_send_buffer_max_capacity_kb = 1;

            cfg.client.connection_start_state = frame::mprpc::ConnectionState::Raw;

            cfg.connection_stop_fnc         = &client_connection_stop;
            cfg.client.connection_start_fnc = &client_connection_start;

            cfg.pool_max_active_connection_count = max_per_pool_connection_count;

            cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(resolver, server_port.c_str() /*, SocketInfo::Inet4*/);

            mprpcclient.start(std::move(cfg));
        }

        pmprpcclient = &mprpcclient;

        const size_t start_count = 10;

        writecount = 10; // initarraysize * 10;//start_count;//

        for (; crtwriteidx < start_count;) {
            MessagePointerT msgptr(frame::mprpc::make_message<Message>(crtwriteidx));
            ++crtwriteidx;
            mprpcclient.sendMessage(
                {"localhost"}, msgptr,
                initarray[crtwriteidx % initarraysize].flags | frame::mprpc::MessageFlagsE::AwaitResponse);
        }

        unique_lock<mutex> lock(mtx);

        if (!cnd.wait_for(lock, std::chrono::seconds(120), []() { return !running; })) {
            solid_throw("Process is taking too long.");
        }

        // m.stop();
        if (crtwriteidx != crtackidx) {
            solid_assert(false);
            solid_throw("Not all messages were completed");
        }
    }

    // exiting

    std::cout << "Transfered size = " << (transfered_size * 2) / 1024 << "KB" << endl;
    std::cout << "Transfered count = " << transfered_count << endl;
    std::cout << "Connection count = " << connection_count << endl;

    return 0;
}
