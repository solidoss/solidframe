#include "solid/frame/aio/openssl/aiosecurecontext.hpp"
#include "solid/frame/aio/openssl/aiosecuresocket.hpp"

#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioactor.hpp"
#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioresolver.hpp"
#include "solid/frame/aio/aiosocket.hpp"
#include "solid/frame/aio/aiostream.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/system/socketaddress.hpp"
#include "solid/system/socketdevice.hpp"

#include "solid/utility/event.hpp"

#include <functional>
#include <iostream>
#include <signal.h>
#include <sstream>

using namespace std;
using namespace solid;

using AioSchedulerT  = frame::Scheduler<frame::aio::Reactor>;
using AtomicSizeT    = atomic<size_t>;
using SecureContextT = frame::aio::openssl::Context;
//-----------------------------------------------------------------------------
namespace {
bool                 running = true;
mutex                mtx;
condition_variable   cnd;
size_t               concnt       = 0;
size_t               recv_count   = 0;
const size_t         repeat_count = 10000;
std::string          srv_port_str;
std::string          rly_port_str;
bool                 be_secure       = false;
bool                 use_relay       = false;
unsigned             wait_seconds    = 100;
constexpr const bool enable_no_delay = true;
} //namespace
//-----------------------------------------------------------------------------
frame::aio::Resolver& async_resolver(frame::aio::Resolver* _pres = nullptr);
//-----------------------------------------------------------------------------
namespace server {
class Listener final : public frame::aio::Actor {
public:
    static size_t backlog_size()
    {
        return SocketInfo::max_listen_backlog_size();
    }

    Listener(
        frame::Service& _rsvc,
        AioSchedulerT&  _rsched,
        SocketDevice&&  _rsd,
        SecureContextT* _psecurectx)
        : rsvc(_rsvc)
        , rsch(_rsched)
        , sock(this->proxy(), std::move(_rsd))
        , psecurectx(_psecurectx)
    {
    }
    ~Listener() override
    {
    }

private:
    void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent) override;
    void onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd);

    typedef frame::aio::Listener ListenerSocketT;

    frame::Service& rsvc;
    AioSchedulerT&  rsch;
    ListenerSocketT sock;
    SecureContextT* psecurectx;
};
//-----------------------------------------------------------------------------
class Connection : public frame::aio::Actor {
    virtual void start(frame::aio::ReactorContext& _rctx)                  = 0;
    virtual void postRecvSome(frame::aio::ReactorContext& _rctx)           = 0;
    virtual bool sendAll(frame::aio::ReactorContext& _rctx, size_t _sz)    = 0;
    virtual bool recvSome(frame::aio::ReactorContext& _rctx, size_t& _rsz) = 0;

protected:
    Connection()
        : recvcnt(0)
        , sendcnt(0)
    {
    }
    ~Connection() override {}
    void        onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent) override;
    static void onRecv(frame::aio::ReactorContext& _rctx, size_t _sz);
    static void onSend(frame::aio::ReactorContext& _rctx);

protected:
    enum { BufferCapacity = 1024 * 2 };

    char     buf[BufferCapacity];
    uint64_t recvcnt;
    uint64_t sendcnt;
    size_t   sendcrt;
};

class PlainConnection final : public Connection {
public:
    PlainConnection(SocketDevice&& _rsd)
        : sock(this->proxy(), std::move(_rsd))
    {
    }

private:
    void postRecvSome(frame::aio::ReactorContext& _rctx) override
    {
        sock.postRecvSome(_rctx, buf, BufferCapacity, Connection::onRecv);
    }
    bool sendAll(frame::aio::ReactorContext& _rctx, size_t _sz) override
    {
        return sock.sendAll(_rctx, buf, _sz, Connection::onSend);
    }
    bool recvSome(frame::aio::ReactorContext& _rctx, size_t& _rsz) override
    {
        return sock.recvSome(_rctx, buf, BufferCapacity, Connection::onRecv, _rsz);
    }
    void start(frame::aio::ReactorContext& _rctx) override
    {
        postRecvSome(_rctx);
    }

private:
    using StreamSocketT = frame::aio::Stream<frame::aio::Socket>;
    StreamSocketT sock;
};

class SecureConnection final : public Connection {
public:
    SecureConnection(SocketDevice&& _rsd, SecureContextT& _rctx)
        : sock(this->proxy(), std::move(_rsd), _rctx)
    {
    }

private:
    void postRecvSome(frame::aio::ReactorContext& _rctx) override
    {
        sock.postRecvSome(_rctx, buf, BufferCapacity, Connection::onRecv);
    }
    bool sendAll(frame::aio::ReactorContext& _rctx, size_t _sz) override
    {
        return sock.sendAll(_rctx, buf, _sz, Connection::onSend);
    }
    bool recvSome(frame::aio::ReactorContext& _rctx, size_t& _rsz) override
    {
        return sock.recvSome(_rctx, buf, BufferCapacity, Connection::onRecv, _rsz);
    }
    void start(frame::aio::ReactorContext& _rctx) override
    {
        sock.secureSetCheckHostName(_rctx, "echo-client");
        sock.secureSetVerifyCallback(_rctx, frame::aio::openssl::VerifyModePeer, onSecureVerify);
        if (sock.secureAccept(_rctx, onSecureAccept)) {
            if (!_rctx.error()) {
                sock.postRecvSome(_rctx, buf, BufferCapacity, Connection::onRecv); //fully asynchronous call
            } else {
                solid_dbg(generic_logger, Error, this << " postStop: " << _rctx.systemError().message());
                postStop(_rctx);
            }
        }
    }
    static void onSecureAccept(frame::aio::ReactorContext& _rctx)
    {
        SecureConnection& rthis = static_cast<SecureConnection&>(_rctx.actor());
        if (!_rctx.error()) {
            solid_dbg(generic_logger, Info, &rthis << " postRecvSome");
            rthis.postRecvSome(_rctx); //fully asynchronous call
        } else {
            solid_dbg(generic_logger, Error, &rthis << " postStop " << rthis.recvcnt << " " << rthis.sendcnt << " error " << _rctx.systemError().message());
            rthis.postStop(_rctx);
        }
    }
    static bool onSecureVerify(frame::aio::ReactorContext& _rctx, bool _preverified, frame::aio::openssl::VerifyContext& /*_rverify_ctx*/)
    {
        SecureConnection& rthis = static_cast<SecureConnection&>(_rctx.actor());
        solid_dbg(generic_logger, Info, &rthis << " " << _preverified);
        return _preverified;
    }

private:
    using StreamSocketT = frame::aio::Stream<frame::aio::openssl::Socket>;
    StreamSocketT sock;
};

} //namespace server

//-----------------------------------------------------------------------------

namespace client {
class Connection : public frame::aio::Actor {
protected:
    Connection(const size_t _idx)
        : recvcnt(0)
        , sendcnt(0)
        , idx(_idx)
        , crt_send_idx(0)
        , expect_recvcnt(0)
    {
    }
    ~Connection() override
    {
    }

private:
    virtual void connect(frame::aio::ReactorContext& _rctx, SocketAddressStub const& _rsas)    = 0;
    virtual void postRecvSome(frame::aio::ReactorContext& _rctx)                               = 0;
    virtual void postSendAll(frame::aio::ReactorContext& _rctx, const char* _pbuf, size_t _sz) = 0;

private:
    void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent) override;
    void onStop(frame::Manager& /*_rm*/) override
    {
        lock_guard<mutex> lock(mtx);
        --concnt;
        recv_count += recvcnt;
        if (concnt == 0) {
            running = false;
            cnd.notify_one();
        }
    }

    void doSend(frame::aio::ReactorContext& _rctx);
    bool checkRecvData(size_t _sz) const;

protected:
    static void onRecv(frame::aio::ReactorContext& _rctx, size_t _sz);
    static void onSend(frame::aio::ReactorContext& _rctx);

    static void onConnect(frame::aio::ReactorContext& _rctx);

protected:
    enum { BufferCapacity = 1024 * 2 };

    char buf[BufferCapacity];

private:
    uint64_t     recvcnt;
    uint64_t     sendcnt;
    const size_t idx;
    size_t       crt_send_idx;
    size_t       expect_recvcnt;
};

class PlainConnection final : public Connection {
public:
    PlainConnection(const size_t _idx)
        : Connection(_idx)
        , sock(this->proxy())
    {
    }

private:
    void connect(frame::aio::ReactorContext& _rctx, SocketAddressStub const& _rsas) override
    {
        if (sock.connect(_rctx, _rsas, [this](frame::aio::ReactorContext& _rctx) {
                onConnect(_rctx);
            })) {
            onConnect(_rctx);
        }
    }
    void postRecvSome(frame::aio::ReactorContext& _rctx) override
    {
        sock.postRecvSome(_rctx, buf, BufferCapacity, Connection::onRecv);
    }
    void postSendAll(frame::aio::ReactorContext& _rctx, const char* _pbuf, const size_t _sz) override
    {
        sock.postSendAll(_rctx, _pbuf, _sz, Connection::onSend);
    }
    void onConnect(frame::aio::ReactorContext& _rctx)
    {
        if (false) {
            int rcvsz = 1062000;
            int sndsz = 2626560;

            sock.device().recvBufferSize(rcvsz);
            sock.device().sendBufferSize(sndsz);

            rcvsz = sndsz = -1;
            sock.device().recvBufferSize(rcvsz);
            sock.device().sendBufferSize(sndsz);

            solid_log(generic_logger, Error, "recvbufsz = " << rcvsz << " sendbufsz = " << sndsz);
        }
        if (enable_no_delay) {
            sock.device().enableNoDelay();
        }
        Connection::onConnect(_rctx);
    }

private:
    using StreamSocketT = frame::aio::Stream<frame::aio::Socket>;
    StreamSocketT sock;
};

class SecureConnection final : public Connection {
public:
    SecureConnection(const size_t _idx, SecureContextT& _rctx)
        : Connection(_idx)
        , sock(this->proxy(), _rctx)
    {
    }

private:
    void connect(frame::aio::ReactorContext& _rctx, SocketAddressStub const& _rsas) override
    {
        if (sock.connect(_rctx, _rsas, [this](frame::aio::ReactorContext& _rctx) {
                this->onConnect(_rctx);
            })) {
            this->onConnect(_rctx);
        }
    }

    void postRecvSome(frame::aio::ReactorContext& _rctx) override
    {
        sock.postRecvSome(_rctx, buf, BufferCapacity, Connection::onRecv);
    }

    void postSendAll(frame::aio::ReactorContext& _rctx, const char* _pbuf, const size_t _sz) override
    {
        sock.postSendAll(_rctx, _pbuf, _sz, Connection::onSend);
    }

    void onConnect(frame::aio::ReactorContext& _rctx)
    {
        if (!_rctx.error()) {
            solid_dbg(generic_logger, Info, this << " Connected");
            if (enable_no_delay) {
                sock.device().enableNoDelay();
            }
            sock.secureSetVerifyDepth(_rctx, 10);
            sock.secureSetCheckHostName(_rctx, "echo-server");
            sock.secureSetVerifyCallback(_rctx, frame::aio::openssl::VerifyModePeer, onSecureVerify);
            if (sock.secureConnect(_rctx, Connection::onConnect)) {
                Connection::onConnect(_rctx);
            }
        } else {
            solid_dbg(generic_logger, Error, this << " postStop");
            postStop(_rctx);
        }
    }

    static bool onSecureVerify(frame::aio::ReactorContext& _rctx, bool _preverified, frame::aio::openssl::VerifyContext& /*_rverify_ctx*/)
    {
        SecureConnection& rthis = static_cast<SecureConnection&>(_rctx.actor());
        solid_dbg(generic_logger, Info, &rthis << " " << _preverified);
        return _preverified;
    }

private:
    using StreamSocketT = frame::aio::Stream<frame::aio::openssl::Socket>;
    StreamSocketT sock;
};

void prepareSendData();

} //namespace client

namespace relay {
class Listener final : public frame::aio::Actor {
public:
    Listener(
        frame::Service& _rsvc,
        AioSchedulerT&  _rsched,
        SocketDevice&&  _rsd)
        : rsvc(_rsvc)
        , rsch(_rsched)
        , sock(this->proxy(), std::move(_rsd))
    {
    }
    ~Listener() override
    {
    }

private:
    void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent) override;
    void onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd);

    typedef frame::aio::Listener ListenerSocketT;

    frame::Service& rsvc;
    AioSchedulerT&  rsch;
    ListenerSocketT sock;
};

class Connection final : public frame::aio::Actor {
public:
    Connection(SocketDevice&& _rsd)
        : sock1(this->proxy(), std::move(_rsd))
        , sock2(this->proxy())
        , recvcnt(0)
        , sendcnt(0)
        , crtid(-1)
    {
    }
    ~Connection() override {}

protected:
    void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent) override;

    static void onRecvSock1(frame::aio::ReactorContext& _rctx, size_t _sz);
    static void onRecvSock2(frame::aio::ReactorContext& _rctx, size_t _sz);
    static void onSendSock1(frame::aio::ReactorContext& _rctx);
    static void onSendSock2(frame::aio::ReactorContext& _rctx);

    void onConnect(frame::aio::ReactorContext& _rctx);

protected:
    typedef frame::aio::Stream<frame::aio::Socket> StreamSocketT;
    enum { BufferCapacity = 1024 * 4 };

    char          buf1[BufferCapacity];
    char          buf2[BufferCapacity];
    StreamSocketT sock1;
    StreamSocketT sock2;
    uint64_t      recvcnt;
    uint64_t      sendcnt;
    size_t        sendcrt;
    uint32_t      crtid;
};
} //namespace relay

int test_echo_tcp_stress(int argc, char* argv[])
{
    solid::log_start(std::cerr, {"solid::frame::aio.*:EWX", "\\*:VEW", "solid::workpool:EWXS"});

    size_t connection_count = 1;

    if (argc > 1) {
        connection_count = atoi(argv[1]);
        if (connection_count == 0) {
            connection_count = 1;
        }
        if (connection_count > 100) {
            connection_count = 100;
        }
    }

    if (argc > 2) {
        if (*argv[2] == 's' || *argv[2] == 'S') {
            be_secure = true;
        }
        if (*argv[2] == 'r' || *argv[2] == 'R') {
            use_relay = true;
        }
    }

    if (argc > 3) {
        if (*argv[3] == 's' || *argv[3] == 'S') {
            be_secure = true;
        }
        if (*argv[3] == 'r' || *argv[3] == 'R') {
            use_relay = true;
        }
    }

    {
        AioSchedulerT        srv_sch;
        frame::Manager       srv_mgr;
        SecureContextT       srv_secure_ctx{SecureContextT::create()};
        frame::ServiceT      srv_svc{srv_mgr};
        CallPool<void()>     cwp{WorkPoolConfiguration(), 1};
        frame::aio::Resolver resolver(cwp);

        async_resolver(&resolver);

        if (be_secure) {
            ErrorCodeT err;
            err = srv_secure_ctx.loadVerifyFile("echo-ca-cert.pem" /*"/etc/pki/tls/certs/ca-bundle.crt"*/);
            solid_check(!err, "failed loadVerifyFile " << err.message());
            err = srv_secure_ctx.loadCertificateFile("echo-server-cert.pem");
            solid_check(!err, "failed loadCertificateFile " << err.message());
            err = srv_secure_ctx.loadPrivateKeyFile("echo-server-key.pem");
            solid_check(!err, "failed loadPrivateKeyFile " << err.message());
        }

        srv_sch.start(thread::hardware_concurrency());

        {
            ResolveData rd = synchronous_resolve("0.0.0.0", "0", 0, SocketInfo::Inet4, SocketInfo::Stream);

            SocketDevice sd;

            sd.create(rd.begin());
            sd.prepareAccept(rd.begin(), server::Listener::backlog_size());

            if (sd) {
                {
                    SocketAddress local_address;
                    ostringstream oss;

                    sd.localAddress(local_address);

                    int srv_port = local_address.port();
                    oss << srv_port;
                    srv_port_str = oss.str();
                }
                solid::ErrorConditionT err;
                solid::frame::ActorIdT actuid;

                actuid = srv_sch.startActor(
                    make_shared<server::Listener>(srv_svc, srv_sch, std::move(sd), be_secure ? &srv_secure_ctx : nullptr),
                    srv_svc, make_event(GenericEvents::Start), err);
                solid_dbg(generic_logger, Info, "Started Listener actor: " << actuid.index << ',' << actuid.unique);
            } else {
                cout << "Error creating listener socket" << endl;
                running = false;
            }
        }

        AioSchedulerT   rly_sch;
        frame::Manager  rly_mgr;
        frame::ServiceT rly_svc{rly_mgr};

        if (use_relay) {
            rly_sch.start(thread::hardware_concurrency());
            {
                ResolveData rd = synchronous_resolve("0.0.0.0", "0", 0, SocketInfo::Inet4, SocketInfo::Stream);

                SocketDevice sd;

                sd.create(rd.begin());
                sd.prepareAccept(rd.begin(), SocketInfo::max_listen_backlog_size());

                if (sd) {
                    {
                        SocketAddress local_address;
                        ostringstream oss;

                        sd.localAddress(local_address);

                        int srv_port = local_address.port();
                        oss << srv_port;
                        rly_port_str = oss.str();
                    }
                    solid::ErrorConditionT err;
                    solid::frame::ActorIdT actuid;

                    actuid = rly_sch.startActor(make_shared<relay::Listener>(rly_svc, rly_sch, std::move(sd)), rly_svc, make_event(GenericEvents::Start), err);
                    solid_dbg(generic_logger, Info, "Started Listener actor: " << actuid.index << ',' << actuid.unique);
                } else {
                    cout << "Error creating listener socket" << endl;
                    running = false;
                }
            }
        }

        AioSchedulerT   clt_sch;
        frame::Manager  clt_mgr;
        SecureContextT  clt_secure_ctx{SecureContextT::create()};
        frame::ServiceT clt_svc{clt_mgr};

        if (be_secure) {
            ErrorCodeT err = clt_secure_ctx.loadVerifyFile("echo-ca-cert.pem" /*"/etc/pki/tls/certs/ca-bundle.crt"*/);
            solid_check(!err, "failed loadVerifyFile " << err.message());
            err = clt_secure_ctx.loadCertificateFile("echo-client-cert.pem");
            solid_check(!err, "failed loadCertificateFile " << err.message());
            err = clt_secure_ctx.loadPrivateKeyFile("echo-client-key.pem");
            solid_check(!err, "failed loadPrivateKeyFile " << err.message());
        }

        clt_sch.start(thread::hardware_concurrency());
        {
            client::prepareSendData();

            for (size_t i = 0; i < connection_count; ++i) {
                shared_ptr<frame::aio::Actor> actptr;
                solid::ErrorConditionT        err;
                solid::frame::ActorIdT        actuid;

                if (be_secure) {
                    actptr = make_shared<client::SecureConnection>(i, clt_secure_ctx);
                    //actptr.reset(new client::SecureConnection(i, clt_secure_ctx));
                } else {
                    //actptr.reset(new client::PlainConnection(i));
                    actptr = make_shared<client::PlainConnection>(i);
                }
                ++concnt;
                actuid = clt_sch.startActor(std::move(actptr), clt_svc, make_event(GenericEvents::Start), err);
                if (actuid.isInvalid()) {
                    --concnt;
                }
                solid_dbg(generic_logger, Info, "Started Connection Actor: " << actuid.index << ',' << actuid.unique);
            }
        }
        {
            unique_lock<mutex> lock(mtx);

            if (!cnd.wait_for(lock, std::chrono::seconds(wait_seconds), []() { return !running; })) {
                solid_throw("Process is taking too long.");
            }
            cout << "Received " << recv_count / 1024 << "KB on " << connection_count << " connections" << endl;
            solid_check(recv_count != 0);
        }
    }

    return 0;
}

//-----------------------------------------------------------------------------
frame::aio::Resolver& async_resolver(frame::aio::Resolver* _pres /* = nullptr*/)
{
    static frame::aio::Resolver& r = *_pres;
    return r;
}
//-----------------------------------------------------------------------------

namespace server {
//-----------------------------------------------------------------------------
//      Listener
//-----------------------------------------------------------------------------

/*virtual*/ void Listener::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    solid_dbg(generic_logger, Info, "event = " << _revent);
    if (generic_event_start == _revent) {
        sock.postAccept(_rctx, [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd) { onAccept(_rctx, _rsd); });
    } else if (generic_event_kill == _revent) {
        postStop(_rctx);
    }
}

void Listener::onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd)
{
    solid_dbg(generic_logger, Info, "");
    size_t repeatcnt = backlog_size();

    do {
        if (!_rctx.error()) {
            if (false) {
                int rcvsz = 1062000;
                int sndsz = 2626560;

                _rsd.recvBufferSize(rcvsz);
                _rsd.sendBufferSize(sndsz);

                rcvsz = sndsz = -1;
                _rsd.recvBufferSize(rcvsz);
                _rsd.sendBufferSize(sndsz);

                solid_log(generic_logger, Error, "recvbufsz = " << rcvsz << " sendbufsz = " << sndsz);
            }
            if (enable_no_delay) {
                _rsd.enableNoDelay();
            }
            shared_ptr<frame::aio::Actor> actptr;
            solid::ErrorConditionT        err;

            if (psecurectx != nullptr) {
                actptr = make_shared<SecureConnection>(std::move(_rsd), *psecurectx);
            } else {
                actptr = make_shared<PlainConnection>(std::move(_rsd));
            }

            rsch.startActor(std::move(actptr), rsvc, make_event(GenericEvents::Start), err);
        } else {
            //e.g. a limit of open file descriptors was reached - we sleep for 10 seconds
            //timer.waitFor(_rctx, std::chrono::seconds(10), std::bind(&Listener::onEvent, this, _1, frame::Event(EventStartE)));
            break;
        }
        --repeatcnt;
    } while (repeatcnt != 0u && sock.accept(
                 _rctx, [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd) { onAccept(_rctx, _rsd); }, _rsd));

    if (repeatcnt == 0u) {
        sock.postAccept(
            _rctx,
            [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd) { onAccept(_rctx, _rsd); }); //fully asynchronous call
    }
}

//-----------------------------------------------------------------------------
//      Connection
//-----------------------------------------------------------------------------
/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    solid_dbg(generic_logger, Error, this << " event = " << _revent);
    if (generic_event_start == _revent) {
        //postRecvSome(_rctx); //fully asynchronous call
        start(_rctx);
    } else if (generic_event_kill == _revent) {
        solid_dbg(generic_logger, Error, this << " postStop");
        postStop(_rctx);
    }
}

/*static*/ void Connection::onRecv(frame::aio::ReactorContext& _rctx, size_t _sz)
{
    unsigned    repeatcnt = 1;
    Connection& rthis     = static_cast<Connection&>(_rctx.actor());
    solid_dbg(generic_logger, Info, &rthis << " " << _sz);
    do {
        if (!_rctx.error()) {
            solid_dbg(generic_logger, Info, &rthis << " write: " << _sz);
            rthis.recvcnt += _sz;
            rthis.sendcrt = _sz;
            if (rthis.sendAll(_rctx, _sz)) {
                if (_rctx.error()) {
                    solid_dbg(generic_logger, Error, &rthis << " postStop " << rthis.recvcnt << " " << rthis.sendcnt);
                    rthis.postStop(_rctx);
                    break;
                }
                rthis.sendcnt += rthis.sendcrt;
            } else {
                solid_dbg(generic_logger, Info, &rthis << "");
                break;
            }
        } else {
            solid_dbg(generic_logger, Error, &rthis << " postStop " << rthis.recvcnt << " " << rthis.sendcnt);
            rthis.postStop(_rctx);
            break;
        }
        --repeatcnt;
    } while (repeatcnt != 0u && rthis.recvSome(_rctx, _sz));

    solid_dbg(generic_logger, Info, &rthis << " " << repeatcnt);

    if (repeatcnt == 0) {
        rthis.postRecvSome(_rctx); //fully asynchronous call
    }
}

/*static*/ void Connection::onSend(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.actor());
    if (!_rctx.error()) {
        solid_dbg(generic_logger, Info, &rthis << " postRecvSome");
        rthis.sendcnt += rthis.sendcrt;
        rthis.postRecvSome(_rctx); //fully asynchronous call
    } else {
        solid_dbg(generic_logger, Error, &rthis << " postStop " << rthis.recvcnt << " " << rthis.sendcnt);
        rthis.postStop(_rctx);
    }
}
} //namespace server
//-----------------------------------------------------------------------------
namespace client {
const size_t sizes[]{
    100,
    200,
    400,
    800,
    1600,
    2400,
    3200,
    4800,
    6400,
    8800,
    12800,
    24000,
    48000,
    96000,
    192000,
    100,
    200,
    400,
    800,
    1600,
    2400,
    3200,
    4800,
    100,
    200,
    400,
    800,
    1600,
    2400,
    3200,
    100,
    200,
    400,
    800,
    1600,
    2400};
const size_t sizes_size = sizeof(sizes) / sizeof(size_t);

vector<string> send_data_vec;

//-----------------------------------------------------------------------------
void prepareSendData()
{

    string pattern;
    pattern.reserve(256);

    for (int i = 0; i < 256; ++i) {
        if (isgraph(i) != 0) {
            pattern += static_cast<char>(i);
        }
    }

    send_data_vec.resize(sizes_size);
    for (size_t i = 0; i < sizes_size; ++i) {
        auto& s = send_data_vec[i];
        s.reserve(sizes[i]);
        for (size_t j = 0; j < sizes[i]; ++j) {
            s += pattern[(i + j) % pattern.size()];
        }
    }
}

//-----------------------------------------------------------------------------
struct ResolvFunc {
    frame::Manager& rm;
    frame::ActorIdT actuid;

    ResolvFunc(frame::Manager& _rm, frame::ActorIdT const& _ractuid)
        : rm(_rm)
        , actuid(_ractuid)
    {
    }

    void operator()(ResolveData& _rrd, ErrorCodeT const& /*_rerr*/)
    {
        Event ev(make_event(GenericEvents::Message));

        ev.any() = std::move(_rrd);

        solid_dbg(generic_logger, Info, this << " send resolv_message");
        rm.notify(actuid, std::move(ev));
    }
};

void Connection::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    solid_dbg(generic_logger, Info, "event = " << _revent);
    if (_revent == generic_event_start) {
        //we must resolve the address then connect
        solid_dbg(generic_logger, Info, "async_resolve = "
                << "127.0.0.1"
                << " " << srv_port_str);
        async_resolver().requestResolve(
            ResolvFunc(_rctx.service().manager(), _rctx.service().manager().id(*this)), "127.0.0.1",
            use_relay ? rly_port_str.c_str() : srv_port_str.c_str(), 0, SocketInfo::Inet4, SocketInfo::Stream);
    } else if (_revent == generic_event_kill) {
        postStop(_rctx);
    } else if (generic_event_message == _revent) {
        ResolveData* presolvemsg = _revent.any().cast<ResolveData>();
        if (presolvemsg != nullptr) {
            if (presolvemsg->empty()) {
                solid_dbg(generic_logger, Error, this << " postStop");
                //++stats.donecnt;
                postStop(_rctx);
            } else {
                connect(_rctx, presolvemsg->begin());
            }
        }
    }
}

void Connection::doSend(frame::aio::ReactorContext& _rctx)
{
    size_t      sendidx = (crt_send_idx + idx) % send_data_vec.size();
    const auto& str     = send_data_vec[sendidx];
    expect_recvcnt      = str.size();

    solid_dbg(generic_logger, Info, this << " sending " << str.size());

    sendcnt += str.size();

    postSendAll(_rctx, str.data(), str.size());
}

/*static*/ void Connection::onConnect(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.actor());

    if (!_rctx.error()) {
        solid_dbg(generic_logger, Error, &rthis << " SUCCESS");
        rthis.postRecvSome(_rctx);
        rthis.doSend(_rctx);
    } else {
        solid_dbg(generic_logger, Error, &rthis << " postStop " << rthis.recvcnt << " " << _rctx.systemError().message());
        //++stats.donecnt;
        rthis.postStop(_rctx);
    }
}

/*static*/ void Connection::onRecv(frame::aio::ReactorContext& _rctx, size_t _sz)
{
    Connection& rthis = static_cast<Connection&>(_rctx.actor());

    if (!_rctx.error()) {
        rthis.recvcnt += _sz;

        solid_check(_sz <= rthis.expect_recvcnt);
        solid_check(rthis.checkRecvData(_sz));

        solid_dbg(generic_logger, Info, &rthis << " received " << _sz);

        rthis.expect_recvcnt -= _sz;

        if (rthis.expect_recvcnt == 0) {
            ++rthis.crt_send_idx;
            if (rthis.crt_send_idx != repeat_count) {
                rthis.doSend(_rctx);
            } else {
                solid_check(rthis.recvcnt == rthis.sendcnt);
                rthis.postStop(_rctx);
            }
        }
        rthis.postRecvSome(_rctx);
    } else {
        solid_dbg(generic_logger, Error, &rthis << " postStop " << rthis.recvcnt << " " << _rctx.systemError().message() << " " << _rctx.error().message());
        //++stats.donecnt;
        rthis.postStop(_rctx);
    }
}

/*static*/ void Connection::onSend(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.actor());

    if (!_rctx.error()) {
        solid_dbg(generic_logger, Info, &rthis << " " << rthis.recvcnt);
    } else {
        solid_dbg(generic_logger, Error, &rthis << " postStop " << rthis.recvcnt << " " << _rctx.systemError().message());
        //++stats.donecnt;
        rthis.postStop(_rctx);
    }
}

bool Connection::checkRecvData(const size_t _sz) const
{
    size_t      sendidx = (crt_send_idx + idx) % send_data_vec.size();
    const auto& str     = send_data_vec[sendidx];
    const char* pbuf    = str.data() + (str.size() - expect_recvcnt);
    return memcmp(buf, pbuf, _sz) == 0;
}

} //namespace client

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
namespace relay {
//-----------------------------------------------------------------------------
//      Listener
//-----------------------------------------------------------------------------

/*virtual*/ void Listener::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    solid_dbg(generic_logger, Info, "event = " << _revent);
    if (_revent == generic_event_start) {
        sock.postAccept(_rctx, [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd) { onAccept(_rctx, _rsd); });
    } else if (_revent == generic_event_kill) {
        postStop(_rctx);
    }
}

void Listener::onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd)
{
    solid_dbg(generic_logger, Info, "");
    unsigned repeatcnt = SocketInfo::max_listen_backlog_size();

    do {
        if (!_rctx.error()) {
            if (enable_no_delay) {
                _rsd.enableNoDelay();
            }
            solid::ErrorConditionT err;

            rsch.startActor(make_shared<Connection>(std::move(_rsd)), rsvc, make_event(GenericEvents::Start), err);
        } else {
            //e.g. a limit of open file descriptors was reached - we sleep for 10 seconds
            //timer.waitFor(_rctx, NanoTime(10), std::bind(&Listener::onEvent, this, _1, frame::Event(EventStartE)));
            break;
        }
        --repeatcnt;
    } while (repeatcnt != 0u && sock.accept(
                 _rctx, [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd) { onAccept(_rctx, _rsd); }, _rsd));

    if (repeatcnt == 0u) {
        sock.postAccept(
            _rctx,
            [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd) { onAccept(_rctx, _rsd); }); //fully asynchronous call
    }
}

//-----------------------------------------------------------------------------
//      Connection
//-----------------------------------------------------------------------------

struct ResolvFunc {
    frame::Manager& rm;
    frame::ActorIdT actuid;

    ResolvFunc(frame::Manager& _rm, frame::ActorIdT const& _ractuid)
        : rm(_rm)
        , actuid(_ractuid)
    {
    }

    void operator()(ResolveData& _rrd, ErrorCodeT const& /*_rerr*/)
    {
        Event ev(make_event(GenericEvents::Message));

        ev.any() = std::move(_rrd);

        solid_dbg(generic_logger, Info, this << " send resolv_message");
        rm.notify(actuid, std::move(ev));
    }
};

/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    solid_dbg(generic_logger, Error, this << " " << _revent);
    if (generic_event_start == _revent) {
        //we must resolve the address then connect
        solid_dbg(generic_logger, Info, "async_resolve = "
                << "127.0.0.1"
                << " " << srv_port_str);
        async_resolver().requestResolve(
            ResolvFunc(_rctx.manager(), _rctx.manager().id(*this)), "127.0.0.1",
            srv_port_str.c_str(), 0, SocketInfo::Inet4, SocketInfo::Stream);
    } else if (generic_event_kill == _revent) {
        solid_dbg(generic_logger, Error, this << " postStop");
        postStop(_rctx);
    } else if (generic_event_message == _revent) {
        ResolveData* presolvemsg = _revent.any().cast<ResolveData>();
        if (presolvemsg != nullptr) {
            if (presolvemsg->empty()) {
                solid_dbg(generic_logger, Error, this << " postStop");
                //sock.shutdown(_rctx);
                postStop(_rctx);
            } else {
                if (sock2.connect(_rctx, presolvemsg->begin(), [this](frame::aio::ReactorContext& _rctx) { onConnect(_rctx); })) {
                    onConnect(_rctx);
                }
            }
        }
    }
}

void Connection::onConnect(frame::aio::ReactorContext& _rctx)
{
    if (!_rctx.error()) {
        solid_dbg(generic_logger, Info, this << " SUCCESS");
        if (enable_no_delay) {
            sock2.device().enableNoDelay();
        }
        sock1.postRecvSome(_rctx, buf1, BufferCapacity, Connection::onRecvSock1);
        sock2.postRecvSome(_rctx, buf2, BufferCapacity, Connection::onRecvSock2);
    } else {
        solid_dbg(generic_logger, Error, this << " postStop " << recvcnt << " " << sendcnt << " " << _rctx.systemError().message());
        postStop(_rctx);
    }
}

/*static*/ void Connection::onRecvSock1(frame::aio::ReactorContext& _rctx, size_t _sz)
{
    unsigned    repeatcnt = 4;
    Connection& rthis     = static_cast<Connection&>(_rctx.actor());
    solid_dbg(generic_logger, Info, &rthis << " " << _sz);
    do {
        if (!_rctx.error()) {
            bool rv = rthis.sock2.sendAll(_rctx, rthis.buf1, _sz, Connection::onSendSock2);
            if (rv) {
                if (_rctx.error()) {
                    solid_dbg(generic_logger, Error, &rthis << " postStop");
                    rthis.postStop(_rctx);
                    break;
                }
            } else {
                break;
            }
        } else {
            solid_dbg(generic_logger, Error, &rthis << " postStop");
            rthis.postStop(_rctx);
            break;
        }
        --repeatcnt;
    } while (repeatcnt != 0u && rthis.sock1.recvSome(_rctx, rthis.buf1, BufferCapacity, Connection::onRecvSock1, _sz));

    if (repeatcnt == 0) {
        bool rv = rthis.sock1.postRecvSome(_rctx, rthis.buf1, BufferCapacity, Connection::onRecvSock1); //fully asynchronous call
        solid_assert(!rv);
    }
}

/*static*/ void Connection::onRecvSock2(frame::aio::ReactorContext& _rctx, size_t _sz)
{
    unsigned    repeatcnt = 4;
    Connection& rthis     = static_cast<Connection&>(_rctx.actor());
    solid_dbg(generic_logger, Info, &rthis << " " << _sz);
    do {
        if (!_rctx.error()) {
            bool rv = rthis.sock1.sendAll(_rctx, rthis.buf2, _sz, Connection::onSendSock1);
            if (rv) {
                if (_rctx.error()) {
                    solid_dbg(generic_logger, Error, &rthis << " postStop");
                    rthis.postStop(_rctx);
                    break;
                }
            } else {
                break;
            }
        } else {
            solid_dbg(generic_logger, Error, &rthis << " postStop");
            rthis.postStop(_rctx);
            break;
        }
        --repeatcnt;
    } while (repeatcnt != 0u && rthis.sock2.recvSome(_rctx, rthis.buf2, BufferCapacity, Connection::onRecvSock2, _sz));

    if (repeatcnt == 0) {
        bool rv = rthis.sock2.postRecvSome(_rctx, rthis.buf2, BufferCapacity, Connection::onRecvSock2); //fully asynchronous call
        solid_assert(!rv);
    }
}

/*static*/ void Connection::onSendSock1(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.actor());
    if (!_rctx.error()) {
        rthis.sock2.postRecvSome(_rctx, rthis.buf2, BufferCapacity, Connection::onRecvSock2);
    } else {
        solid_dbg(generic_logger, Error, &rthis << " postStop");
        rthis.postStop(_rctx);
    }
}

/*static*/ void Connection::onSendSock2(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.actor());
    if (!_rctx.error()) {
        rthis.sock1.postRecvSome(_rctx, rthis.buf1, BufferCapacity, Connection::onRecvSock1);
    } else {
        solid_dbg(generic_logger, Error, &rthis << " postStop");
        rthis.postStop(_rctx);
    }
}

} //namespace relay
//-----------------------------------------------------------------------------
