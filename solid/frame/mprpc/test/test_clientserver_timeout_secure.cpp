#include "solid/frame/mprpc/mprpcsocketstub_openssl.hpp"

#include "solid/frame/mprpc/mprpccompression_snappy.hpp"
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
#include "solid/frame/aio/aiosocket.hpp"
#include "solid/frame/aio/aiostream.hpp"
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

atomic<int>        wait_count{0};
bool               running = true;
mutex              mtx;
condition_variable cnd;
string             server_port;

frame::aio::Resolver& async_resolver(frame::aio::Resolver* _pres = nullptr)
{
    static frame::aio::Resolver& r = *_pres;
    return r;
}

struct Activate : frame::mprpc::Message {
    uint32_t     idx;
    std::string  str;
    mutable bool serialized;

    Activate(uint32_t _idx)
        : idx(_idx)
        , serialized(false)
    {
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << (void*)this << " idx = " << idx);
    }

    Activate()
        : serialized(false)
    {
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << (void*)this);
    }
    ~Activate() override
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
};

void server_connection_start(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());
}

void server_connection_stop(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << _rctx.error().message());
}

void server_complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Activate>& _rsent_msg_ptr, std::shared_ptr<Activate>& _rrecv_msg_ptr,
    ErrorConditionT const& /*_rerror*/)
{
    solid_check(false);
}

class Connection : public frame::aio::Actor {
    enum { BufferCapacity = 1024 * 2 };
    char buf[BufferCapacity];
    using StreamSocketT = frame::aio::Stream<frame::aio::Socket>;
    StreamSocketT sock;

public:
    Connection()
        : sock(this->proxy())
    {
    }

private:
    void connect(frame::aio::ReactorContext& _rctx, SocketAddressStub const& _rsas)
    {
        if (sock.connect(_rctx, _rsas, [](frame::aio::ReactorContext& _rctx) {
                onConnect(_rctx);
            })) {
            onConnect(_rctx);
        }
    }
    void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent) override
    {
        solid_dbg(generic_logger, Info, "event = " << _revent);
        if (_revent == generic_event_start) {
            //we must resolve the address then connect
            solid_dbg(generic_logger, Info, "async_resolve = "
                    << "127.0.0.1"
                    << " " << server_port);
            async_resolver().requestResolve(
                [&rm = _rctx.service().manager(), actor_id = _rctx.service().manager().id(*this), this](ResolveData& _rrd, ErrorCodeT const& /*_rerr*/) {
                    Event ev(make_event(GenericEvents::Message));

                    ev.any() = std::move(_rrd);

                    solid_dbg(generic_logger, Info, this << " send resolv_message");
                    rm.notify(actor_id, std::move(ev));
                },
                "127.0.0.1", server_port.c_str(), 0, SocketInfo::Inet4, SocketInfo::Stream);
        } else if (_revent == generic_event_kill) {
            postStop(_rctx);
        } else if (generic_event_message == _revent) {
            ResolveData* presolvemsg = _revent.any().cast<ResolveData>();
            if (presolvemsg != nullptr) {
                if (presolvemsg->empty()) {
                    solid_dbg(generic_logger, Error, this << " postStop");
                    postStop(_rctx);
                } else {
                    connect(_rctx, presolvemsg->begin());
                }
            }
        }
    }

    static void onConnect(frame::aio::ReactorContext& _rctx)
    {
        Connection& rthis = static_cast<Connection&>(_rctx.actor());

        if (!_rctx.error()) {
            solid_dbg(generic_logger, Error, &rthis << " SUCCESS");
            rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv);
        } else {
            solid_dbg(generic_logger, Error, &rthis << " postStop " << _rctx.systemError().message());
            solid_check(false);
        }
    }

    static void onRecv(frame::aio::ReactorContext& _rctx, size_t _sz)
    {
        //Connection& rthis = static_cast<Connection&>(_rctx.actor());

        if (_rctx.error()) {
            if (wait_count.fetch_sub(1) == 1) {
                unique_lock<mutex> lock(mtx);
                running = false;
                cnd.notify_one();
            }
        }
    }
};

} //namespace

int test_clientserver_timeout_secure(int argc, char* argv[])
{

    solid::log_start(std::cerr, {".*:EW", "\\*:VIEW"});
    int connection_count = 10;

    if (argc > 1) {
        connection_count = atoi(argv[1]);
    }

    char server_option = 's';

    if (argc > 2) {
        if (*argv[2] == 's' || *argv[2] == 'S') {
            server_option = 's'; //secure
        }
        if (*argv[2] == 'p' || *argv[2] == 'P') {
            server_option = 'p'; //passive
        }
        if (*argv[2] == 'a' || *argv[2] == 'A') {
            server_option = 'a'; //active
        }
    }

    {
        AioSchedulerT                     sch_server;
        AioSchedulerT                     sch_client;
        frame::Manager                    m;
        frame::mprpc::ServiceT            mprpcserver(m);
        ErrorConditionT                   err;
        lockfree::CallPoolT<void(), void> cwp{WorkPoolConfiguration(1)};
        frame::aio::Resolver              resolver([&cwp](std::function<void()>&& _fnc) { cwp.push(std::move(_fnc)); });
        frame::ServiceT                   svc_client{m};

        async_resolver(&resolver);

        sch_client.start(1);
        sch_server.start(1);

        { //mprpc server initialization
            auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
                reflection::v1::metadata::factory,
                [&](auto& _rmap) {
                    _rmap.template registerMessage<Activate>(1, "Activate", server_complete_message);
                });
            frame::mprpc::Configuration cfg(sch_server, proto);

            //cfg.recv_buffer_capacity = 1024;
            //cfg.send_buffer_capacity = 1024;

            cfg.connection_stop_fnc                   = &server_connection_stop;
            cfg.server.connection_start_fnc           = &server_connection_start;
            cfg.connection_timeout_inactivity_seconds = 5;
            cfg.connection_timeout_keepalive_seconds  = 3;
            cfg.server.connection_start_state         = frame::mprpc::ConnectionState::Passive;

            cfg.server.listener_address_str = "0.0.0.0:0";

            if (server_option == 's') {

                cfg.server.connection_timeout_activation_seconds = 1000;
                cfg.server.connection_timeout_secured_seconds    = 5;

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
            } else if (server_option == 'p') {
                cfg.server.connection_timeout_activation_seconds = 5;
                cfg.server.connection_timeout_secured_seconds    = 1000;

            } else if (server_option == 'a') {
                cfg.server.connection_start_state = frame::mprpc::ConnectionState::Active;
            }

            mprpcserver.start(std::move(cfg));

            {
                std::ostringstream oss;
                oss << mprpcserver.configuration().server.listenerPort();
                server_port = oss.str();
                solid_dbg(generic_logger, Info, "server listens on port: " << server_port);
            }
        }

        wait_count = connection_count;

        for (int i = 0; i < connection_count; ++i) {
            solid::ErrorConditionT err;
            solid::frame::ActorIdT actuid;

            actuid = sch_client.startActor(make_shared<Connection>(), svc_client, make_event(GenericEvents::Start), err);

            if (actuid.isInvalid()) {
                solid_check(false);
            }
            solid_dbg(generic_logger, Info, "Started Connection Actor: " << actuid.index << ',' << actuid.unique);
        }

        unique_lock<mutex> lock(mtx);

        if (!cnd.wait_for(lock, std::chrono::seconds(50), []() { return !running; })) {
            solid_throw("Process is taking too long.");
        }

        //m.stop();
    }

    //exiting

    std::cout << "Connection count = " << connection_count << endl;

    return 0;
}
