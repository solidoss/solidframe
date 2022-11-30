#include <future>
#include <string>
#include <vector>

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
#include "solid/utility/workpool.hpp"

using namespace std;
using namespace solid;

using AioSchedulerT  = frame::Scheduler<frame::aio::Reactor>;
using SecureContextT = frame::aio::openssl::Context;

namespace {
LoggerT logger("test");

using CallPoolT     = locking::CallPoolT<void()>;
using SynchContextT = decltype(declval<CallPoolT>().createSynchronizationContext());

inline auto milliseconds_since_epoch(const std::chrono::system_clock::time_point& _time = std::chrono::system_clock::now())
{
    using namespace std::chrono;
    const uint64_t ms = duration_cast<milliseconds>(_time.time_since_epoch()).count();
    return ms;
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
    void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent) override;
};

struct Message : frame::mprpc::Message {
    uint32_t topic_id_;
    uint32_t iteration_   = 0;
    uint64_t time_offset_ = -1;
    uint64_t topic_value_ = 0;

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        using ReflectorT = decay_t<decltype(_rr)>;

        _rr.add(_rthis.topic_id_, _rctx, 1, "topic_id");
        _rr.add(_rthis.iteration_, _rctx, 2, "iteration");
        _rr.add(_rthis.time_offset_, _rctx, 3, "time_offset");
        _rr.add(_rthis.topic_value_, _rctx, 4, "topic_value");

        if constexpr (ReflectorT::is_const_reflector) {
        }
    }

    Message() = default;

    Message(
        const uint32_t _topic_id,
        const uint64_t _time_offset)
        : topic_id_(_topic_id)
        , time_offset_(_time_offset)
    {
    }
};

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

void client_complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror);

void server_complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& /*_rerror*/);

void wait();
//-----------------------------------------------------------------------------
vector<frame::ActorIdT> worker_actor_id_vec;
CallPoolT               worker_pool;
size_t                  per_message_loop_count = 10;
std::atomic<size_t>     active_message_count;
std::atomic<uint64_t>   max_time_delta{0};
std::atomic<uint64_t>   min_time_delta{std::numeric_limits<uint64_t>::max()};
std::atomic<uint64_t>   max_time_server_trip_delta{0};
std::atomic<uint64_t>   min_time_server_trip_delta{std::numeric_limits<uint64_t>::max()};
promise<void>           promise;
//-----------------------------------------------------------------------------
} // namespace

int test_clientserver_topic(int argc, char* argv[])
{

    solid::log_start(std::cerr, {".*:EWX", "test:EWX"});
    // solid::log_start(std::cerr, {"solid::frame::mprpc.*:EWX", "\\*:VIEWX"});

    size_t topic_count                   = 1000;
    bool   secure                        = false;
    bool   compress                      = false;
    size_t max_per_pool_connection_count = 100;
    size_t message_count                 = 10000;

    {
        AioSchedulerT          sch_client;
        AioSchedulerT          sch_server;
        frame::Manager         m;
        frame::mprpc::ServiceT mprpcserver(m);
        frame::mprpc::ServiceT mprpcclient(m);
        frame::ServiceT        generic_service{m};
        ErrorConditionT        err;
        frame::aio::Resolver   resolver([](std::function<void()>&& _fnc) { worker_pool.push(std::move(_fnc)); });

        worker_pool.start(WorkPoolConfiguration(4));
        sch_client.start(1);
        sch_server.start(2);

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
                    frame::make_actor<WorkerActor>(sch_server, generic_service, i, make_event(GenericEvents::Start, topic_vec), err));
            }
            solid_dbg(logger, Info, "Created topics for " << worker_count << " workers");
        }

        std::string server_port;

        { // mprpc server initialization
            auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
                reflection::v1::metadata::factory,
                [&](auto& _rmap) {
                    _rmap.template registerMessage<Message>(1, "Message", server_complete_message);
                });
            frame::mprpc::Configuration cfg(sch_server, proto);

            cfg.connection_stop_fnc         = &server_connection_stop;
            cfg.server.connection_start_fnc = &server_connection_start;

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

            mprpcserver.start(std::move(cfg));

            {
                std::ostringstream oss;
                oss << mprpcserver.configuration().server.listenerPort();
                server_port = oss.str();
                solid_dbg(logger, Info, "server listens on port: " << server_port);
            }
        }

        { // mprpc client initialization
            auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
                reflection::v1::metadata::factory,
                [&](auto& _rmap) {
                    _rmap.template registerMessage<Message>(1, "Message", client_complete_message);
                });
            frame::mprpc::Configuration cfg(sch_client, proto);

            cfg.connection_stop_fnc         = &client_connection_stop;
            cfg.client.connection_start_fnc = &client_connection_start;

            cfg.pool_max_active_connection_count = max_per_pool_connection_count;
            cfg.pool_max_message_queue_size      = message_count;

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

        active_message_count = message_count;

        for (size_t i = 0; i < message_count; ++i) {
            mprpcclient.sendMessage("", std::make_shared<Message>(i, milliseconds_since_epoch()), {frame::mprpc::MessageFlagsE::AwaitResponse});
        }

        wait();

        cout << "max delta        : " << max_time_delta.load() << endl;
        cout << "min delta        : " << min_time_delta.load() << endl;
        cout << "max server delta : " << max_time_server_trip_delta.load() << endl;
        cout << "min server delta : " << min_time_server_trip_delta.load() << endl;

        mprpcserver.stop();
        generic_service.stop();
        worker_pool.stop();
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
void WorkerActor::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    if (_revent == generic_event_start) {
        solid_assert(!local_worker_context_ptr);
        local_worker_context_ptr = make_unique<WorkerContext>(std::move(*_revent.any().cast<vector<shared_ptr<Topic>>>()));
        solid_dbg(logger, Info, "Created local WorkerContex " << local_worker_context_ptr.get() << " " << local_worker_context_ptr->topic_vec_.size());
    } else if (_revent == generic_event_kill) {
        local_worker_context_ptr.reset();
        postStop(_rctx);
    }
}

void client_complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& _rerror)
{
    solid_dbg(logger, Info, _rctx.recipientId());

    if (_rrecv_msg_ptr) {
        solid_check(_rsent_msg_ptr);
        solid_check(_rrecv_msg_ptr->time_offset_ >= _rsent_msg_ptr->time_offset_);
        solid_check(_rrecv_msg_ptr->topic_value_ > _rsent_msg_ptr->topic_value_);

        const auto now = milliseconds_since_epoch();

        solid_statistic_max(max_time_delta, now - _rsent_msg_ptr->time_offset_);
        solid_statistic_min(min_time_delta, now - _rsent_msg_ptr->time_offset_);
        solid_statistic_max(max_time_server_trip_delta, _rrecv_msg_ptr->time_offset_ - _rsent_msg_ptr->time_offset_);
        solid_statistic_min(min_time_server_trip_delta, _rrecv_msg_ptr->time_offset_ - _rsent_msg_ptr->time_offset_);

        _rrecv_msg_ptr->time_offset_ = now;
        ++_rrecv_msg_ptr->iteration_;

        if (_rrecv_msg_ptr->iteration_ < per_message_loop_count) {
            _rrecv_msg_ptr->clearHeader();
            _rctx.service().sendMessage("", std::move(_rrecv_msg_ptr), {frame::mprpc::MessageFlagsE::AwaitResponse});
        } else {
            if (active_message_count.fetch_sub(1) == 1) {
                promise.set_value();
            }
        }
    }
}

void server_complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Message>& _rsent_msg_ptr, std::shared_ptr<Message>& _rrecv_msg_ptr,
    ErrorConditionT const& /*_rerror*/)
{
    if (_rrecv_msg_ptr) {
        solid_dbg(logger, Info, _rctx.recipientId());

        if (!_rrecv_msg_ptr->isOnPeer()) {
            solid_throw("Message not on peer!.");
        }

        auto& topic_ptr = local_worker_context_ptr->topic_vec_[_rrecv_msg_ptr->topic_id_ % local_worker_context_ptr->topic_vec_.size()];
        topic_ptr->synch_ctx_.push(
            [topic_ptr, _rrecv_msg_ptr = std::move(_rrecv_msg_ptr), &service = _rctx.service(), recipient_id = _rctx.recipientId()]() {
                ++topic_ptr->value_;
                _rrecv_msg_ptr->topic_value_ = topic_ptr->value_;
                _rrecv_msg_ptr->time_offset_ = milliseconds_since_epoch();
                service.sendResponse(recipient_id, _rrecv_msg_ptr);
            });
    }
    if (_rsent_msg_ptr) {
        solid_dbg(logger, Info, _rctx.recipientId() << " done sent message " << _rsent_msg_ptr.get());
    }
}

//-----------------------------------------------------------------------------
} // namespace
