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
#include "solid/utility/threadpool.hpp"

#include "cxxopts.hpp"

#include <functional>
#include <iostream>
#include <signal.h>
#include <unordered_map>

using namespace std;
using namespace solid;
using namespace std::placeholders;

using AioSchedulerT = frame::Scheduler<frame::aio::Reactor<Event<32>>>;
using BufferPairT   = pair<const char*, size_t>;

//------------------------------------------------------------------
//------------------------------------------------------------------

namespace {

struct Params {
    int    listener_port = -1;
    string connect_addr_str;
    string connect_port_str;

    vector<string> dbg_modules;
    string         dbg_addr;
    string         dbg_port;
    bool           dbg_console  = true;
    bool           dbg_buffered = true;
    bool           log          = true;
};

Params params;

frame::aio::Resolver& async_resolver(frame::aio::Resolver* _pres = nullptr)
{
    static frame::aio::Resolver& r = *_pres;
    return r;
}

using CallPoolT = ThreadPool<Function<void(), 80>, Function<void(), 80>>;

} // namespace

//------------------------------------------------------------------
//------------------------------------------------------------------

class Listener final : public frame::aio::Actor {
public:
    Listener(
        frame::Service& _rsvc,
        AioSchedulerT&  _rsched,
        SocketDevice&&  _rsd)
        : rsvc_(_rsvc)
        , rsch_(_rsched)
        , sock_(this->proxy(), std::move(_rsd))
    {
    }
    ~Listener()
    {
    }

private:
    void onEvent(frame::aio::ReactorContext& _rctx, EventBase&& _revent) override;
    void onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd);

    void onTimer(frame::aio::ReactorContext& _rctx);

    typedef frame::aio::Listener    ListenerSocketT;
    typedef frame::aio::SteadyTimer TimerT;

    frame::Service& rsvc_;
    AioSchedulerT&  rsch_;
    ListenerSocketT sock_;
};

class BufferBase;
using BufferPtrT = std::shared_ptr<BufferBase>;

class BufferBase {
    char*  pb_;
    size_t bc_;

protected:
    BufferBase(char* _pb, size_t _bc)
        : pb_(_pb)
        , bc_(_bc)
    {
    }

public:
    BufferPtrT next_;

    virtual ~BufferBase() {}

    char* data() const
    {
        return pb_;
    }

    size_t capacity() const
    {
        return bc_;
    }
};

template <size_t C>
class Buffer;

template <>
class Buffer<0> : public BufferBase {
public:
    Buffer(size_t _cp)
        : BufferBase(new char[_cp], _cp)
    {
    }
};

template <size_t C>
class Buffer : public BufferBase {
    char b[C];

public:
    Buffer(size_t _cp = 0)
        : BufferBase(b, C)
    {
    }
};

BufferPtrT make_buffer(const size_t _sz)
{
    if (_sz <= 512) {
        return std::make_shared<Buffer<512>>(_sz);
    } else if (_sz <= 1024) {
        return std::make_shared<Buffer<1024 * 1>>(_sz);
    } else if (_sz <= 1024 * 2) {
        return std::make_shared<Buffer<1024 * 2>>(_sz);
    } else if (_sz <= 1024 * 4) {
        return std::make_shared<Buffer<1024 * 4>>(_sz);
    } else if (_sz <= 1024 * 8) {
        return std::make_shared<Buffer<1024 * 8>>(_sz);
    } else if (_sz <= 1024 * 16) {
        return std::make_shared<Buffer<1024 * 16>>(_sz);
    } else if (_sz <= 1024 * 32) {
        return std::make_shared<Buffer<1024 * 32>>(_sz);
    } else if (_sz <= 1024 * 64) {
        return std::make_shared<Buffer<1024 * 64>>(_sz);
    }

    return std::make_shared<Buffer<0>>(_sz);
}

struct EventData;
using EventDataPtrT = std::unique_ptr<EventData>;

struct EventData {
    EventDataPtrT   nextptr_;
    frame::ActorIdT senderid_;
    BufferPtrT      bufptr_;
    size_t          bufsz_;

    EventData()
        : bufsz_(-1)
    {
    }
};

class Connection final : public frame::aio::Actor {
public:
    Connection(SocketDevice&& _rsd)
        : sock_(this->proxy(), std::move(_rsd))
    {
        init();
    }

    Connection(const frame::ActorIdT& _peer_obduid)
        : sock_(this->proxy())
        , peer_actuid_(_peer_obduid)
    {
        init();
    }

    ~Connection() {}

protected:
    void init()
    {
        rbufcnt_        = 1;
        rbufptr_        = make_buffer(BufferCapacity);
        pweventdataend_ = nullptr;
    }

    void onEvent(frame::aio::ReactorContext& _rctx, EventBase&& _revent) override;
    void onStop(frame::Manager& _rm) override
    {
        _rm.notify(peer_actuid_, Event(generic_event<GenericEventE::Kill>));
    }

    static void onRecv(frame::aio::ReactorContext& _rctx, size_t _sz);
    static void onSend(frame::aio::ReactorContext& _rctx);

    static void onConnect(frame::aio::ReactorContext& _rctx);

    bool prepareCurrentReadBuffer()
    {
        if (rbufptr_) {
            return true;
        } else if (rbufcnt_ < BufferCount) {
            rbufptr_ = make_buffer(BufferCapacity);
            ++rbufcnt_;
            return true;
        }

        return false;
    }

    EventDataPtrT makeReadEventData(frame::aio::ReactorContext& _rctx, const size_t _bufsz)
    {
        EventDataPtrT rv;
        if (reventdataptr_) {
            rv             = std::move(reventdataptr_);
            reventdataptr_ = std::move(rv->nextptr_);
        } else {
            // rv = std::make_shared<EventData>();
            rv = EventDataPtrT(new EventData());
        }
        rv->senderid_ = _rctx.manager().id(*this);
        rv->bufptr_   = std::move(rbufptr_);
        rbufptr_      = std::move(rv->bufptr_->next_);
        rv->bufsz_    = _bufsz;
        return rv;
    }

    EventDataPtrT popWriteEventData()
    {
        EventDataPtrT rv;
        rv                = std::move(weventdatabegptr_);
        rv->bufsz_        = 0;
        weventdatabegptr_ = std::move(rv->nextptr_);
        if (!weventdatabegptr_) {
            pweventdataend_ = nullptr;
        }
        return rv;
    }

protected:
    enum { BufferCapacity = 1024 * 4,
        BufferCount       = 4 };
    using StreamSocketT = frame::aio::Stream<frame::aio::Socket>;

    BufferPtrT      rbufptr_;
    uint16_t        rbufcnt_;
    EventDataPtrT   reventdataptr_;
    EventDataPtrT   weventdatabegptr_;
    EventData*      pweventdataend_;
    StreamSocketT   sock_;
    frame::ActorIdT peer_actuid_;
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

    cout << "sizeof(Connection) = " << sizeof(Connection) << endl;

    CallPoolT            cwp{{1, 100, 0}, [](const size_t) {}, [](const size_t) {}};
    frame::aio::Resolver resolver([&cwp](std::function<void()>&& _fnc) { cwp.pushOne(std::move(_fnc)); });

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
            sd.prepareAccept(rd.begin(), 2000);

            if (sd) {
                solid::ErrorConditionT err;
                solid::frame::ActorIdT actuid;

                actuid = sch.startActor(make_shared<Listener>(svc, sch, std::move(sd)), svc, make_event(GenericEventE::Start), err);
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

/*virtual*/ void Listener::onEvent(frame::aio::ReactorContext& _rctx, EventBase&& _revent)
{
    solid_log(generic_logger, Info, "event = " << _revent);
    if (_revent == generic_event<GenericEventE::Start>) {
        sock_.postAccept(_rctx, [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd) { this->onAccept(_rctx, _rsd); });
    } else if (_revent == generic_event<GenericEventE::Kill>) {
        postStop(_rctx);
    }
}

void Listener::onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd)
{
    solid_log(generic_logger, Info, "");
    unsigned repeatcnt = 4;

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
            frame::ActorIdT        actuid = rsch_.startActor(make_shared<Connection>(std::move(_rsd)), rsvc_, make_event(GenericEventE::Start), err);

            rsch_.startActor(make_shared<Connection>(actuid), rsvc_, make_event(GenericEventE::Start), err);
        } else {
            // e.g. a limit of open file descriptors was reached - we sleep for 10 seconds
            // timer.waitFor(_rctx, NanoTime(10), std::bind(&Listener::onEvent, this, _1, frame::Event(EventStartE)));
            break;
        }
        --repeatcnt;
    } while (repeatcnt && sock_.accept(_rctx, std::bind(&Listener::onAccept, this, _1, _2), _rsd));

    if (!repeatcnt) {
        sock_.postAccept(
            _rctx,
            [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd) { onAccept(_rctx, _rsd); }); // fully asynchronous call
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
        auto ev(make_event(GenericEventE::Message, _rrd));

        solid_log(generic_logger, Info, this << " send resolv_message");
        rm.notify(actuid, std::move(ev));
    }
};

/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext& _rctx, EventBase&& _revent)
{
    solid_log(generic_logger, Error, this << " " << _revent);
    if (generic_event<GenericEventE::Wake> == _revent) {
        EventDataPtrT& evtdataptr = *_revent.cast<EventDataPtrT>();

        if (evtdataptr->bufsz_) {
            // new data to send
            solid_log(generic_logger, Info, this << " new data to sent ");

            if (pweventdataend_) {
                pweventdataend_->nextptr_ = std::move(evtdataptr);
                pweventdataend_           = pweventdataend_->nextptr_.get();
            } else {
                weventdatabegptr_ = std::move(evtdataptr);
                pweventdataend_   = weventdatabegptr_.get();
                sock_.postSendAll(_rctx, weventdatabegptr_->bufptr_->data(), weventdatabegptr_->bufsz_, Connection::onSend);
            }
        } else {
            // a send ack

            solid_log(generic_logger, Info, this << " on sent ack event " << sock_.hasPendingRecv());

            // store both the read event data ptr and the bufptr
            // first store the bufptr
            if (rbufptr_) {
                evtdataptr->bufptr_->next_ = std::move(rbufptr_->next_);
                rbufptr_->next_            = std::move(evtdataptr->bufptr_);
            } else {
                rbufptr_ = std::move(evtdataptr->bufptr_);
                sock_.postRecvSome(_rctx, rbufptr_->data(), rbufptr_->capacity(), Connection::onRecv);
            }
            // then store the event data ptr
            evtdataptr->nextptr_ = std::move(reventdataptr_);
            reventdataptr_       = std::move(evtdataptr);
        }

    } else if (generic_event<GenericEventE::Start> == _revent) {
        if (sock_.device()) {
            sock_.device().enableNoDelay();
            // the accepted socket
            // wait for peer to connect
        } else {
            // the connecting socket
            solid_log(generic_logger, Info, "async_resolve = " << params.connect_addr_str << " " << params.connect_port_str);
            async_resolver().requestResolve(
                ResolvFunc(_rctx.manager(), _rctx.manager().id(*this)), params.connect_addr_str.c_str(),
                params.connect_port_str.c_str(), 0, SocketInfo::Inet4, SocketInfo::Stream);
        }
    } else if (generic_event<GenericEventE::Kill> == _revent) {
        solid_log(generic_logger, Error, this << " postStop");
        postStop(_rctx);
    } else if (generic_event<GenericEventE::Message> == _revent) {
        ResolveData* presolvemsg = _revent.cast<ResolveData>();
        if (presolvemsg) {
            if (presolvemsg->empty()) {
                solid_log(generic_logger, Error, this << " postStop");
                postStop(_rctx);
            } else {
                if (sock_.connect(_rctx, presolvemsg->begin(), &Connection::onConnect)) {
                    onConnect(_rctx);
                }
            }
        }

        frame::ActorIdT* ppeer_actuid = _revent.cast<frame::ActorIdT>();
        if (ppeer_actuid) {
            // peer connection established
            peer_actuid_ = *ppeer_actuid;
            // do the first read
            sock_.postRecvSome(_rctx, rbufptr_->data(), rbufptr_->capacity(), Connection::onRecv);
        }
    }
}

/*static*/ void Connection::onConnect(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.actor());

    if (!_rctx.error()) {
        solid_log(generic_logger, Info, &rthis << " SUCCESS");

        Event ev(make_event(GenericEventE::Message, _rctx.manager().id(rthis)));

        solid_log(generic_logger, Info, &rthis << " send resolv_message");
        if (_rctx.manager().notify(rthis.peer_actuid_, std::move(ev))) {

            rthis.sock_.device().enableNoDelay();
            // do the first read
            rthis.sock_.postRecvSome(_rctx, rthis.rbufptr_->data(), rthis.rbufptr_->capacity(), Connection::onRecv);
        } else {
            // peer has died
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

        _rctx.manager().notify(rthis.peer_actuid_, make_event(GenericEventE::Wake, rthis.makeReadEventData(_rctx, _sz)));

        if (rthis.prepareCurrentReadBuffer()) {
            rthis.sock_.postRecvSome(_rctx, rthis.rbufptr_->data(), rthis.rbufptr_->capacity(), Connection::onRecv);
        } else {
            // no free buffer
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
        _rctx.manager().notify(rthis.peer_actuid_, make_event(GenericEventE::Wake, rthis.popWriteEventData()));

        solid_log(generic_logger, Info, &rthis << " ");

        if (rthis.weventdatabegptr_) {
            rthis.sock_.postSendAll(_rctx, rthis.weventdatabegptr_->bufptr_->data(), rthis.weventdatabegptr_->bufsz_, Connection::onSend);
        }

    } else {
        solid_log(generic_logger, Error, &rthis << " postStop " << _rctx.systemError().message());
        rthis.postStop(_rctx);
    }
}

//-----------------------------------------------------------------------------
