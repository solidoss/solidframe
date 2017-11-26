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
#include "solid/frame/mpipc/mpipcservice.hpp"

#include "solid/frame/mpipc/mpipcprotocol_serialization_v1.hpp"
#include "solid/frame/mpipc/mpipcsocketstub_openssl.hpp"

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

size_t real_size(size_t _sz)
{
    //offset + (align - (offset mod align)) mod align
    return _sz + ((sizeof(uint64_t) - (_sz % sizeof(uint64_t))) % sizeof(uint64_t));
}

struct Request : frame::mpipc::Message {
    uint32_t    idx;
    std::string str;

    Request(uint32_t _idx)
        : idx(_idx)
    {
        idbg("CREATE ---------------- " << (void*)this << " idx = " << idx);
        init();
    }
    Request()
    {
        idbg("CREATE ---------------- " << (void*)this);
    }
    ~Request()
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

struct Response : frame::mpipc::Message {
    uint32_t    idx;
    std::string str;

    Response(const Request& _rreq)
        : frame::mpipc::Message(_rreq)
        , idx(_rreq.idx)
        , str(_rreq.str)
    {
        idbg("CREATE ---------------- " << (void*)this);
    }

    Response()
    {
        idbg("CREATE ---------------- " << (void*)this);
    }

    ~Response()
    {
        idbg("DELETE ---------------- " << (void*)this);
    }

    template <class S>
    void solidSerialize(S& _s, frame::mpipc::ConnectionContext& _rctx)
    {
        _s.push(idx, "idx");
        _s.push(str, "str");
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
    auto lambda = [](frame::mpipc::ConnectionContext&, ErrorConditionT const& _rerror) {
        idbg("enter active error: " << _rerror.message());
        return frame::mpipc::MessagePointerT();
    };
    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId(), lambda);
}

void server_connection_stop(frame::mpipc::ConnectionContext& _rctx)
{
    idbg(_rctx.recipientId() << " error: " << _rctx.error().message());
}

void server_connection_start(frame::mpipc::ConnectionContext& _rctx)
{
    idbg(_rctx.recipientId());
    auto lambda = [](frame::mpipc::ConnectionContext&, ErrorConditionT const& _rerror) {
        idbg("enter active error: " << _rerror.message());
        return frame::mpipc::MessagePointerT();
    };
    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId(), lambda);
}

// void client_receive_request(frame::mpipc::ConnectionContext &_rctx, std::shared_ptr<Request> &_rmsgptr){
//  idbg(_rctx.recipientId());
//  SOLID_THROW("Received request on client.");
// }

void client_complete_request(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<Request>&        _rsendmsgptr,
    std::shared_ptr<Response>&       _rrecvmsgptr,
    ErrorConditionT const&           _rerr)
{
    idbg(_rctx.recipientId());
    SOLID_THROW("Should not be called");
}

void client_complete_response(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<Response>&       _rsendmsgptr,
    std::shared_ptr<Response>&       _rrecvmsgptr,
    ErrorConditionT const&           _rerr)
{
    idbg(_rctx.recipientId());
    SOLID_THROW("Should not be called");
}

void on_receive_response(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<Request>&        _rreqmsgptr,
    std::shared_ptr<Response>&       _rresmsgptr,
    ErrorConditionT const&           _rerr)
{
    idbg(_rctx.recipientId());

    if (not _rreqmsgptr) {
        SOLID_THROW("Request should not be empty");
    }

    if (not _rresmsgptr) {
        SOLID_THROW("Response should not be empty");
    }

    ++crtbackidx;
    ++crtackidx;

    transfered_size += _rresmsgptr->str.size();
    ++transfered_count;

    if (!_rresmsgptr->isBackOnSender()) {
        SOLID_THROW("Message not back on sender!.");
    }

    if (crtbackidx == writecount) {
        unique_lock<mutex> lock(mtx);
        running = false;
        cnd.notify_one();
    }
}

struct ResponseHandler {
    void operator()(
        frame::mpipc::ConnectionContext& _rctx,
        std::shared_ptr<Request>&        _rreqmsgptr,
        std::shared_ptr<Response>&       _rresmsgptr,
        ErrorConditionT const&           _rerr)
    {
        on_receive_response(_rctx, _rreqmsgptr, _rresmsgptr, _rerr);
    }
};

void server_complete_request(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<Request>&        _rsendmsgptr,
    std::shared_ptr<Request>&        _rrecvmsgptr,
    ErrorConditionT const&           _rerr)
{
    if (_rerr) {
        SOLID_THROW("Error");
        return;
    }

    if (_rsendmsgptr.get()) {
        SOLID_THROW("Server does not send Request");
        return;
    }

    if (not _rrecvmsgptr) {
        SOLID_THROW("Server should receive Request");
        return;
    }

    idbg(_rctx.recipientId() << " message id on sender " << _rrecvmsgptr->senderRequestId());

    if (not _rrecvmsgptr->check()) {
        SOLID_THROW("Message check failed.");
    }

    if (!_rrecvmsgptr->isOnPeer()) {
        SOLID_THROW("Message not on peer!.");
    }

    //send message back
    frame::mpipc::MessagePointerT msgptr(new Response(*_rrecvmsgptr));
    _rctx.service().sendResponse(_rctx.recipientId(), msgptr);

    ++crtreadidx;

    idbg(crtreadidx);

    if (crtwriteidx < writecount) {
        frame::mpipc::MessagePointerT msgptr(new Request(crtwriteidx));
        ++crtwriteidx;
        pmpipcclient->sendRequest(
            "localhost", msgptr,
            //on_receive_response
            ResponseHandler()
            /*[](frame::mpipc::ConnectionContext &_rctx, std::shared_ptr<Response> &_rmsgptr, ErrorConditionT const &_rerr)->void{
                    on_receive_response(_rctx, _rmsgptr, _rerr);
                }*/,
            initarray[crtwriteidx % initarraysize].flags);
    }
}

void server_complete_response(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<Response>&       _rsendmsgptr,
    std::shared_ptr<Response>&       _rrecvmsgptr,
    ErrorConditionT const&           _rerr)
{
    idbg(_rctx.recipientId());

    if (_rerr) {
        SOLID_THROW("Error");
        return;
    }

    if (not _rsendmsgptr) {
        SOLID_THROW("Send message should not be empty");
    }

    if (_rrecvmsgptr) {
        SOLID_THROW("Recv message should be empty");
    }
}

} //namespace

int test_clientserver_sendrequest(int argc, char** argv)
{
#ifdef SOLID_HAS_DEBUG
    Debug::the().levelMask("ew");
    Debug::the().moduleMask("any");
    Debug::the().initStdErr(false, nullptr);
#endif

    size_t max_per_pool_connection_count = 1;

    if (argc > 1) {
        max_per_pool_connection_count = atoi(argv[1]);
    }

    bool secure = false;

    if (argc > 2) {
        if (*argv[2] == 's' or *argv[2] == 'S') {
            secure = true;
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

            proto->registerType<Request>(
                server_complete_request);
            proto->registerType<Response>(
                server_complete_response);
            //cfg.recv_buffer_capacity = 1024;
            //cfg.send_buffer_capacity = 1024;

            cfg.connection_stop_fnc         = server_connection_stop;
            cfg.server.connection_start_fnc = server_connection_start;

            cfg.server.listener_address_str = "0.0.0.0:0";

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
            auto                        proto = frame::mpipc::serialization_v1::Protocol::create();
            frame::mpipc::Configuration cfg(sch_client, proto);

            proto->registerType<Request>(
                client_complete_request);
            proto->registerType<Response>(
                client_complete_response);

            //cfg.recv_buffer_capacity = 1024;
            //cfg.send_buffer_capacity = 1024;

            cfg.connection_stop_fnc         = client_connection_stop;
            cfg.client.connection_start_fnc = client_connection_start;

            cfg.pool_max_active_connection_count = max_per_pool_connection_count;

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

        const size_t start_count = initarraysize / 2;

        writecount = initarraysize;

        for (; crtwriteidx < start_count;) {
            frame::mpipc::MessagePointerT msgptr(new Request(crtwriteidx));
            ++crtwriteidx;
            mpipcclient.sendRequest(
                "localhost", msgptr,

                //ResponseHandler()
                [](
                    frame::mpipc::ConnectionContext& _rctx,
                    std::shared_ptr<Request>&        _rreqmsgptr,
                    std::shared_ptr<Response>&       _rresmsgptr,
                    ErrorConditionT const&           _rerr) -> void {
                    on_receive_response(_rctx, _rreqmsgptr, _rresmsgptr, _rerr);
                },
                initarray[crtwriteidx % initarraysize].flags);
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
