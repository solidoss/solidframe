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

#include "solid/utility/threadpool.hpp"

#include "solid/system/exception.hpp"

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

size_t real_size(size_t _sz)
{
    // offset + (align - (offset mod align)) mod align
    return _sz + ((sizeof(uint64_t) - (_sz % sizeof(uint64_t))) % sizeof(uint64_t));
}

struct Request : frame::mprpc::Message {
    uint32_t    idx;
    std::string str;

    Request(uint32_t _idx)
        : idx(_idx)
    {
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << (void*)this << " idx = " << idx);
        init();
    }
    Request()
    {
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << (void*)this);
    }
    ~Request() override
    {
        solid_dbg(generic_logger, Info, "DELETE ---------------- " << (void*)this);
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.idx, _rctx, 0, "idx").add(_rthis.str, _rctx, 1, "str");
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
            pu[i] = pup[i % pattern_size]; // pattern[i % pattern.size()];
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
            if (pu[i] != pup[i % pattern_size]) {
                return false;
            }
        }
        return true;
    }
};

struct Response : frame::mprpc::Message {
    uint32_t    idx;
    std::string str;

    Response(const Request& _rreq)
        : frame::mprpc::Message(_rreq)
        , idx(_rreq.idx)
        , str(_rreq.str)
    {
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << (void*)this);
    }

    Response()
    {
        solid_dbg(generic_logger, Info, "CREATE ---------------- " << (void*)this);
    }

    ~Response() override
    {
        solid_dbg(generic_logger, Info, "DELETE ---------------- " << (void*)this);
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.idx, _rctx, 0, "idx").add(_rthis.str, _rctx, 1, "str");
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

// void client_receive_request(frame::mprpc::ConnectionContext &_rctx, std::shared_ptr<Request> &_rmsgptr){
//  solid_dbg(generic_logger, Info, _rctx.recipientId());
//  solid_throw("Received request on client.");
// }

void client_complete_request(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Request>& /*_rsendmsgptr*/,
    std::shared_ptr<Response>& /*_rrecvmsgptr*/,
    ErrorConditionT const& /*_rerr*/)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());
    solid_throw("Should not be called");
}

void client_complete_response(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Response>& /*_rsendmsgptr*/,
    std::shared_ptr<Response>& /*_rrecvmsgptr*/,
    ErrorConditionT const& /*_rerr*/)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());
    solid_throw("Should not be called");
}

void on_receive_response(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Request>&        _rreqmsgptr,
    std::shared_ptr<Response>&       _rresmsgptr,
    ErrorConditionT const& /*_rerr*/)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());

    if (!_rreqmsgptr) {
        solid_throw("Request should not be empty");
    }

    if (!_rresmsgptr) {
        solid_throw("Response should not be empty");
    }

    ++crtbackidx;
    ++crtackidx;

    transfered_size += _rresmsgptr->str.size();
    ++transfered_count;

    if (!_rresmsgptr->isBackOnSender()) {
        solid_throw("Message not back on sender!.");
    }

    if (crtbackidx == writecount) {
        lock_guard<mutex> lock(mtx);
        running = false;
        cnd.notify_one();
    }
}

struct ResponseHandler {
    void operator()(
        frame::mprpc::ConnectionContext& _rctx,
        std::shared_ptr<Request>&        _rreqmsgptr,
        std::shared_ptr<Response>&       _rresmsgptr,
        ErrorConditionT const&           _rerr)
    {
        on_receive_response(_rctx, _rreqmsgptr, _rresmsgptr, _rerr);
    }
};

void server_complete_request(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Request>&        _rsendmsgptr,
    std::shared_ptr<Request>&        _rrecvmsgptr,
    ErrorConditionT const&           _rerr)
{
    if (_rerr) {
        solid_throw("Error");
        return;
    }

    if (_rsendmsgptr) {
        solid_throw("Server does not send Request");
        return;
    }

    if (!_rrecvmsgptr) {
        solid_throw("Server should receive Request");
        return;
    }

    solid_dbg(generic_logger, Info, _rctx.recipientId() << " message id on sender " << _rrecvmsgptr->senderRequestId());

    if (!_rrecvmsgptr->check()) {
        solid_throw("Message check failed.");
    }

    if (!_rrecvmsgptr->isOnPeer()) {
        solid_throw("Message not on peer!.");
    }

    // send message back
    frame::mprpc::MessagePointerT msgptr(std::make_shared<Response>(*_rrecvmsgptr));
    _rctx.service().sendResponse(_rctx.recipientId(), msgptr);

    ++crtreadidx;

    solid_dbg(generic_logger, Info, crtreadidx);

    if (crtwriteidx < writecount) {
        frame::mprpc::MessagePointerT msgptr(std::make_shared<Request>(crtwriteidx));
        ++crtwriteidx;
        pmprpcclient->sendRequest(
            "localhost", msgptr,
            // on_receive_response
            ResponseHandler()
            /*[](frame::mprpc::ConnectionContext &_rctx, std::shared_ptr<Response> &_rmsgptr, ErrorConditionT const &_rerr)->void{
                    on_receive_response(_rctx, _rmsgptr, _rerr);
                }*/
            ,
            initarray[crtwriteidx % initarraysize].flags);
    }
}

void server_complete_response(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Response>&       _rsendmsgptr,
    std::shared_ptr<Response>&       _rrecvmsgptr,
    ErrorConditionT const&           _rerr)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());

    if (_rerr) {
        solid_throw("Error");
        return;
    }

    if (!_rsendmsgptr) {
        solid_throw("Send message should not be empty");
    }

    if (_rrecvmsgptr) {
        solid_throw("Recv message should be empty");
    }
}

} // namespace

int test_clientserver_sendrequest(int argc, char* argv[])
{
    solid::log_start(std::cerr, {".*:EWX"});

    size_t max_per_pool_connection_count = 1;

    if (argc > 1) {
        max_per_pool_connection_count = atoi(argv[1]);
    }

    bool secure = false;

    if (argc > 2) {
        if (*argv[2] == 's' || *argv[2] == 'S') {
            secure = true;
        }
    }

    for (int i = 0; i < 127; ++i) {
        if (isprint(i) != 0 && isblank(i) == 0) {
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
        frame::mprpc::ServiceT mprpcserver(m);
        frame::mprpc::ServiceT mprpcclient(m);
        ErrorConditionT        err;
        CallPoolT              cwp{1, 100, 0, [](const size_t) {}, [](const size_t) {}};
        frame::aio::Resolver   resolver([&cwp](std::function<void()>&& _fnc) { cwp.pushOne(std::move(_fnc)); });

        sch_client.start(1);
        sch_server.start(1);

        std::string server_port;

        { // mprpc server initialization
            auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
                reflection::v1::metadata::factory,
                [&](auto& _rmap) {
                    _rmap.template registerMessage<Request>(1, "Request", server_complete_request);
                    _rmap.template registerMessage<Response>(2, "Response", server_complete_response);
                });
            frame::mprpc::Configuration cfg(sch_server, proto);

            // cfg.recv_buffer_capacity = 1024;
            // cfg.send_buffer_capacity = 1024;

            cfg.connection_stop_fnc         = &server_connection_stop;
            cfg.server.connection_start_fnc = &server_connection_start;

            cfg.server.listener_address_str = "0.0.0.0:0";

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
                    _rmap.template registerMessage<Request>(1, "Request", client_complete_request);
                    _rmap.template registerMessage<Response>(2, "Response", client_complete_response);
                });
            frame::mprpc::Configuration cfg(sch_client, proto);

            // cfg.recv_buffer_capacity = 1024;
            // cfg.send_buffer_capacity = 1024;

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

        const size_t start_count = initarraysize / 2;

        writecount = initarraysize;

        for (; crtwriteidx < start_count;) {
            frame::mprpc::MessagePointerT msgptr(std::make_shared<Request>(crtwriteidx));
            ++crtwriteidx;
            mprpcclient.sendRequest(
                "localhost", msgptr,

                // ResponseHandler()
                [](
                    frame::mprpc::ConnectionContext& _rctx,
                    std::shared_ptr<Request>&        _rreqmsgptr,
                    std::shared_ptr<Response>&       _rresmsgptr,
                    ErrorConditionT const&           _rerr) -> void {
                    on_receive_response(_rctx, _rreqmsgptr, _rresmsgptr, _rerr);
                },
                initarray[crtwriteidx % initarraysize].flags);
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

    // exiting

    std::cout << "Transfered size = " << (transfered_size * 2) / 1024 << "KB" << endl;
    std::cout << "Transfered count = " << transfered_count << endl;
    std::cout << "Connection count = " << connection_count << endl;

    return 0;
}
