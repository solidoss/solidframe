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

using AioSchedulerT = frame::Scheduler<frame::aio::Reactor>;
using BufferPairT   = pair<const char*, size_t>;

//------------------------------------------------------------------
//------------------------------------------------------------------

namespace {

struct Params {
    int    listener_port;
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

frame::aio::Resolver& async_resolver(frame::aio::Resolver* _pres = nullptr)
{
    static frame::aio::Resolver& r = *_pres;
    return r;
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
        : sock(this->proxy(), std::move(_rsd))
    {
        init();
    }

    Connection(const frame::ActorIdT& _peer_obduid)
        : sock(this->proxy())
        , peer_actuid(_peer_obduid)
    {
        init();
    }

    void init()
    {
        buf_crt_recv     = 0;
        buf_crt_send     = 0;
        buf_ack_send     = 0;
        buf_pop_sending  = 0;
        buf_push_sending = 0;
    }

    ~Connection() {}

protected:
    void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent) override;
    void onStop(frame::Manager& _rm) override
    {
        _rm.notify(peer_actuid, Event(generic_event_kill));
    }

    static void onRecv(frame::aio::ReactorContext& _rctx, size_t _sz);
    static void onSend(frame::aio::ReactorContext& _rctx);

    static void onConnect(frame::aio::ReactorContext& _rctx);

    size_t doneBuffer(frame::aio::ReactorContext& _rctx);

protected:
    typedef frame::aio::Stream<frame::aio::Socket> StreamSocketT;
    enum { BufferCapacity = 1024 * 4,
        BufferCount       = 8 };

    char            buf[BufferCount][BufferCapacity];
    char            sbuf[BufferCapacity];
    uint8_t         buf_crt_recv;
    uint8_t         buf_crt_send;
    uint8_t         buf_ack_send;
    uint8_t         buf_pop_sending;
    uint8_t         buf_push_sending;
    BufferPairT     buf_sending[BufferCount];
    StreamSocketT   sock;
    frame::ActorIdT peer_actuid;
};

bool parseArguments(Params& _par, int argc, char* argv[]);

int main(int argc, char* argv[])
{

    if (parseArguments(params, argc, argv))
        return 0;

#ifndef SOLID_ON_WINDOWS
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
    lockfree::CallPoolT<void(), void> cwp{WorkPoolConfiguration(1)};
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
            }
        }

        cin.ignore();
        m.stop();
    }

    return 0;
}

//-----------------------------------------------------------------------------
bool parseArguments(Params& _par, int argc, char* argv[])
{
    using namespace cxxopts;
    try {
        Options options(argv[0], "SolidFrame Example Relay-Server Application");
        // clang-format off
        options.add_options()
            ("p,listen-port", "Listen port", value<int>(_par.listener_port)->default_value("2000"))
            ("c,connect-addr", "Connect address", value<string>(_par.connect_addr_str)->default_value(""))
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
        sock.postAccept(_rctx, [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd) { this->onAccept(_rctx, _rsd); });
    } else if (_revent == generic_event_kill) {
        postStop(_rctx);
    }
}

void Listener::onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd)
{
    solid_log(generic_logger, Info, "");
    unsigned repeatcnt = SocketInfo::max_listen_backlog_size();

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
            frame::ActorIdT        actuid = rsch.startActor(make_shared<Connection>(std::move(_rsd)), rsvc, make_event(GenericEvents::Start), err);
            rsch.startActor(make_shared<Connection>(actuid), rsvc, make_event(GenericEvents::Start), err);

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

size_t Connection::doneBuffer(frame::aio::ReactorContext& _rctx)
{
    _rctx.manager().notify(peer_actuid, make_event(GenericEvents::Raise));

    const size_t sz = buf_sending[buf_pop_sending].second;

    memcpy(sbuf, buf_sending[buf_pop_sending].first, sz);

    buf_sending[buf_pop_sending].first = nullptr;
    buf_pop_sending                    = (buf_pop_sending + 1) % BufferCount;
    return sz;
}

/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    solid_log(generic_logger, Error, this << " " << _revent);
    if (generic_event_raise == _revent) {
        BufferPairT* pbufpair = _revent.any().cast<BufferPairT>();

        if (pbufpair) {
            //new data to send
            solid_assert(buf_sending[buf_push_sending].first == nullptr);
            solid_log(generic_logger, Info, this << " new data to sent " << (int)buf_pop_sending << ' ' << (int)buf_push_sending << ' ' << pbufpair->second);
            buf_sending[buf_push_sending] = *pbufpair;

            if (buf_push_sending == buf_pop_sending && !sock.hasPendingSend()) {
                solid_log(generic_logger, Info, this << " sending " << buf_sending[buf_pop_sending].second);
                //sock.postSendAll(_rctx, buf_sending[buf_pop_sending].first, buf_sending[buf_pop_sending].second, Connection::onSend);
                buf_push_sending = (buf_push_sending + 1) % BufferCount;
                sock.postSendAll(_rctx, sbuf, doneBuffer(_rctx), Connection::onSend);
            } else {
                buf_push_sending = (buf_push_sending + 1) % BufferCount;
            }
        } else {
            //a send ack

            solid_log(generic_logger, Info, this << " on sent ack event " << (int)buf_crt_recv << ' ' << (int)buf_ack_send << ' ' << sock.hasPendingRecv());

            if (buf_ack_send != buf_crt_recv) {
                if (!sock.hasPendingRecv()) {
                    sock.postRecvSome(_rctx, buf[buf_crt_recv], BufferCapacity, Connection::onRecv);
                }
            }

            buf_ack_send = (buf_ack_send + 1) % BufferCount;
        }

    } else if (generic_event_start == _revent) {
        if (sock.device()) {
            sock.device().enableNoDelay();
            //the accepted socket
            //wait for peer to connect
        } else {
            //the connecting socket
            solid_log(generic_logger, Info, "async_resolve = " << params.connect_addr_str << " " << params.connect_port_str);
            async_resolver().requestResolve(
                ResolvFunc(_rctx.manager(), _rctx.manager().id(*this)), params.connect_addr_str.c_str(),
                params.connect_port_str.c_str(), 0, SocketInfo::Inet4, SocketInfo::Stream);
        }
    } else if (generic_event_kill == _revent) {
        solid_log(generic_logger, Error, this << " postStop");
        postStop(_rctx);
    } else if (generic_event_message == _revent) {
        ResolveData* presolvemsg = _revent.any().cast<ResolveData>();
        if (presolvemsg) {
            if (presolvemsg->empty()) {
                solid_log(generic_logger, Error, this << " postStop");
                postStop(_rctx);
            } else {
                if (sock.connect(_rctx, presolvemsg->begin(), &Connection::onConnect)) {
                    onConnect(_rctx);
                }
            }
        }

        frame::ActorIdT* ppeer_actuid = _revent.any().cast<frame::ActorIdT>();
        if (ppeer_actuid) {
            //peer connection established
            peer_actuid = *ppeer_actuid;
            //do the first read
            sock.postRecvSome(_rctx, buf[buf_crt_recv], BufferCapacity, Connection::onRecv);
        }
    }
}

/*static*/ void Connection::onConnect(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.actor());

    if (!_rctx.error()) {
        solid_log(generic_logger, Info, &rthis << " SUCCESS");

        Event ev(make_event(GenericEvents::Message));

        ev.any() = _rctx.manager().id(rthis);

        solid_log(generic_logger, Info, &rthis << " send resolv_message");
        if (_rctx.manager().notify(rthis.peer_actuid, std::move(ev))) {

            rthis.sock.device().enableNoDelay();
            //do the first read
            rthis.sock.postRecvSome(_rctx, rthis.buf[rthis.buf_crt_recv], BufferCapacity, Connection::onRecv);
        } else {
            //peer has died
            solid_log(generic_logger, Error, &rthis << " postStop " << _rctx.systemError().message());
            rthis.postStop(_rctx);
        }
    } else {
        solid_log(generic_logger, Error, &rthis << " postStop " << _rctx.systemError().message());
        rthis.postStop(_rctx);
    }
}

/*static*/ void Connection::onRecv(frame::aio::ReactorContext& _rctx, size_t _sz)
{
    Connection& rthis = static_cast<Connection&>(_rctx.actor());

    solid_log(generic_logger, Info, &rthis << " " << _sz);

    if (!_rctx.error()) {

        Event ev(make_event(GenericEvents::Raise));

        ev.any().emplace<BufferPairT>(rthis.buf[rthis.buf_crt_recv], _sz);

        _rctx.manager().notify(rthis.peer_actuid, std::move(ev));

        rthis.buf_crt_recv = (rthis.buf_crt_recv + 1) % BufferCount;

        if (rthis.buf_ack_send != rthis.buf_crt_recv) {
            rthis.sock.postRecvSome(_rctx, rthis.buf[rthis.buf_crt_recv], BufferCapacity, Connection::onRecv);
        } else {
            //no free buffer
        }

    } else {
        solid_log(generic_logger, Error, &rthis << " postStop " << _rctx.systemError().message());
        rthis.postStop(_rctx);
    }
}

/*static*/ void Connection::onSend(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.actor());

    if (!_rctx.error()) {

        solid_log(generic_logger, Info, &rthis << " " << (int)rthis.buf_pop_sending << ' ' << (int)rthis.buf_push_sending);

        if (rthis.buf_pop_sending != rthis.buf_push_sending) {
            rthis.sock.postSendAll(_rctx, rthis.sbuf, rthis.doneBuffer(_rctx), Connection::onSend);
        }

    } else {
        solid_log(generic_logger, Error, &rthis << " postStop " << _rctx.systemError().message());
        rthis.postStop(_rctx);
    }
}

//-----------------------------------------------------------------------------
