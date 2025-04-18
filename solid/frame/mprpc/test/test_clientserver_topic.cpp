#include <cmath>
#include <fstream>
#include <future>
#include <string>
#include <vector>
// #ifdef __cpp_lib_ranges
#include <ranges>
// #endif

#include "solid/frame/mprpc/mprpcsocketstub_openssl.hpp"

#include "solid/frame/mprpc/mprpccompression_snappy.hpp"
#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v3.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioactor.hpp"
#include "solid/frame/aio/aioresolver.hpp"

#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/utility/stack.hpp"
#include "solid/utility/threadpool.hpp"

using namespace std;
using namespace solid;

using AioSchedulerT  = frame::Scheduler<frame::aio::Reactor<frame::mprpc::EventT>>;
using SecureContextT = frame::aio::openssl::Context;

// #define TRACE_DURATION

namespace {

LoggerT logger("test");

constexpr size_t thread_count = 4;

#ifdef SOLID_ON_LINUX
vector<int> isolcpus = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15, 17, 18, 19};

void set_current_thread_affinity()
{
    if (std::thread::hardware_concurrency() < (thread_count + isolcpus[0])) {
        return;
    }
    static std::atomic<int> crtCore(0);

    const int isolCore = isolcpus[crtCore.fetch_add(1) % isolcpus.size()];
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(isolCore, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    // solid_check(rc == 0);
    (void)rc;
}
#else
void set_current_thread_affinity()
{
}
#endif

using ResolvePoolT = ThreadPool<Function<void(), 80>, Function<void(), 80>>;

using CallPoolT     = ThreadPool<Function<void(), 128>, Function<void(), 80> /* , EmptyThreadPoolStatistic */>;
using SynchContextT = decltype(declval<CallPoolT>().createSynchronizationContext());

inline auto milliseconds_since_epoch(const std::chrono::system_clock::time_point& _time = std::chrono::system_clock::now())
{
    using namespace std::chrono;
    const uint64_t ms = duration_cast<milliseconds>(_time.time_since_epoch()).count();
    return ms;
}

uint64_t microseconds_since_epoch()
{
    using namespace std::chrono;
    static const auto start   = high_resolution_clock::now();
    const auto        elapsed = std::chrono::high_resolution_clock::now() - start;

    return duration_cast<microseconds>(elapsed).count();
}

struct Topic {
    const size_t  id_;
    SynchContextT synch_ctx_;
    uint64_t      value_ = 0;

    Topic(
        const size_t    _id,
        SynchContextT&& _context)
        : id_(_id)
        , synch_ctx_(std::move(_context))
    {
    }
};

struct WorkerContext {
    vector<shared_ptr<Topic>> topic_vec_;

    WorkerContext(
        vector<shared_ptr<Topic>>&& _topic_vec)
        : topic_vec_(_topic_vec)
    {
    }
};

struct WorkerActor : frame::aio::Actor {
private:
    void onEvent(frame::aio::ReactorContext& _rctx, EventBase&& _revent) override;
};

std::atomic_size_t req_count = {0};
std::atomic_size_t res_count = {0};

struct Request : frame::mprpc::Message {
    uint32_t         topic_id_                 = InvalidIndex{};
    uint32_t         iteration_                = 0;
    uint64_t         time_point_               = -1;
    mutable uint64_t serialization_time_point_ = 0;

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        using ReflectorT = decay_t<decltype(_rr)>;

        if constexpr (ReflectorT::is_const_reflector) {
            _rthis.serialization_time_point_ = microseconds_since_epoch();
        }

        _rr.add(_rthis.topic_id_, _rctx, 1, "topic_id");
        _rr.add(_rthis.iteration_, _rctx, 2, "iteration");
        _rr.add(_rthis.time_point_, _rctx, 3, "time_point");
        _rr.add(_rthis.serialization_time_point_, _rctx, 4, "serialization_time_point");
    }

    Request()
    {
        ++req_count;
    }

    Request(
        const uint32_t _topic_id,
        const uint64_t _time_point)
        : topic_id_(_topic_id)
        , time_point_(_time_point)
    {
        ++req_count;
    }
};

struct Response : frame::mprpc::Message {
    uint64_t         time_point_               = -1;
    mutable uint64_t serialization_time_point_ = 0;
    uint64_t         topic_value_              = 0;
    uint64_t         topic_context_            = 0;
    uint64_t         receive_time_point_       = 0;

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        using ReflectorT = decay_t<decltype(_rr)>;

        if constexpr (ReflectorT::is_const_reflector) {
            _rthis.serialization_time_point_ = microseconds_since_epoch();
        }

        _rr.add(_rthis.time_point_, _rctx, 3, "time_point");
        _rr.add(_rthis.serialization_time_point_, _rctx, 4, "serialization_time_point");
        _rr.add(_rthis.topic_value_, _rctx, 6, "topic_value");
        _rr.add(_rthis.topic_context_, _rctx, 7, "topic_context");
        _rr.add(_rthis.receive_time_point_, _rctx, 5, "receive_time_point");
    }

    Response()
    {
        ++res_count;
    }
};

using CacheableRequestT         = EnableCacheable<Request>;
using CacheableResponseT        = EnableCacheable<Response>;
using RequestPointerT           = solid::frame::mprpc::MessagePointerT<Request>;
using CesponsePointerT          = solid::frame::mprpc::MessagePointerT<Response>;
using CacheableRequestPointerT  = solid::frame::mprpc::MessagePointerT<CacheableRequestT>;
using CacheableResponsePointerT = solid::frame::mprpc::MessagePointerT<CacheableResponseT>;

template <typename T>
auto lin_value(T _from, T _to, const size_t _index, const size_t _n)
{
    assert(_n > 1 && _index < _n);
    const auto amount = static_cast<T>(_index) / (_n - 1);
    const auto v      = std::lerp(_from, _to, amount);
    return v;
}

#ifdef __cpp_lib_ranges
template <typename T>
auto lin_space(T _from, T _to, const size_t _n)
{
    return std::views::iota(size_t{0}, _n) | views::transform([=](auto i) { return lin_value(_from, _to, i, _n); });
}
#endif

void client_connection_stop(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
}

void client_connection_start(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(logger, Info, _rctx.recipientId());
    auto lambda = [](frame::mprpc::ConnectionContext&, ErrorConditionT const& _rerror) {
        solid_dbg(logger, Info, "enter active error: " << _rerror.message());
    };
    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId(), lambda);
}

void server_connection_stop(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
}

void server_connection_start(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(logger, Info, _rctx.recipientId());
    auto lambda = [](frame::mprpc::ConnectionContext&, ErrorConditionT const& _rerror) {
        solid_dbg(logger, Info, "enter active error: " << _rerror.message());
    };
    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId(), lambda);
}

void client_complete_request(
    frame::mprpc::ConnectionContext& _rctx,
    RequestPointerT& _rsent_msg_ptr, CacheableResponsePointerT& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror);

void client_complete_response(
    frame::mprpc::ConnectionContext& _rctx,
    CacheableResponsePointerT& _rsent_msg_ptr, CacheableResponsePointerT& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror);

void server_complete_response(
    frame::mprpc::ConnectionContext& _rctx,
    CacheableResponsePointerT& _rsent_msg_ptr, CacheableResponsePointerT& _rrecv_msg_ptr,
    ErrorConditionT const& /*_rerror*/);

void server_complete_request(
    frame::mprpc::ConnectionContext& _rctx,
    CacheableRequestPointerT& _rsent_msg_ptr, CacheableRequestPointerT& _rrecv_msg_ptr,
    ErrorConditionT const& /*_rerror*/);

void multicast_run(CallPoolT& _work_pool);
void wait();

struct ThreadPoolLocalContext {
    uint64_t value_ = 0;
};

//-----------------------------------------------------------------------------
// Time points:
// - request send initiate time -> Message::time_offset (snd)
// - request serialization time -> Message::serialization_time_point_ (snd)
// - request deserialization time -> Message::receive_time_point_ (rcv)
// - request process time -> Message::time_point_ (rcv)
// - response serialization time -> Message::serialization_time_point_ (rcv)
// - response receive time -> now

using DurationTupleT = tuple<uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t>;
using DurationDqT    = std::deque<DurationTupleT>;
using TraceRecordT   = tuple<uint64_t, uint64_t>;
using TraceDqT       = std::deque<TraceRecordT>;

vector<frame::ActorIdT>                         worker_actor_id_vec;
CallPoolT                                       worker_pool;
size_t                                          per_message_loop_count = 10;
std::atomic<size_t>                             active_message_count;
std::atomic<uint64_t>                           max_time_delta{0};
std::atomic<uint64_t>                           min_time_delta{std::numeric_limits<uint64_t>::max()};
std::atomic<uint64_t>                           max_time_server_trip_delta{0};
std::atomic<uint64_t>                           min_time_server_trip_delta{std::numeric_limits<uint64_t>::max()};
std::atomic<uint64_t>                           max_time_server_threadpool_latency{0};
std::atomic<uint64_t>                           min_time_server_threadpool_latency{std::numeric_limits<uint64_t>::max()};
std::atomic<uint64_t>                           request_count{0};
frame::mprpc::RecipientId                       client_id;
TraceDqT                                        trace_dq;
std::mutex                                      trace_mtx;
size_t                                          max_per_pool_connection_count = 10;
std::atomic<size_t>                             client_connection_count{0};
std::promise<void>                              client_connection_promise;
std::promise<void>                              promise;
std::atomic_bool                                running = true;
thread_local unique_ptr<ThreadPoolLocalContext> local_thread_pool_context_ptr;
chrono::microseconds                            test_duration{chrono::seconds(1)};
// bool                                            cache_local_messages = false;

#ifdef TRACE_DURATION
DurationDqT duration_dq;
#endif

auto create_message_ptr = [](auto& _rctx, auto& _rmsgptr) {
    using PtrT  = std::decay_t<decltype(_rmsgptr)>;
    using ElemT = typename PtrT::element_type;

    _rmsgptr = ElemT::create();
};

//-----------------------------------------------------------------------------
} // namespace

int test_clientserver_topic(int argc, char* argv[])
{

    solid::log_start(std::cerr, {".*:EWX", "test:EWXS"});
    // solid::log_start(std::cerr, {"solid::frame::mprpc.*:EWX", "\\*:VIEWX"});

    size_t topic_count   = 1000;
    bool   secure        = false;
    bool   compress      = false;
    size_t message_count = 100000;

    {
        AioSchedulerT          sch_client;
        AioSchedulerT          sch_server;
        ResolvePoolT           resolve_pool;
        frame::Manager         m;
        frame::mprpc::ServiceT mprpcserver(m);
        frame::mprpc::ServiceT mprpcclient(m);
        frame::ServiceT        generic_service{m};
        ErrorConditionT        err;
        frame::aio::Resolver   resolver([&resolve_pool](std::function<void()>&& _fnc) { resolve_pool.pushOne(std::move(_fnc)); });

        worker_pool.start(
            {thread_count, 10000, 100},
            [](const size_t) {
                set_current_thread_affinity();
                local_thread_pool_context_ptr = std::make_unique<ThreadPoolLocalContext>();
            },
            [](const size_t) {});

        resolve_pool.start(
            {1, 100, 0}, [](const size_t) {}, [](const size_t) {});
        sch_client.start([]() {set_current_thread_affinity();return true; }, []() {}, 1);
        sch_server.start([]() {
            set_current_thread_affinity();
            for(size_t  i = 0; i < 2000; ++i){
                auto  reply_ptr = frame::mprpc::make_message<CacheableResponseT>();
                reply_ptr->cache();
            }
            return true; }, []() {}, 2);

        {
            // create the topics
            vector<shared_ptr<Topic>> topic_vec;
            for (size_t i = 0; i < topic_count; ++i) {
                topic_vec.emplace_back(make_shared<Topic>(i, worker_pool.createSynchronizationContext()));
            }
            ErrorConditionT err;
            const auto      worker_count = sch_server.workerCount();
            for (size_t i = 0; i < worker_count; ++i) {
                worker_actor_id_vec.emplace_back(
                    frame::make_actor<WorkerActor>(sch_server, generic_service, i, make_event(GenericEventE::Start, topic_vec), err));
            }

            // TODO: at this point we're not sure that the topic_vec has reached all the threads
            // we need a shared promise or something

            solid_dbg(logger, Info, "Created topics for " << worker_count << " workers");
        }

        std::string server_port;

        { // mprpc server initialization
            auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
                reflection::v1::metadata::factory,
                [&](auto& _rmap) {
                    _rmap.template registerMessage<CacheableRequestT>(1, "Request", server_complete_request, create_message_ptr);
                    _rmap.template registerMessage<CacheableResponseT>(2, "Response", server_complete_response, create_message_ptr);
                });
            frame::mprpc::Configuration cfg(sch_server, proto);

            cfg.connection_stop_fnc                      = &server_connection_stop;
            cfg.server.connection_start_fnc              = &server_connection_start;
            cfg.connection_send_buffer_start_capacity_kb = 8;
            cfg.connection_recv_buffer_start_capacity_kb = 8;

            cfg.server.listener_address_str = "0.0.0.0:0";

            if (secure) {
                solid_dbg(logger, Info, "Configure SSL server -------------------------------------");
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

            {
                frame::mprpc::ServiceStartStatus start_status;
                mprpcserver.start(start_status, std::move(cfg));
                solid_check(!start_status.listen_addr_vec_.empty());
                std::ostringstream oss;
                oss << start_status.listen_addr_vec_.back().port();
                server_port = oss.str();
                solid_dbg(logger, Info, "server listens on: " << start_status.listen_addr_vec_.back() << " port: " << server_port);
            }
        }

        { // mprpc client initialization
            auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
                reflection::v1::metadata::factory,
                [&](auto& _rmap) {
                    _rmap.template registerMessage<Request>(1, "Request", client_complete_request);
                    _rmap.template registerMessage<CacheableResponseT>(2, "Response", client_complete_response, create_message_ptr);
                });
            frame::mprpc::Configuration cfg(sch_client, proto);

            cfg.connection_stop_fnc                      = &client_connection_stop;
            cfg.client.connection_start_fnc              = &client_connection_start;
            cfg.connection_send_buffer_start_capacity_kb = 8;
            cfg.connection_recv_buffer_start_capacity_kb = 8;
            cfg.pool_max_active_connection_count         = max_per_pool_connection_count;
            cfg.pool_max_message_queue_size              = message_count;

            cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF{resolver, server_port};

            if (secure) {
                solid_dbg(logger, Info, "Configure SSL client ------------------------------------");
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

            mprpcclient.start(std::move(cfg));
        }

        err = mprpcclient.createConnectionPool(
            {"127.0.0.1"}, client_id,
            [](frame::mprpc::ConnectionContext& _rctx, EventBase&& _revent, const ErrorConditionT& _rerr) {
                solid_dbg(logger, Info, "client pool event: " << _revent << " error: " << _rerr.message());
                if (_revent == frame::mprpc::pool_event_connection_activate) {
                    const auto count = client_connection_count.fetch_add(1);
                    if ((count + 1) == max_per_pool_connection_count) {
                        client_connection_promise.set_value();
                    }
                }
            },
            max_per_pool_connection_count);

        { // wait for all connections to be created:
            auto fut = client_connection_promise.get_future();
            solid_check(fut.wait_for(chrono::seconds(150)) == future_status::ready, "Taking too long to create connections - waited 150 secs");
            fut.get();
        }

        thread multicaster{
            []() {
                multicast_run(worker_pool);
            }};

        active_message_count      = message_count;
        const uint64_t start_msec = milliseconds_since_epoch();

        solid_log(logger, Warning, "========== START sending messages ==========");

        if (false) {
            const uint64_t startms = microseconds_since_epoch();
            for (size_t i = 0; i < message_count; ++i) {
                mprpcclient.sendMessage(client_id, frame::mprpc::make_message<Request>(i, microseconds_since_epoch()), {frame::mprpc::MessageFlagsE::AwaitResponse});
            }
            solid_log(logger, Warning, "========== DONE sending messages ========== " << (microseconds_since_epoch() - startms) << "us");
        } else {
            const double   stop    = test_duration.count();
            const uint64_t startms = microseconds_since_epoch();

            const auto start = chrono::high_resolution_clock::now();

            for (size_t i = 0; i < message_count; ++i) {
                this_thread::sleep_until(start + chrono::microseconds(static_cast<int64_t>(lin_value(0.0, stop, i, message_count))));
                mprpcclient.sendMessage(client_id, frame::mprpc::make_message<Request>(i, microseconds_since_epoch()), {frame::mprpc::MessageFlagsE::AwaitResponse});
            }
            solid_log(logger, Warning, "========== DONE sending messages ========== " << (microseconds_since_epoch() - startms) << "us");
        }

        solid_statistic_add(request_count, message_count);

        wait();

        cout << "max delta              : " << max_time_delta.load() << endl;
        cout << "min delta              : " << min_time_delta.load() << endl;
        cout << "max server delta       : " << max_time_server_trip_delta.load() << endl;
        cout << "min server delta       : " << min_time_server_trip_delta.load() << endl;
        cout << "max threadpool latency : " << max_time_server_threadpool_latency.load() << endl;
        cout << "min threadpool latency : " << min_time_server_threadpool_latency.load() << endl;
        cout << "request count    : " << request_count.load() << endl;

        multicaster.join();
        mprpcserver.stop();
        generic_service.stop();
        worker_pool.stop();

        solid_log(logger, Statistic, "Test duration: " << (milliseconds_since_epoch() - start_msec) << "msecs");
        solid_log(logger, Statistic, "ThreadPool statistic: " << worker_pool.statistic());
        solid_log(logger, Statistic, "mprpcserver statistic: " << mprpcserver.statistic());
        solid_log(logger, Statistic, "mprpcclient statistic: " << mprpcclient.statistic());
        solid_log(logger, Statistic, "req_count: " << req_count.load() << " res_count: " << res_count.load());

#ifdef TRACE_DURATION
        if (1) {
            ofstream ofs("duration.csv");
            if (ofs) {
                for (const auto& t : duration_dq) {
                    // ofs << get<0>(t) << ", " << get<1>(t) << ", " << get<2>(t) << ", " << get<3>(t) << ", " << get<4>(t) << ", " << get<5>(t) << ", " << get<6>(t) << "," << endl;
                    ofs << get<0>(t) << ", " << get<2>(t) - get<1>(t) << ", " << get<3>(t) - get<1>(t) << ", " << get<4>(t) - get<3>(t) << ", " << get<5>(t) - get<1>(t) << ", " << get<6>(t) - get<1>(t) << "," << endl;
                }
                ofs.close();
            }
        }
#endif
        if (0) {
            ofstream ofs("trace.csv");
            if (ofs) {
                for (const auto& t : trace_dq) {
                    ofs << get<0>(t) << ", " << get<1>(t) << "," << endl;
                }
                ofs.close();
            }
        }
    }
    return 0;
}

namespace {
//-----------------------------------------------------------------------------
thread_local unique_ptr<WorkerContext> local_worker_context_ptr;
//-----------------------------------------------------------------------------
void wait()
{
    auto fut = promise.get_future();
    solid_check(fut.wait_for(chrono::seconds(150)) == future_status::ready, "Taking too long - waited 150 secs");
    fut.get();
}
void WorkerActor::onEvent(frame::aio::ReactorContext& _rctx, EventBase&& _revent)
{
    if (_revent == generic_event<GenericEventE::Start>) {
        solid_assert(!local_worker_context_ptr);
        local_worker_context_ptr = make_unique<WorkerContext>(std::move(*_revent.cast<vector<shared_ptr<Topic>>>()));
        solid_dbg(logger, Info, "Created local WorkerContex " << local_worker_context_ptr.get() << " " << local_worker_context_ptr->topic_vec_.size());
    } else if (_revent == generic_event<GenericEventE::Kill>) {
        local_worker_context_ptr.reset();
        postStop(_rctx);
    }
}

thread_local uint64_t local_send_duration = 0;

void multicast_run(CallPoolT& _work_pool)
{
#if THREAD_POOL_OPTION == 3
    uint64_t context = 0;
    while (running) {
        _work_pool.pushAll(
            [context]() {
                local_thread_pool_context_ptr->value_ = context;
            });
        ++context;
    }
#endif
}
void client_complete_response(
    frame::mprpc::ConnectionContext& _rctx,
    CacheableResponsePointerT& _rsent_msg_ptr, CacheableResponsePointerT& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    solid_check(false); // should not be called
}

void client_complete_request(
    frame::mprpc::ConnectionContext& _rctx,
    RequestPointerT& _rsent_msg_ptr, CacheableResponsePointerT& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    solid_dbg(logger, Info, _rctx.recipientId());

    solid_check(_rsent_msg_ptr);
    solid_check(_rrecv_msg_ptr);
    solid_check(_rrecv_msg_ptr->time_point_ >= _rsent_msg_ptr->time_point_);

    const auto now = microseconds_since_epoch();

    solid_statistic_max(max_time_delta, now - _rsent_msg_ptr->time_point_);
    solid_statistic_min(min_time_delta, now - _rsent_msg_ptr->time_point_);
    solid_statistic_max(max_time_server_trip_delta, _rrecv_msg_ptr->time_point_ - _rsent_msg_ptr->time_point_);
    solid_statistic_min(min_time_server_trip_delta, _rrecv_msg_ptr->time_point_ - _rsent_msg_ptr->time_point_);
    solid_statistic_max(max_time_server_threadpool_latency, _rrecv_msg_ptr->time_point_ - _rrecv_msg_ptr->receive_time_point_);
    solid_statistic_min(min_time_server_threadpool_latency, _rrecv_msg_ptr->time_point_ - _rrecv_msg_ptr->receive_time_point_);

#ifdef TRACE_DURATION
    duration_dq.emplace_back(
        _rctx.recipientId().connectionId().index,
        _rsent_msg_ptr->time_point_,
        _rsent_msg_ptr->serialization_time_point_ /*  - _rsent_msg_ptr->time_point_ */, // client serialization offset
        _rrecv_msg_ptr->receive_time_point_ /*  - _rsent_msg_ptr->time_point_ */, // receive offset
        _rrecv_msg_ptr->time_point_ /*  - _rsent_msg_ptr->time_point_ */, // process offset
        _rrecv_msg_ptr->serialization_time_point_ /*  - _rsent_msg_ptr->time_point_ */, // server serialization offset
        now /* - _rsent_msg_ptr->time_point_ */ // roundtrip offset
    );
#endif

    cacheable_cache(std::move(_rrecv_msg_ptr));

    _rsent_msg_ptr->time_point_ = now;
    ++_rsent_msg_ptr->iteration_;

    if (_rsent_msg_ptr->iteration_ < per_message_loop_count) {
        _rsent_msg_ptr->clearHeader();
        _rctx.service().sendMessage(client_id, std::move(_rsent_msg_ptr), {frame::mprpc::MessageFlagsE::AwaitResponse});
        local_send_duration += (microseconds_since_epoch() - now);
        solid_statistic_inc(request_count);
    } else {
        if (active_message_count.fetch_sub(1) == 1) {
            solid_dbg(logger, Warning, "Local client send duration: " << local_send_duration);
            _rctx.service().forceCloseConnectionPool(
                client_id,
                [](frame::mprpc::ConnectionContext& _rctx) {});
            running = false;
            promise.set_value();
        }
    }
}

void server_complete_response(
    frame::mprpc::ConnectionContext& _rctx,
    CacheableResponsePointerT& _rsent_msg_ptr, CacheableResponsePointerT& _rrecv_msg_ptr,
    ErrorConditionT const& /*_rerror*/)
{
    solid_check(_rsent_msg_ptr);
    solid_check(!_rrecv_msg_ptr);
    solid_dbg(logger, Info, _rctx.recipientId() << " done sent message " << _rsent_msg_ptr.get());
    cacheable_cache(std::move(_rsent_msg_ptr));
}

void server_complete_request(
    frame::mprpc::ConnectionContext& _rctx,
    CacheableRequestPointerT& _rsent_msg_ptr, CacheableRequestPointerT& _rrecv_msg_ptr,
    ErrorConditionT const& /*_rerror*/)
{
    solid_check(_rrecv_msg_ptr);
    solid_check(!_rsent_msg_ptr);
    solid_check(_rrecv_msg_ptr->isOnPeer());

    solid_dbg(logger, Info, _rctx.recipientId());
    auto& topic_ptr                = local_worker_context_ptr->topic_vec_[_rrecv_msg_ptr->topic_id_ % local_worker_context_ptr->topic_vec_.size()];
    auto  reply_ptr                = CacheableResponseT::create();
    reply_ptr->receive_time_point_ = microseconds_since_epoch();
    reply_ptr->header(_rrecv_msg_ptr->header());

    // cacheable_cache(std::move(_rrecv_msg_ptr));

    auto lambda = [topic_ptr, _rrecv_msg_ptr = std::move(_rrecv_msg_ptr),
                      &service = _rctx.service(), recipient_id = _rctx.recipientId(), reply_ptr = std::move(reply_ptr)]() {
        ++topic_ptr->value_;

        reply_ptr->topic_value_   = topic_ptr->value_;
        reply_ptr->topic_context_ = local_thread_pool_context_ptr->value_;
        reply_ptr->time_point_    = microseconds_since_epoch();

        reply_ptr->cacheableAttach(std::move(_rrecv_msg_ptr));

        service.sendResponse(recipient_id, reply_ptr);
    };

    static_assert(CallPoolT::is_small_one_type<decltype(lambda)>(), "Type not small");

    if (false) {
        std::lock_guard<mutex> lock(trace_mtx);
        if (!trace_dq.empty()) {
            if (get<0>(trace_dq.back()) == _rctx.recipientId().connectionId().index) {
                ++get<1>(trace_dq.back());
            } else {
                trace_dq.emplace_back(_rctx.recipientId().connectionId().index, 1);
            }
        } else {
            trace_dq.emplace_back(_rctx.recipientId().connectionId().index, 1);
        }
        // topic_ptr->synch_ctx_.push(std::move(lambda));
    } else {
        // worker_pool.pushOne(std::move(lambda));
        topic_ptr->synch_ctx_.push(std::move(lambda));
        //
        // lambda();
    }
    // worker_pool.push(std::move(lambda));
    //  lambda();
}

//-----------------------------------------------------------------------------
} // namespace
