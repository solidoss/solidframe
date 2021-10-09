#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioactor.hpp"
#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/system/log.hpp"
#include "solid/system/socketaddress.hpp"
#include "solid/system/socketdevice.hpp"

#include "solid/utility/event.hpp"

#include "cxxopts.hpp"

#include <functional>
#include <iostream>
#include <signal.h>

using namespace std;
using namespace solid;
using namespace std::placeholders;

typedef frame::Scheduler<frame::aio::Reactor> AioSchedulerT;

//------------------------------------------------------------------
//------------------------------------------------------------------

struct Params {
    int            listener_port;
    int            talker_port;
    int            connect_port;
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
//------------------------------------------------------------------
//------------------------------------------------------------------

class Listener : public frame::aio::Actor {
public:
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
        , ptimer(nullptr)
        , timercnt(0)
    {
    }
    ~Listener()
    {
        delete ptimer;
    }

private:
    void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent) override;
    void onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd);

    void onTimer(frame::aio::ReactorContext& _rctx);

    typedef frame::aio::Listener    ListenerSocketT;
    typedef frame::aio::SteadyTimer TimerT;

    frame::Service& rsvc;
    AioSchedulerT&  rsch;
    ListenerSocketT sock;
    TimerT*         ptimer;
    size_t          timercnt;
};

#define USE_CONNECTION

#ifdef USE_CONNECTION

#include "solid/frame/aio/aiosocket.hpp"
#include "solid/frame/aio/aiostream.hpp"

class Connection : public frame::aio::Actor {
protected:
    Connection()
        : sock(this->proxy())
        , recvcnt(0)
        , sendcnt(0)
    {
    }

public:
    Connection(SocketDevice&& _rsd)
        : sock(this->proxy(), std::move(_rsd))
        , recvcnt(0)
        , sendcnt(0)
    {
    }
    ~Connection() {}

protected:
    /*virtual*/ void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent);
    static void      onRecv(frame::aio::ReactorContext& _rctx, size_t _sz);
    static void      onSend(frame::aio::ReactorContext& _rctx);
    void             onTimer(frame::aio::ReactorContext& _rctx);

protected:
    typedef frame::aio::Stream<frame::aio::Socket> StreamSocketT;
    //typedef frame::aio::Timer                         TimerT;
    enum { BufferCapacity = 1024 * 2 };

    char          buf[BufferCapacity];
    StreamSocketT sock;
    uint64_t      recvcnt;
    uint64_t      sendcnt;
    size_t        sendcrt;
    //TimerT            timer;
};

class ClientConnection : public Connection {
    ResolveData rd;

private:
    /*virtual*/ void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent);
    void             onConnect(frame::aio::ReactorContext& _rctx);

public:
    ClientConnection(ResolveData const& _rd)
        : rd(_rd)
    {
    }
};
#endif

#define USE_TALKER
#ifdef USE_TALKER

#include "solid/frame/aio/aiodatagram.hpp"
#include "solid/frame/aio/aiosocket.hpp"

class Talker : public frame::aio::Actor {
public:
    Talker(SocketDevice&& _rsd)
        : sock(this->proxy(), std::move(_rsd))
    {
    }
    ~Talker() {}

private:
    /*virtual*/ void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent);
    void             onRecv(frame::aio::ReactorContext& _rctx, SocketAddress& _raddr, size_t _sz);
    void             onSend(frame::aio::ReactorContext& _rctx);

private:
    typedef frame::aio::Datagram<frame::aio::Socket> DatagramSocketT;

    enum { BufferCapacity = 1024 * 2 };

    char            buf[BufferCapacity];
    DatagramSocketT sock;
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
    {

        AioSchedulerT sch;

        frame::Manager  m;
        frame::ServiceT svc(m);

        sch.start(thread::hardware_concurrency());

        {
            ResolveData rd = synchronous_resolve("0.0.0.0", params.listener_port, 0, SocketInfo::Inet4, SocketInfo::Stream);

            SocketDevice sd;

            sd.create(rd.begin());
            sd.prepareAccept(rd.begin(), Listener::backlog_size());

            if (sd) {
                solid::ErrorConditionT err;
                solid::frame::ActorIdT actuid;

                actuid = sch.startActor(make_shared<Listener>(svc, sch, std::move(sd)), svc, make_event(GenericEvents::Start), err);
                solid_log(generic_logger, Info, "Started Listener actor: " << actuid.index << ',' << actuid.unique);
            } else {
                cout << "Error creating listener socket" << endl;
                running = false;
            }
#ifdef USE_CONNECTION
            {
                rd = synchronous_resolve("127.0.0.1", params.connect_port, 0, SocketInfo::Inet4, SocketInfo::Stream);

                solid::ErrorConditionT err;
                solid::frame::ActorIdT actuid;

                actuid = sch.startActor(make_shared<ClientConnection>(rd), svc, make_event(GenericEvents::Start), err);

                solid_log(generic_logger, Info, "Started Client Connection actor: " << actuid.index << ',' << actuid.unique);
            }
#endif
#ifdef USE_TALKER
            rd = synchronous_resolve("0.0.0.0", params.talker_port, 0, SocketInfo::Inet4, SocketInfo::Datagram);

            sd.create(rd.begin());
            sd.bind(rd.begin());

            if (sd) {
                solid::ErrorConditionT err;
                solid::frame::ActorIdT actuid;

                actuid = sch.startActor(make_shared<Talker>(std::move(sd)), svc, make_event(GenericEvents::Start), err);

                solid_log(generic_logger, Info, "Started Talker actor: " << actuid.index << ',' << actuid.unique);
            } else {
                cout << "Error creating talker socket" << endl;
                running = false;
            }
#endif
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
    using namespace cxxopts;
    try {
        Options options{argv[0], "SolidFrame concept application"};
        options.add_options()("h,help", "List program options")("l,listen-port", "Listener port", value<int>(_par.listener_port)->default_value("2000"))("t,talk-port", "Talker port", value<int>(_par.talker_port)->default_value("3000"))("c,connection-port", "Connection port", value<int>(_par.connect_port)->default_value("5000"))("M,debug-modules", "Debug logging modules", value<vector<string>>(_par.dbg_modules))("A,debug-address", "Debug server address (e.g. on linux use: nc -l 2222)", value<string>(_par.dbg_addr))("P,debug-port", "Debug server port (e.g. on linux use: nc -l 2222)", value<string>(_par.dbg_port))("C,debug-console", "Debug console", value<bool>(_par.dbg_console)->implicit_value("true")->default_value("false"))("S,debug-unbuffered", "Debug unbuffered", value<bool>(_par.dbg_buffered)->implicit_value("false")->default_value("true"));
        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help({""}) << std::endl;
            return true;
        }
        return false;
    } catch (std::exception& e) {
        cout << e.what() << "\n";
        return true;
    }
}
//-----------------------------------------------------------------------------
//      Listener
//-----------------------------------------------------------------------------

/*virtual*/ void Listener::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    solid_log(generic_logger, Info, "event = " << _revent);
    if (generic_event_start == _revent) {
        sock.postAccept(_rctx, std::bind(&Listener::onAccept, this, _1, _2));
        //sock.postAccept(_rctx, [this](frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){return onAccept(_rctx, _rsd);});
        ptimer = new TimerT(this->proxy());
        ptimer->waitUntil(_rctx, _rctx.steadyTime() + std::chrono::seconds(2), [this](frame::aio::ReactorContext& _rctx) { return onTimer(_rctx); });
    } else if (generic_event_kill == _revent) {
        postStop(_rctx);
    }
}

void Listener::onTimer(frame::aio::ReactorContext& _rctx)
{
    solid_log(generic_logger, Info, "On Listener Timer");
    ptimer->waitUntil(_rctx, _rctx.steadyTime() + std::chrono::seconds(2), [this](frame::aio::ReactorContext& _rctx) { return onTimer(_rctx); });
    ++timercnt;
    if (timercnt == 4) {
        delete ptimer;
        ptimer = nullptr;
    }
}

void Listener::onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd)
{
    solid_log(generic_logger, Info, "");
    size_t repeatcnt = backlog_size();

    do {
        if (!_rctx.error()) {
#ifdef USE_CONNECTION
            _rsd.enableNoDelay();
            solid::ErrorConditionT err;

            rsch.startActor(make_shared<Connection>(std::move(_rsd)), rsvc, make_event(GenericEvents::Start), err);
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
    solid_log(generic_logger, Error, this << " event = " << _revent);
    if (generic_event_start == _revent) {
        sock.postRecvSome(_rctx, buf, BufferCapacity, Connection::onRecv); //fully asynchronous call
    } else if (generic_event_kill == _revent) {
        solid_log(generic_logger, Error, this << " postStop");
        sock.shutdown(_rctx);
        postStop(_rctx);
    }
}

/*static*/ void Connection::onRecv(frame::aio::ReactorContext& _rctx, size_t _sz)
{
    unsigned    repeatcnt = 2;
    Connection& rthis     = static_cast<Connection&>(_rctx.actor());
    solid_log(generic_logger, Info, &rthis << " " << _sz);
    do {
        if (!_rctx.error()) {
            solid_log(generic_logger, Info, &rthis << " write: " << _sz);
            rthis.recvcnt += _sz;
            rthis.sendcrt = _sz;
            if (rthis.sock.sendAll(_rctx, rthis.buf, _sz, Connection::onSend)) {
                if (_rctx.error()) {
                    solid_log(generic_logger, Error, &rthis << " postStop " << rthis.recvcnt << " " << rthis.sendcnt);
                    rthis.postStop(_rctx);
                    break;
                }
                rthis.sendcnt += rthis.sendcrt;
            } else {
                solid_log(generic_logger, Info, &rthis << "");
                break;
            }
        } else {
            solid_log(generic_logger, Error, &rthis << " postStop " << rthis.recvcnt << " " << rthis.sendcnt);
            rthis.postStop(_rctx);
            break;
        }
        --repeatcnt;
    } while (repeatcnt && rthis.sock.recvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv, _sz));

    solid_log(generic_logger, Info, &rthis << " " << repeatcnt);

    if (repeatcnt == 0) {
        bool rv = rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv); //fully asynchronous call
        solid_assert(!rv);
    }
}

/*static*/ void Connection::onSend(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.actor());
    if (!_rctx.error()) {
        solid_log(generic_logger, Info, &rthis << " postRecvSome");
        rthis.sendcnt += rthis.sendcrt;
        rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv); //fully asynchronous call
    } else {
        solid_log(generic_logger, Error, &rthis << " postStop " << rthis.recvcnt << " " << rthis.sendcnt);
        rthis.postStop(_rctx);
    }
}

void Connection::onTimer(frame::aio::ReactorContext& _rctx)
{
}

//-----------------------------------------------------------------------------

/*virtual*/ void ClientConnection::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    solid_log(generic_logger, Error, this << " event = " << _revent);
    if (generic_event_start == _revent) {

        string hoststr;
        string servstr;

        synchronous_resolve(
            hoststr,
            servstr,
            rd.begin(),
            ReverseResolveInfo::Numeric);

        solid_log(generic_logger, Info, "Connect to " << hoststr << ":" << servstr);

        if (sock.connect(_rctx, rd.begin(), std::bind(&ClientConnection::onConnect, this, _1))) {
            onConnect(_rctx);
        }
    } else if (generic_event_kill == _revent) {
        solid_log(generic_logger, Error, this << " postStop");
        postStop(_rctx);
    }
}
void ClientConnection::onConnect(frame::aio::ReactorContext& _rctx)
{
    if (!_rctx.error()) {
        solid_log(generic_logger, Info, this << " SUCCESS");
        sock.postRecvSome(_rctx, buf, BufferCapacity, Connection::onRecv); //fully asynchronous call
    } else {
        solid_log(generic_logger, Error, this << " postStop " << recvcnt << " " << sendcnt << " " << _rctx.systemError().message());
        postStop(_rctx);
    }
}

#endif

//-----------------------------------------------------------------------------
//      Talker
//-----------------------------------------------------------------------------
#ifdef USE_TALKER
/*virtual*/ void Talker::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    solid_log(generic_logger, Info, this << " event = " << _revent);
    if (generic_event_start == _revent) {
        sock.postRecvFrom(_rctx, buf, BufferCapacity, std::bind(&Talker::onRecv, this, _1, _2, _3)); //fully asynchronous call
    } else if (generic_event_kill == _revent) {
        solid_log(generic_logger, Error, this << " postStop");
        postStop(_rctx);
    }
}

void Talker::onRecv(frame::aio::ReactorContext& _rctx, SocketAddress& _raddr, size_t _sz)
{
    unsigned repeatcnt = 10;
    do {
        if (!_rctx.error()) {
            if (sock.sendTo(_rctx, buf, _sz, _raddr, std::bind(&Talker::onSend, this, _1))) {
                if (_rctx.error()) {
                    solid_log(generic_logger, Error, this << " postStop");
                    postStop(_rctx);
                    break;
                }
            } else {
                break;
            }
        } else {
            solid_log(generic_logger, Error, this << " postStop");
            postStop(_rctx);
            break;
        }
        --repeatcnt;
    } while (repeatcnt && sock.recvFrom(_rctx, buf, BufferCapacity, std::bind(&Talker::onRecv, this, _1, _2, _3), _raddr, _sz));

    solid_log(generic_logger, Info, repeatcnt);
    if (repeatcnt == 0) {
        bool rv = sock.postRecvFrom(_rctx, buf, BufferCapacity, std::bind(&Talker::onRecv, this, _1, _2, _3)); //fully asynchronous call
        solid_assert(!rv);
    }
}

void Talker::onSend(frame::aio::ReactorContext& _rctx)
{
    if (!_rctx.error()) {
        sock.postRecvFrom(_rctx, buf, BufferCapacity, std::bind(&Talker::onRecv, this, _1, _2, _3)); //fully asynchronous call
    } else {
        postStop(_rctx);
    }
}

#endif
