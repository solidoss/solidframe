#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioresolver.hpp"
#include "solid/frame/aio/aiosocket.hpp"
#include "solid/frame/aio/aiostream.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include "solid/system/debug.hpp"
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

    string dbg_levels;
    string dbg_modules;
    string dbg_addr;
    string dbg_port;
    bool   dbg_console;
    bool   dbg_buffered;
    bool   log;
};

Params params;

frame::aio::Resolver& async_resolver()
{
    static frame::aio::Resolver r;
    return r;
}

} //namespace

//------------------------------------------------------------------
//------------------------------------------------------------------

class Listener : public Dynamic<Listener, frame::aio::Object> {
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

class Connection : public Dynamic<Connection, frame::aio::Object> {
public:
    Connection(SocketDevice&& _rsd)
        : sock(this->proxy(), std::move(_rsd))
    {
        init();
    }

    Connection(const frame::ObjectIdT& _peer_obduid)
        : sock(this->proxy())
        , peer_objuid(_peer_obduid)
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
        _rm.notify(peer_objuid, Event(generic_event_kill));
    }

    static void onRecv(frame::aio::ReactorContext& _rctx, size_t _sz);
    static void onSend(frame::aio::ReactorContext& _rctx);

    static void onConnect(frame::aio::ReactorContext& _rctx);

    size_t doneBuffer(frame::aio::ReactorContext& _rctx);

protected:
    typedef frame::aio::Stream<frame::aio::Socket> StreamSocketT;
    enum { BufferCapacity = 1024 * 4,
        BufferCount       = 8 };

    char             buf[BufferCount][BufferCapacity];
    char             sbuf[BufferCapacity];
    uint8_t          buf_crt_recv;
    uint8_t          buf_crt_send;
    uint8_t          buf_ack_send;
    uint8_t          buf_pop_sending;
    uint8_t          buf_push_sending;
    BufferPairT      buf_sending[BufferCount];
    StreamSocketT    sock;
    frame::ObjectIdT peer_objuid;
};

bool parseArguments(Params& _par, int argc, char* argv[]);

int main(int argc, char* argv[])
{

    if (parseArguments(params, argc, argv))
        return 0;

    signal(SIGPIPE, SIG_IGN);

#ifdef SOLID_HAS_DEBUG
    {
        string dbgout;
        Debug::the().levelMask(params.dbg_levels.c_str());
        Debug::the().moduleMask(params.dbg_modules.c_str());
        if (params.dbg_addr.size() && params.dbg_port.size()) {
            Debug::the().initSocket(
                params.dbg_addr.c_str(),
                params.dbg_port.c_str(),
                params.dbg_buffered,
                &dbgout);
        } else if (params.dbg_console) {
            Debug::the().initStdErr(
                params.dbg_buffered,
                &dbgout);
        } else {
            Debug::the().initFile(
                *argv[0] == '.' ? argv[0] + 2 : argv[0],
                params.dbg_buffered,
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
    async_resolver().start(1);
    {

        AioSchedulerT sch;

        frame::Manager  m;
        frame::ServiceT svc(m);

        if (sch.start(thread::hardware_concurrency())) {
            cout << "Error starting scheduler" << endl;
        } else {
            ResolveData rd = synchronous_resolve("0.0.0.0", params.listener_port, 0, SocketInfo::Inet4, SocketInfo::Stream);

            SocketDevice sd;

            sd.create(rd.begin());
            sd.prepareAccept(rd.begin(), SocketInfo::max_listen_backlog_size());

            if (sd) {
                DynamicPointer<frame::aio::Object> objptr(new Listener(svc, sch, std::move(sd)));
                solid::ErrorConditionT             err;
                solid::frame::ObjectIdT            objuid;

                objuid = sch.startObject(objptr, svc, make_event(GenericEvents::Start), err);
                idbg("Started Listener object: " << objuid.index << ',' << objuid.unique);
            } else {
                cout << "Error creating listener socket" << endl;
            }
        }

        cin.ignore();

        async_resolver().stop();
        m.stop();
    }

    return 0;
}

//-----------------------------------------------------------------------------
bool parseArguments(Params& _par, int argc, char* argv[])
{
    using namespace boost::program_options;
    try {
        options_description desc("SolidFrame Example Relay-Server Application");
        desc.add_options()("help,h", "List program options")("listen-port,l", value<int>(&_par.listener_port)->default_value(2000), "Listener port")("connect-addr,c", value<string>(&_par.connect_addr_str)->default_value(""), "Connect address")("debug-levels,L", value<string>(&_par.dbg_levels)->default_value("view"), "Debug logging levels")("debug-modules,M", value<string>(&_par.dbg_modules), "Debug logging modules")("debug-address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")("debug-port,P", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")("debug-console,C", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")("debug-unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered");
        variables_map vm;
        store(parse_command_line(argc, argv, desc), vm);
        notify(vm);
        if (vm.count("help")) {
            cout << desc << "\n";
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
    idbg("event = " << _revent);
    if (_revent == generic_event_start) {
        sock.postAccept(_rctx, [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd) { this->onAccept(_rctx, _rsd); });
    } else if (_revent == generic_event_kill) {
        postStop(_rctx);
    }
}

void Listener::onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd)
{
    idbg("");
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

            frame::ObjectIdT objuid;
            {
                DynamicPointer<frame::aio::Object> objptr(new Connection(std::move(_rsd)));
                solid::ErrorConditionT             err;

                objuid = rsch.startObject(objptr, rsvc, make_event(GenericEvents::Start), err);
            }

            {
                DynamicPointer<frame::aio::Object> objptr(new Connection(objuid));
                solid::ErrorConditionT             err;

                rsch.startObject(objptr, rsvc, make_event(GenericEvents::Start), err);
            }
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
    frame::Manager&  rm;
    frame::ObjectIdT objuid;

    ResolvFunc(frame::Manager& _rm, frame::ObjectIdT const& _robjuid)
        : rm(_rm)
        , objuid(_robjuid)
    {
    }

    void operator()(ResolveData& _rrd, ErrorCodeT const& _rerr)
    {
        Event ev(make_event(GenericEvents::Message));

        ev.any() = std::move(_rrd);

        idbg(this << " send resolv_message");
        rm.notify(objuid, std::move(ev));
    }
};

size_t Connection::doneBuffer(frame::aio::ReactorContext& _rctx)
{
    _rctx.manager().notify(peer_objuid, make_event(GenericEvents::Raise));

    const size_t sz = buf_sending[buf_pop_sending].second;

    memcpy(sbuf, buf_sending[buf_pop_sending].first, sz);

    buf_sending[buf_pop_sending].first = nullptr;
    buf_pop_sending                    = (buf_pop_sending + 1) % BufferCount;
    return sz;
}

/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    edbg(this << " " << _revent);
    if (generic_event_raise == _revent) {
        BufferPairT* pbufpair = _revent.any().cast<BufferPairT>();

        if (pbufpair) {
            //new data to send
            SOLID_ASSERT(buf_sending[buf_push_sending].first == nullptr);
            idbg(this << " new data to sent " << (int)buf_pop_sending << ' ' << (int)buf_push_sending << ' ' << pbufpair->second);
            buf_sending[buf_push_sending] = *pbufpair;

            if (buf_push_sending == buf_pop_sending && not sock.hasPendingSend()) {
                idbg(this << " sending " << buf_sending[buf_pop_sending].second);
                //sock.postSendAll(_rctx, buf_sending[buf_pop_sending].first, buf_sending[buf_pop_sending].second, Connection::onSend);
                buf_push_sending = (buf_push_sending + 1) % BufferCount;
                sock.postSendAll(_rctx, sbuf, doneBuffer(_rctx), Connection::onSend);
            } else {
                buf_push_sending = (buf_push_sending + 1) % BufferCount;
            }
        } else {
            //a send ack

            idbg(this << " on sent ack event " << (int)buf_crt_recv << ' ' << (int)buf_ack_send << ' ' << sock.hasPendingRecv());

            if (buf_ack_send != buf_crt_recv) {
                if (not sock.hasPendingRecv()) {
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
            idbg("async_resolve = " << params.connect_addr_str << " " << params.connect_port_str);
            async_resolver().requestResolve(
                ResolvFunc(_rctx.manager(), _rctx.manager().id(*this)), params.connect_addr_str.c_str(),
                params.connect_port_str.c_str(), 0, SocketInfo::Inet4, SocketInfo::Stream);
        }
    } else if (generic_event_kill == _revent) {
        edbg(this << " postStop");
        postStop(_rctx);
    } else if (generic_event_message == _revent) {
        ResolveData* presolvemsg = _revent.any().cast<ResolveData>();
        if (presolvemsg) {
            if (presolvemsg->empty()) {
                edbg(this << " postStop");
                postStop(_rctx);
            } else {
                if (sock.connect(_rctx, presolvemsg->begin(), &Connection::onConnect)) {
                    onConnect(_rctx);
                }
            }
        }

        frame::ObjectIdT* ppeer_objuid = _revent.any().cast<frame::ObjectIdT>();
        if (ppeer_objuid) {
            //peer connection established
            peer_objuid = *ppeer_objuid;
            //do the first read
            sock.postRecvSome(_rctx, buf[buf_crt_recv], BufferCapacity, Connection::onRecv);
        }
    }
}

/*static*/ void Connection::onConnect(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());

    if (!_rctx.error()) {
        idbg(&rthis << " SUCCESS");

        Event ev(make_event(GenericEvents::Message));

        ev.any() = _rctx.manager().id(rthis);

        idbg(&rthis << " send resolv_message");
        if (_rctx.manager().notify(rthis.peer_objuid, std::move(ev))) {

            rthis.sock.device().enableNoDelay();
            //do the first read
            rthis.sock.postRecvSome(_rctx, rthis.buf[rthis.buf_crt_recv], BufferCapacity, Connection::onRecv);
        } else {
            //peer has died
            edbg(&rthis << " postStop " << _rctx.systemError().message());
            rthis.postStop(_rctx);
        }
    } else {
        edbg(&rthis << " postStop " << _rctx.systemError().message());
        rthis.postStop(_rctx);
    }
}

/*static*/ void Connection::onRecv(frame::aio::ReactorContext& _rctx, size_t _sz)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());

    idbg(&rthis << " " << _sz);

    if (!_rctx.error()) {

        Event ev(make_event(GenericEvents::Raise));

        ev.any() = BufferPairT(rthis.buf[rthis.buf_crt_recv], _sz);

        _rctx.manager().notify(rthis.peer_objuid, std::move(ev));

        rthis.buf_crt_recv = (rthis.buf_crt_recv + 1) % BufferCount;

        if (rthis.buf_ack_send != rthis.buf_crt_recv) {
            rthis.sock.postRecvSome(_rctx, rthis.buf[rthis.buf_crt_recv], BufferCapacity, Connection::onRecv);
        } else {
            //no free buffer
        }

    } else {
        edbg(&rthis << " postStop " << _rctx.systemError().message());
        rthis.postStop(_rctx);
    }
}

/*static*/ void Connection::onSend(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());

    if (!_rctx.error()) {

        idbg(&rthis << " " << (int)rthis.buf_pop_sending << ' ' << (int)rthis.buf_push_sending);

        if (rthis.buf_pop_sending != rthis.buf_push_sending) {
            rthis.sock.postSendAll(_rctx, rthis.sbuf, rthis.doneBuffer(_rctx), Connection::onSend);
        }

    } else {
        edbg(&rthis << " postStop " << _rctx.systemError().message());
        rthis.postStop(_rctx);
    }
}

//-----------------------------------------------------------------------------
