#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/system/debug.hpp"
#include "solid/system/socketaddress.hpp"
#include "solid/system/socketdevice.hpp"

#include "solid/utility/event.hpp"

#include "boost/program_options.hpp"

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
    int    listener_port;
    int    talker_port;
    int    connect_port;
    string dbg_levels;
    string dbg_modules;
    string dbg_addr;
    string dbg_port;
    bool   dbg_console;
    bool   dbg_buffered;
    bool   log;
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
            unique_lock<mutex> lock(mtx);
            running = false;
            cnd.notify_all();
        }
    }
    }
}
//------------------------------------------------------------------
//------------------------------------------------------------------

class Listener : public Dynamic<Listener, frame::aio::Object> {
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

class Connection : public Dynamic<Connection, frame::aio::Object> {
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

class ClientConnection : public Dynamic<Connection, Connection> {
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

class Talker : public Dynamic<Talker, frame::aio::Object> {
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
    Params p;
    if (parseArguments(p, argc, argv))
        return 0;
#ifndef SOLID_ON_WINDOWS
    signal(SIGINT, term_handler); /* Die on SIGTERM */
    signal(SIGPIPE, SIG_IGN);
#endif

#ifdef SOLID_HAS_DEBUG
    {
        string dbgout;
        Debug::the().levelMask(p.dbg_levels.c_str());
        Debug::the().moduleMask(p.dbg_modules.c_str());
        if (p.dbg_addr.size() && p.dbg_port.size()) {
            Debug::the().initSocket(
                p.dbg_addr.c_str(),
                p.dbg_port.c_str(),
                p.dbg_buffered,
                &dbgout);
        } else if (p.dbg_console) {
            Debug::the().initStdErr(
                p.dbg_buffered,
                &dbgout);
        } else {
            Debug::the().initFile(
                *argv[0] == '.' ? argv[0] + 2 : argv[0],
                p.dbg_buffered,
                3,
                1024 * 1024 * 64,
                &dbgout);
        }
        cout << "Debug output: " << dbgout << endl;
        dbgout.clear();
        Debug::the().moduleNames(dbgout);
        cout << "Debug modules: " << dbgout << endl;
    }
#endif
    {

        AioSchedulerT sch;

        frame::Manager  m;
        frame::ServiceT svc(m);

        if (sch.start(thread::hardware_concurrency())) {
            running = false;
            cout << "Error starting scheduler" << endl;
        } else {
            ResolveData rd = synchronous_resolve("0.0.0.0", p.listener_port, 0, SocketInfo::Inet4, SocketInfo::Stream);

            SocketDevice sd;

            sd.create(rd.begin());
            sd.prepareAccept(rd.begin(), Listener::backlog_size());

            if (sd) {
                DynamicPointer<frame::aio::Object> objptr(new Listener(svc, sch, std::move(sd)));
                solid::ErrorConditionT             err;
                solid::frame::ObjectIdT            objuid;

                objuid = sch.startObject(objptr, svc, make_event(GenericEvents::Start), err);
                idbg("Started Listener object: " << objuid.index << ',' << objuid.unique);
            } else {
                cout << "Error creating listener socket" << endl;
                running = false;
            }
#ifdef USE_CONNECTION
            {
                rd = synchronous_resolve("127.0.0.1", p.connect_port, 0, SocketInfo::Inet4, SocketInfo::Stream);

                DynamicPointer<frame::aio::Object> objptr(new ClientConnection(rd));
                solid::ErrorConditionT             err;
                solid::frame::ObjectIdT            objuid;

                objuid = sch.startObject(objptr, svc, make_event(GenericEvents::Start), err);

                idbg("Started Client Connection object: " << objuid.index << ',' << objuid.unique);
            }
#endif
#ifdef USE_TALKER
            rd = synchronous_resolve("0.0.0.0", p.talker_port, 0, SocketInfo::Inet4, SocketInfo::Datagram);

            sd.create(rd.begin());
            sd.bind(rd.begin());

            if (sd) {
                DynamicPointer<frame::aio::Object> objptr(new Talker(std::move(sd)));

                solid::ErrorConditionT  err;
                solid::frame::ObjectIdT objuid;

                objuid = sch.startObject(objptr, svc, make_event(GenericEvents::Start), err);

                idbg("Started Talker object: " << objuid.index << ',' << objuid.unique);
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
    using namespace boost::program_options;
    try {
        options_description desc("SolidFrame concept application");
        desc.add_options()("help,h", "List program options")("listen-port,l", value<int>(&_par.listener_port)->default_value(2000), "Listener port")("talk-port,t", value<int>(&_par.talker_port)->default_value(3000), "Talker port")("connection-port,c", value<int>(&_par.connect_port)->default_value(5000), "Connection port")("debug-levels,L", value<string>(&_par.dbg_levels)->default_value("view"), "Debug logging levels")("debug-modules,M", value<string>(&_par.dbg_modules), "Debug logging modules")("debug-address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")("debug-port,P", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")("debug-console,C", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")("debug-unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered");
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
    idbg("event = " << _revent);
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
    idbg("On Listener Timer");
    ptimer->waitUntil(_rctx, _rctx.steadyTime() + std::chrono::seconds(2), [this](frame::aio::ReactorContext& _rctx) { return onTimer(_rctx); });
    ++timercnt;
    if (timercnt == 4) {
        delete ptimer;
        ptimer = nullptr;
    }
}

void Listener::onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd)
{
    idbg("");
    size_t repeatcnt = backlog_size();

    do {
        if (!_rctx.error()) {
#ifdef USE_CONNECTION
            _rsd.enableNoDelay();
            DynamicPointer<frame::aio::Object> objptr(new Connection(std::move(_rsd)));
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
    edbg(this << " event = " << _revent);
    if (generic_event_start == _revent) {
        sock.postRecvSome(_rctx, buf, BufferCapacity, Connection::onRecv); //fully asynchronous call
    } else if (generic_event_kill == _revent) {
        edbg(this << " postStop");
        sock.shutdown(_rctx);
        postStop(_rctx);
    }
}

/*static*/ void Connection::onRecv(frame::aio::ReactorContext& _rctx, size_t _sz)
{
    unsigned    repeatcnt = 2;
    Connection& rthis     = static_cast<Connection&>(_rctx.object());
    idbg(&rthis << " " << _sz);
    do {
        if (!_rctx.error()) {
            idbg(&rthis << " write: " << _sz);
            rthis.recvcnt += _sz;
            rthis.sendcrt = _sz;
            if (rthis.sock.sendAll(_rctx, rthis.buf, _sz, Connection::onSend)) {
                if (_rctx.error()) {
                    edbg(&rthis << " postStop " << rthis.recvcnt << " " << rthis.sendcnt);
                    rthis.postStop(_rctx);
                    break;
                }
                rthis.sendcnt += rthis.sendcrt;
            } else {
                idbg(&rthis << "");
                break;
            }
        } else {
            edbg(&rthis << " postStop " << rthis.recvcnt << " " << rthis.sendcnt);
            rthis.postStop(_rctx);
            break;
        }
        --repeatcnt;
    } while (repeatcnt && rthis.sock.recvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv, _sz));

    idbg(&rthis << " " << repeatcnt);

    if (repeatcnt == 0) {
        bool rv = rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv); //fully asynchronous call
        SOLID_ASSERT(!rv);
    }
}

/*static*/ void Connection::onSend(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());
    if (!_rctx.error()) {
        idbg(&rthis << " postRecvSome");
        rthis.sendcnt += rthis.sendcrt;
        rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv); //fully asynchronous call
    } else {
        edbg(&rthis << " postStop " << rthis.recvcnt << " " << rthis.sendcnt);
        rthis.postStop(_rctx);
    }
}

void Connection::onTimer(frame::aio::ReactorContext& _rctx)
{
}

//-----------------------------------------------------------------------------

/*virtual*/ void ClientConnection::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    edbg(this << " event = " << _revent);
    if (generic_event_start == _revent) {

        string hoststr;
        string servstr;

        synchronous_resolve(
            hoststr,
            servstr,
            rd.begin(),
            ReverseResolveInfo::Numeric);

        idbg("Connect to " << hoststr << ":" << servstr);

        if (sock.connect(_rctx, rd.begin(), std::bind(&ClientConnection::onConnect, this, _1))) {
            onConnect(_rctx);
        }
    } else if (generic_event_kill == _revent) {
        edbg(this << " postStop");
        postStop(_rctx);
    }
}
void ClientConnection::onConnect(frame::aio::ReactorContext& _rctx)
{
    if (!_rctx.error()) {
        idbg(this << " SUCCESS");
        sock.postRecvSome(_rctx, buf, BufferCapacity, Connection::onRecv); //fully asynchronous call
    } else {
        edbg(this << " postStop " << recvcnt << " " << sendcnt << " " << _rctx.systemError().message());
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
    idbg(this << " event = " << _revent);
    if (generic_event_start == _revent) {
        sock.postRecvFrom(_rctx, buf, BufferCapacity, std::bind(&Talker::onRecv, this, _1, _2, _3)); //fully asynchronous call
    } else if (generic_event_kill == _revent) {
        edbg(this << " postStop");
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
                    edbg(this << " postStop");
                    postStop(_rctx);
                    break;
                }
            } else {
                break;
            }
        } else {
            edbg(this << " postStop");
            postStop(_rctx);
            break;
        }
        --repeatcnt;
    } while (repeatcnt && sock.recvFrom(_rctx, buf, BufferCapacity, std::bind(&Talker::onRecv, this, _1, _2, _3), _raddr, _sz));

    idbg(repeatcnt);
    if (repeatcnt == 0) {
        bool rv = sock.postRecvFrom(_rctx, buf, BufferCapacity, std::bind(&Talker::onRecv, this, _1, _2, _3)); //fully asynchronous call
        SOLID_ASSERT(!rv);
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
