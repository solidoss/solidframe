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

#include "solid/system/log.hpp"
#include "solid/system/socketaddress.hpp"
#include "solid/system/socketdevice.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/utility/event.hpp"
#include "solid/utility/workpool.hpp"

#include "cxxopts.hpp"

#include <functional>
#include <iostream>
#include <signal.h>
#include <unordered_map>

using namespace std;
using namespace solid;
using namespace std::placeholders;

typedef frame::Scheduler<frame::aio::Reactor> AioSchedulerT;
typedef std::atomic<uint32_t>                 AtomicUint32_tT;
typedef std::unordered_map<uint32_t,
    solid::frame::ActorIdT>
    UniqueMapT;
enum Events {
    EventStartE = 0,
    EventRunE,
    EventStopE,
    EventMoveE,
    EventResolveE,
};

//------------------------------------------------------------------
//------------------------------------------------------------------

namespace {

struct Params {
    int    listener_port;
    int    talker_port;
    string connect_addr_str;
    string connect_port_str;

    vector<string> dbg_modules;
    string         dbg_addr;
    string         dbg_port;
    bool           dbg_console;
    bool           dbg_buffered;
    bool           log;
};

Params params;

mutex              mtx;
condition_variable cnd;
bool               running(true);

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

AtomicUint32_tT crt_id(0);
UniqueMapT      umap;

frame::aio::Resolver& async_resolver(frame::aio::Resolver* _pres = nullptr)
{
    static frame::aio::Resolver& r = *_pres;
    return r;
}

void connection_register(uint32_t _id, frame::ActorIdT const& _ruid)
{
    umap[_id] = _ruid;
}

frame::ActorIdT connection_uid(uint32_t _id)
{
    frame::ActorIdT rv;
    auto            it = umap.find(_id);
    if (it != umap.end()) {
        rv = it->second;
        umap.erase(it);
    }
    return rv;
}

void connection_unregister(uint32_t _id)
{
    connection_uid(_id);
}

} //namespace

//------------------------------------------------------------------
//------------------------------------------------------------------

class Listener : public frame::aio::Actor {
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
    ~Listener()
    {
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
};

class Connection : public frame::aio::Actor {
public:
    Connection(SocketDevice&& _rsd)
        : sock1(this->proxy(), std::move(_rsd))
        , sock2(this->proxy())
        , recvcnt(0)
        , sendcnt(0)
        , crtid(-1)
    {
    }
    ~Connection() {}

protected:
    void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent) override;
    void onStop(frame::Manager& _rm) override;

    static void onRecvSock1(frame::aio::ReactorContext& _rctx, size_t _sz);
    static void onRecvSock2(frame::aio::ReactorContext& _rctx, size_t _sz);
    static void onSendSock1(frame::aio::ReactorContext& _rctx);
    static void onSendSock2(frame::aio::ReactorContext& _rctx);

    void        onRecvId(frame::aio::ReactorContext& _rctx, size_t _off, size_t _sz);
    static void onSendId(frame::aio::ReactorContext& _rctx);

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

bool parseArguments(Params& _par, int argc, char* argv[]);

int main(int argc, char* argv[])
{

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
    lockfree::CallPoolT<void(), void> cwp{WorkPoolConfiguration(1), 1};
    frame::aio::Resolver              resolver([&cwp](std::function<void()>&& _fnc) { cwp.push(std::move(_fnc)); });

    async_resolver(&resolver);
    {

        AioSchedulerT sch;

        frame::Manager  m;
        frame::ServiceT svc(m);

        sch.start(thread::hardware_concurrency());

        {
            ResolveData rd = synchronous_resolve("0.0.0.0", params.listener_port, 0, SocketInfo::Inet4, SocketInfo::Stream);

            SocketDevice sd;

            sd.create(rd.begin());
            sd.prepareAccept(rd.begin(), SocketInfo::max_listen_backlog_size());

            if (sd) {
                solid::ErrorConditionT err;
                solid::frame::ActorIdT actuid;

                actuid = sch.startActor(make_shared<Listener>(svc, sch, std::move(sd)), svc, make_event(GenericEvents::Start), err);
                solid_log(generic_logger, Info, "Started Listener actor: " << actuid.index << ',' << actuid.unique);
            } else {
                cout << "Error creating listener socket" << endl;
                running = false;
            }
        }

        if ((0)) {
            unique_lock<mutex> lock(mtx);
            while (running) {
                cnd.wait(lock);
            }
        } else {
            cin.ignore();
        }

        m.stop();
    }

    return 0;
}

//-----------------------------------------------------------------------------
bool parseArguments(Params& _par, int argc, char* argv[])
{
    using namespace cxxopts;
    try {
        Options options{argv[0], "SolidFrame concept application"};
        // clang-format off
        options.add_options()("help,h", "List program options")
            ("l,listen-port", "IPC Listen port", value<int>(_par.listener_port)->default_value("2000"))
            ("c,connect", "Connect address", value<string>(_par.connect_addr_str)->default_value(""))
            ("M,debug-modules", "Debug logging modules", value<vector<string>>(_par.dbg_modules))
            ("A,debug-address", "Debug server address (e.g. on linux use: nc -l 2222)", value<string>(_par.dbg_addr))
            ("P,debug-port", "Debug server port (e.g. on linux use: nc -l 2222)", value<string>(_par.dbg_port))
            ("C,debug-console", "Debug console", value<bool>(_par.dbg_console)->implicit_value("true")->default_value("false"))
            ("S,debug-unbuffered", "Debug unbuffered", value<bool>(_par.dbg_buffered)->implicit_value("false")->default_value("true"))
            ("h,help", "List program options");
        // clang-format on
        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help({""}) << std::endl;
            return true;
        }

        size_t pos;

        pos = _par.connect_addr_str.rfind(':');
        if (pos != string::npos) {
            _par.connect_addr_str[pos] = '\0';

            _par.connect_port_str.assign(_par.connect_addr_str.c_str() + pos + 1);

            _par.connect_addr_str.resize(pos);
        } else {
            _par.connect_port_str = _par.listener_port;
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
    solid_log(generic_logger, Info, "event = " << _revent);
    if (_revent == generic_event_start) {
        sock.postAccept(_rctx, std::bind(&Listener::onAccept, this, _1, _2));
    } else if (_revent == generic_event_kill) {
        postStop(_rctx);
    }
}

void Listener::onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd)
{
    solid_log(generic_logger, Info, "");
    size_t repeatcnt = SocketInfo::max_listen_backlog_size();

    do {
        if (!_rctx.error()) {
#if 0
            int sz = 1024 * 64;
            _rsd.recvBufferSize(sz);
            sz = 1024 * 32;
            _rsd.sendBufferSize(sz);
#endif
            _rsd.enableNoDelay();
            solid::ErrorConditionT err;

            rsch.startActor(make_shared<Connection>(std::move(_rsd)), rsvc, make_event(GenericEvents::Start), err);
        } else {
            //e.g. a limit of open file descriptors was reached - we sleep for 10 seconds
            //timer.waitFor(_rctx, NanoTime(10), std::bind(&Listener::onEvent, this, _1, frame::Event(EventStartE)));
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

struct ResolvFunc {
    frame::Manager& rm;
    frame::ActorIdT actuid;

    ResolvFunc(frame::Manager& _rm, frame::ActorIdT const& _ractuid)
        : rm(_rm)
        , actuid(_ractuid)
    {
    }

    void operator()(ResolveData& _rrd, ErrorCodeT const& _rerr)
    {
        Event ev(make_event(GenericEvents::Message));

        ev.any() = std::move(_rrd);

        solid_log(generic_logger, Info, this << " send resolv_message");
        rm.notify(actuid, std::move(ev));
    }
};

struct MoveMessage {
    SocketDevice sd;
    uint8_t      sz;
    char         buf[12];

    MoveMessage(
        SocketDevice&& _rsd,
        char* _buf, uint8_t _buflen)
        : sd(std::move(_rsd))
        , sz(_buflen)
    {
        solid_assert(_buflen < 12);
        memcpy(buf, _buf, _buflen);
    }

    MoveMessage(
        MoveMessage&& _umm)
        : sd(std::move(_umm.sd))
        , sz(_umm.sz)
    {
        memcpy(buf, _umm.buf, sz);
    }
};

/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    solid_log(generic_logger, Error, this << " " << _revent);
    if (generic_event_start == _revent) {
        if (params.connect_addr_str.size()) {
            //we must resolve the address then connect
            solid_log(generic_logger, Info, "async_resolve = " << params.connect_addr_str << " " << params.connect_port_str);
            async_resolver().requestResolve(
                ResolvFunc(_rctx.manager(), _rctx.manager().id(*this)), params.connect_addr_str.c_str(),
                params.connect_port_str.c_str(), 0, SocketInfo::Inet4, SocketInfo::Stream);
        } else {
            const uint32_t id = crtid = crt_id++;

            snprintf(buf1, BufferCapacity, "%lu\r\n", static_cast<unsigned long>(id));
            sock1.postSendAll(_rctx, buf1, strlen(buf1), Connection::onSendId);
            sock1.postRecvSome(_rctx, buf2, 12, [this](frame::aio::ReactorContext& _rctx, size_t _sz) { return onRecvId(_rctx, 0, _sz); });
        }
    } else if (generic_event_kill == _revent) {
        solid_log(generic_logger, Error, this << " postStop");
        postStop(_rctx);
    } else if (generic_event_message == _revent) {
        MoveMessage* pmovemsg = _revent.any().cast<MoveMessage>();
        if (pmovemsg) {
            sock2.reset(_rctx, std::move(pmovemsg->sd));
            if (pmovemsg->sz) {
                memcpy(buf2, pmovemsg->buf, pmovemsg->sz);
                sock1.postSendAll(_rctx, buf2, pmovemsg->sz, Connection::onSendSock1);
            } else {
                sock2.postRecvSome(_rctx, buf2, BufferCapacity, Connection::onRecvSock2);
            }
            if (buf1[0]) {
                sock2.postSendAll(_rctx, buf1 + 1, buf1[0], Connection::onSendSock2);
            } else {
                sock1.postRecvSome(_rctx, buf1, BufferCapacity, Connection::onRecvSock1);
            }
        }
        ResolveData* presolvemsg = _revent.any().cast<ResolveData>();
        if (presolvemsg) {
            if (presolvemsg->empty()) {
                solid_log(generic_logger, Error, this << " postStop");
                //sock.shutdown(_rctx);
                postStop(_rctx);
            } else {
                if (sock2.connect(_rctx, presolvemsg->begin(), std::bind(&Connection::onConnect, this, _1))) {
                    onConnect(_rctx);
                }
            }
        }
    }
}

void Connection::onConnect(frame::aio::ReactorContext& _rctx)
{
    if (!_rctx.error()) {
        solid_log(generic_logger, Info, this << " SUCCESS");
        sock2.device().enableNoDelay();
        sock1.postRecvSome(_rctx, buf1, BufferCapacity, Connection::onRecvSock1);
        sock2.postRecvSome(_rctx, buf2, BufferCapacity, Connection::onRecvSock2);
    } else {
        solid_log(generic_logger, Error, this << " postStop " << recvcnt << " " << sendcnt << " " << _rctx.systemError().message());
        postStop(_rctx);
    }
}

/*virtual*/ void Connection::onStop(frame::Manager& _rm)
{
    connection_unregister(crtid);
}

/*static*/ void Connection::onRecvSock1(frame::aio::ReactorContext& _rctx, size_t _sz)
{
    unsigned    repeatcnt = 4;
    Connection& rthis     = static_cast<Connection&>(_rctx.actor());
    solid_log(generic_logger, Info, &rthis << " " << _sz);
    do {
        if (!_rctx.error()) {
            bool rv = rthis.sock2.sendAll(_rctx, rthis.buf1, _sz, Connection::onSendSock2);
            if (rv) {
                if (_rctx.error()) {
                    solid_log(generic_logger, Error, &rthis << " postStop");
                    rthis.postStop(_rctx);
                    break;
                }
            } else {
                break;
            }
        } else {
            solid_log(generic_logger, Error, &rthis << " postStop");
            rthis.postStop(_rctx);
            break;
        }
        --repeatcnt;
    } while (repeatcnt && rthis.sock1.recvSome(_rctx, rthis.buf1, BufferCapacity, Connection::onRecvSock1, _sz));

    if (repeatcnt == 0) {
        bool rv = rthis.sock1.postRecvSome(_rctx, rthis.buf1, BufferCapacity, Connection::onRecvSock1); //fully asynchronous call
        solid_assert(!rv);
    }
}

/*static*/ void Connection::onRecvSock2(frame::aio::ReactorContext& _rctx, size_t _sz)
{
    unsigned    repeatcnt = 4;
    Connection& rthis     = static_cast<Connection&>(_rctx.actor());
    solid_log(generic_logger, Info, &rthis << " " << _sz);
    do {
        if (!_rctx.error()) {
            bool rv = rthis.sock1.sendAll(_rctx, rthis.buf2, _sz, Connection::onSendSock1);
            if (rv) {
                if (_rctx.error()) {
                    solid_log(generic_logger, Error, &rthis << " postStop");
                    rthis.postStop(_rctx);
                    break;
                }
            } else {
                break;
            }
        } else {
            solid_log(generic_logger, Error, &rthis << " postStop");
            rthis.postStop(_rctx);
            break;
        }
        --repeatcnt;
    } while (repeatcnt && rthis.sock2.recvSome(_rctx, rthis.buf2, BufferCapacity, Connection::onRecvSock2, _sz));

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
        solid_log(generic_logger, Error, &rthis << " postStop");
        rthis.postStop(_rctx);
    }
}

/*static*/ void Connection::onSendSock2(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.actor());
    if (!_rctx.error()) {
        rthis.sock1.postRecvSome(_rctx, rthis.buf1, BufferCapacity, Connection::onRecvSock1);
    } else {
        solid_log(generic_logger, Error, &rthis << " postStop");
        rthis.postStop(_rctx);
    }
}

void Connection::onRecvId(frame::aio::ReactorContext& _rctx, size_t _off, size_t _sz)
{
    solid_log(generic_logger, Info, "sz = " << _sz << " off = " << _off);
    _sz += _off;

    size_t i         = _off;
    bool   found_eol = false;
    for (; i < _sz; ++i) {
        if (buf2[i] == '\n') {
            buf2[i]   = '\0';
            found_eol = true;
            break;
        }
        if (buf2[i] == '\r') {
            buf2[i]   = '\0';
            found_eol = true;
            if ((i + 1) < _sz && buf2[i + 1] == '\n') {
                ++i;
            }
        }
    }

    if (found_eol) {
        uint32_t idx = InvalidIndex();
        if (strlen(buf2) >= 1) {
            sscanf(buf2, "%ul", &idx);
            solid_log(generic_logger, Info, this << " received idx = " << idx);
        }
        if (idx == crtid || idx == InvalidIndex()) {
            //wait for a peer connection
            buf1[0] = _sz - i;
            buf1[1] = i;
            memcpy(buf1 + 1, buf2 + i, _sz - i);
            connection_register(crtid, _rctx.manager().id(*this));
        } else {
            //move to a peer connection
            frame::ActorIdT actid = connection_uid(idx);
            Event           ev(make_event(GenericEvents::Message));
            SocketDevice    sd(sock1.reset(_rctx));
            ev.any().emplace<MoveMessage>(std::move(sd), buf2 + i, static_cast<uint8_t>(_sz - i));
            solid_log(generic_logger, Info, this << " send move_message with size = " << (_sz - i));
            _rctx.manager().notify(actid, std::move(ev));
            solid_log(generic_logger, Error, this << " postStop");
            postStop(_rctx);
        }
    } else {
        //not found
        if (_sz < 12) {
            _off = _sz;
            sock1.postRecvSome(_rctx, buf2 + _sz, 12 - _sz, [this, _off](frame::aio::ReactorContext& _rctx, size_t _sz) { return onRecvId(_rctx, _off, _sz); });
        } else {
            solid_log(generic_logger, Error, this << " postStop");
            postStop(_rctx);
        }
    }
}

/*static*/ void Connection::onSendId(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.actor());
    if (!_rctx.error()) {
        solid_log(generic_logger, Info, &rthis << "");
    } else {
        solid_log(generic_logger, Error, &rthis << " postStop " << rthis.recvcnt << " " << rthis.sendcnt);
        rthis.postStop(_rctx);
    }
}

//-----------------------------------------------------------------------------
