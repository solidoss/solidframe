/*

$ openssl genrsa 2048 > ca-key.pem
$ openssl req -new -x509 -nodes -days 1000 -key ca-key.pem > ca-cert.pem
$ openssl req -newkey rsa:2048 -days 1000 -nodes -keyout server-key.pem > server-req.pem
$ openssl x509 -req -in server-req.pem -days 1000 -CA ca-cert.pem -CAkey ca-key.pem -set_serial 01 > server-cert.pem

*/

#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aiotimer.hpp"
#include "solid/frame/aio/openssl/aiosecurecontext.hpp"
#include "solid/frame/aio/openssl/aiosecuresocket.hpp"

#include "solid/system/log.hpp"
#include "solid/system/socketaddress.hpp"
#include "solid/system/socketdevice.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/utility/event.hpp"

#include "boost/program_options.hpp"

#include <functional>
#include <iostream>
#include <signal.h>

using namespace std;
using namespace solid;
using namespace std::placeholders;

typedef frame::Scheduler<frame::aio::Reactor> AioSchedulerT;
typedef frame::aio::openssl::Context          SecureContextT;

//------------------------------------------------------------------
//------------------------------------------------------------------

struct Params {
    int            listener_port;
    vector<string> dbg_modules;
    string         dbg_addr;
    string         dbg_port;
    bool           dbg_console;
    bool           dbg_buffered;
    bool           log;
};

namespace {
mutex              mtx;
condition_variable cnd;
bool               running(true);
} // namespace

static void term_handler(int signum)
{
    switch (signum) {
    case SIGINT:
    case SIGTERM: {
        if (running) {
            lock_guard<mutex> lock(mtx);
            running = false;
            cnd.notify_all();
        }
    }
    }
}

SecureContextT secure_ctx(SecureContextT::create());

//------------------------------------------------------------------
//------------------------------------------------------------------

class Listener : public Dynamic<Listener, frame::aio::Object> {
public:
    // We will use the this backlog_size
    // both as parameter to listen and as max
    // accept loop count - this way we make sure
    // that the accetor socket backlog queue is
    // emptied a.s.a.p.
    static size_t backlog_size()
    {
        return SocketInfo::max_listen_backlog_size();
    }

    Listener(
        frame::Service& _rsvc,
        AioSchedulerT&  _rsched,
        SocketDevice&&  _rsd)
        : rsvc(_rsvc)
        , rsch(_rsched)
        , sock(this->proxy(), std::move(_rsd))
    {
    }
    ~Listener()
    {
    }

private:
    /*virtual*/ void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent);
    void             onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd);

    typedef frame::aio::Listener    ListenerSocketT;
    typedef frame::aio::SteadyTimer TimerT;

    frame::Service& rsvc;
    AioSchedulerT&  rsch;
    ListenerSocketT sock;
};

#define USE_CONNECTION

#ifdef USE_CONNECTION

#include "solid/frame/aio/aiosocket.hpp"
#include "solid/frame/aio/aiostream.hpp"

class Connection : public Dynamic<Connection, frame::aio::Object> {
public:
    Connection(SocketDevice&& _rsd, SecureContextT& _rctx)
        : sock(this->proxy(), std::move(_rsd), _rctx)
        , recvcnt(0)
        , sendcnt(0)
    {
    }
    ~Connection() {}

protected:
    /*virtual*/ void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent);
    static void      onRecv(frame::aio::ReactorContext& _rctx, size_t _sz);
    static void      onSend(frame::aio::ReactorContext& _rctx);
    static void      onSecureAccept(frame::aio::ReactorContext& _rctx);
    static bool      onSecureVerify(frame::aio::ReactorContext& _rctx, bool _preverified, frame::aio::openssl::VerifyContext& _rverify_ctx);

protected:
    typedef frame::aio::Stream<frame::aio::openssl::Socket> StreamSocketT;
    //typedef frame::aio::Timer                                 TimerT;
    enum { BufferCapacity = 1024 * 2 };

    char          buf[BufferCapacity];
    StreamSocketT sock;
    uint64_t      recvcnt;
    uint64_t      sendcnt;
    size_t        sendcrt;
};

#endif

bool parseArguments(Params& _par, int argc, char* argv[]);

int main(int argc, char* argv[])
{
    Params params;
    if (parseArguments(params, argc, argv))
        return 0;

#ifndef SOLID_ON_WINDOWS
    signal(SIGINT, term_handler); /* Die on SIGTERM */
    signal(SIGPIPE, SIG_IGN);
#endif

    if (params.dbg_addr.size() && params.dbg_port.size()) {
        solid::log_start(
            params.dbg_addr.c_str(),
            params.dbg_port.c_str(),
            params.dbg_modules,
            params.dbg_buffered);

    } else if (params.dbg_console) {
        solid::log_start(std::cerr, params.dbg_modules);
    } else {
        solid::log_start(
            *argv[0] == '.' ? argv[0] + 2 : argv[0],
            params.dbg_modules,
            params.dbg_buffered,
            3,
            1024 * 1024 * 64);
    }

    secure_ctx.loadVerifyFile("echo-ca-cert.pem" /*"/etc/pki/tls/certs/ca-bundle.crt"*/);
    secure_ctx.loadCertificateFile("echo-server-cert.pem");
    secure_ctx.loadPrivateKeyFile("echo-server-key.pem");

    {

        AioSchedulerT sch;

        frame::Manager  m;
        frame::ServiceT svc(m);

        if (sch.start(thread::hardware_concurrency())) {
            running = false;
            cout << "Error starting scheduler" << endl;
        } else {
            ResolveData rd = synchronous_resolve("0.0.0.0", params.listener_port, 0, SocketInfo::Inet4, SocketInfo::Stream);

            SocketDevice sd;

            sd.create(rd.begin());
            sd.prepareAccept(rd.begin(), Listener::backlog_size());

            if (sd) {
                DynamicPointer<frame::aio::Object> objptr(new Listener(svc, sch, std::move(sd)));
                solid::ErrorConditionT             err;
                solid::frame::ObjectIdT            objuid;

                objuid = sch.startObject(objptr, svc, make_event(GenericEvents::Start), err);
                solid_log(basic_logger, Info, "Started Listener object: " << objuid.index << ',' << objuid.unique);
            } else {
                cout << "Error creating listener socket" << endl;
                running = false;
            }
        }

        if (0) {
            unique_lock<mutex> lock(mtx);
            while (running) {
                cnd.wait(lock);
            }
        } else {
            cin.ignore();
        }

        //m.stop();
    }
    return 0;
}

//-----------------------------------------------------------------------------
bool parseArguments(Params& _par, int argc, char* argv[])
{
    using namespace boost::program_options;
    try {
        options_description desc("SolidFrame concept application");
        desc.add_options()("help,h", "List program options")("listen-port,l", value<int>(&_par.listener_port)->default_value(2000), "Listener port")("debug-modules,M", value<vector<string>>(&_par.dbg_modules), "Debug logging modules")("debug-address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")("debug-port,P", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")("debug-console,C", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")("debug-unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered");
        variables_map vm;
        store(parse_command_line(argc, argv, desc), vm);
        notify(vm);
        if (vm.count("help")) {
            cout << desc << "\n";
            return true;
        }
        return false;
    } catch (exception& e) {
        cout << e.what() << "\n";
        return true;
    }
}
//-----------------------------------------------------------------------------
//      Listener
//-----------------------------------------------------------------------------

/*virtual*/ void Listener::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    solid_log(basic_logger, Info, "event = " << _revent);
    if (generic_event_start == _revent) {
        sock.postAccept(_rctx, std::bind(&Listener::onAccept, this, _1, _2));
        //sock.postAccept(_rctx, [this](frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){return onAccept(_rctx, _rsd);});
    } else if (generic_event_kill == _revent) {
        postStop(_rctx);
    }
}

void Listener::onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd)
{
    solid_log(basic_logger, Info, "");
    unsigned repeatcnt = backlog_size();

    do {
        if (!_rctx.error()) {
#ifdef USE_CONNECTION
            int sz = 1024 * 64;
            _rsd.recvBufferSize(sz);
            sz = 10224 * 32;
            _rsd.sendBufferSize(sz);
            solid_log(basic_logger, Error, "new_connection");
            DynamicPointer<frame::aio::Object> objptr(new Connection(std::move(_rsd), secure_ctx));
            solid::ErrorConditionT             err;

            rsch.startObject(objptr, rsvc, make_event(GenericEvents::Start), err);

#else
            cout << "Accepted connection: " << _rsd.descriptor() << endl;
#endif
        } else {
            //e.g. a limit of open file descriptors was reached - we sleep for 10 seconds
            //timer.waitFor(_rctx, std::chrono::seconds(10), std::bind(&Listener::onEvent, this, _1, frame::Event(EventStartE)));
            break;
        }
        --repeatcnt;
    } while (repeatcnt && sock.accept(_rctx, std::bind(&Listener::onAccept, this, _1, _2), _rsd));

    if (!repeatcnt) {
        sock.postAccept(
            _rctx,
            [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd) { onAccept(_rctx, _rsd); }); //fully asynchronous call
    }
}

//-----------------------------------------------------------------------------
//      Connection
//-----------------------------------------------------------------------------
#ifdef USE_CONNECTION
/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    solid_log(basic_logger, Error, this << " " << _revent);
    if (generic_event_start == _revent) {
        sock.secureSetCheckHostName(_rctx, "echo-client");
        sock.secureSetVerifyCallback(_rctx, frame::aio::openssl::VerifyModePeer, onSecureVerify);
        if (sock.secureAccept(_rctx, Connection::onSecureAccept)) {
            if (!_rctx.error()) {
                sock.postRecvSome(_rctx, buf, BufferCapacity, Connection::onRecv); //fully asynchronous call
            } else {
                solid_log(basic_logger, Error, this << " postStop");
                postStop(_rctx);
            }
        }
    } else if (generic_event_kill == _revent) {
        solid_log(basic_logger, Error, this << " postStop");
        sock.shutdown(_rctx);
        postStop(_rctx);
    }
}

/*static*/ void Connection::onRecv(frame::aio::ReactorContext& _rctx, size_t _sz)
{
    unsigned    repeatcnt = 2;
    Connection& rthis     = static_cast<Connection&>(_rctx.object());
    solid_log(basic_logger, Info, &rthis << " " << _sz);
    do {
        if (!_rctx.error()) {
            solid_log(basic_logger, Info, &rthis << " write: " << _sz);
            rthis.recvcnt += _sz;
            rthis.sendcrt = _sz;
            if (rthis.sock.sendAll(_rctx, rthis.buf, _sz, Connection::onSend)) {
                if (_rctx.error()) {
                    solid_log(basic_logger, Error, &rthis << " postStop " << rthis.recvcnt << " " << rthis.sendcnt);
                    rthis.postStop(_rctx);
                    break;
                }
                rthis.sendcnt += rthis.sendcrt;
            } else {
                solid_log(basic_logger, Info, &rthis << "");
                break;
            }
        } else {
            solid_log(basic_logger, Error, &rthis << " postStop " << rthis.recvcnt << " " << rthis.sendcnt);
            rthis.postStop(_rctx);
            break;
        }
        --repeatcnt;
    } while (repeatcnt && rthis.sock.recvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv, _sz));

    solid_log(basic_logger, Info, &rthis << " " << repeatcnt);
    //timer.waitFor(_rctx, NanoTime(30), std::bind(&Connection::onTimer, this, _1));
    if (repeatcnt == 0) {
        bool rv = rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv); //fully asynchronous call
        SOLID_ASSERT(!rv);
    }
}

/*static*/ void Connection::onSend(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());
    if (!_rctx.error()) {
        solid_log(basic_logger, Info, &rthis << " postRecvSome");
        rthis.sendcnt += rthis.sendcrt;
        rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv); //fully asynchronous call
    } else {
        solid_log(basic_logger, Error, &rthis << " postStop " << rthis.recvcnt << " " << rthis.sendcnt);
        rthis.postStop(_rctx);
    }
}

/*static*/ void Connection::onSecureAccept(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());
    if (!_rctx.error()) {
        solid_log(basic_logger, Info, &rthis << " postRecvSome");
        rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv); //fully asynchronous call
    } else {
        solid_log(basic_logger, Error, &rthis << " postStop " << rthis.recvcnt << " " << rthis.sendcnt << " error " << _rctx.systemError().message());
        rthis.postStop(_rctx);
    }
}

/*static*/ bool Connection::onSecureVerify(frame::aio::ReactorContext& _rctx, bool _preverified, frame::aio::openssl::VerifyContext& _rverify_ctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());
    solid_log(basic_logger, Info, &rthis << " " << _preverified);
    return _preverified;
}

#endif
