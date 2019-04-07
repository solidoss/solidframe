// solid/frame/aio/src/aioreactor_epoll.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <fcntl.h>

#include "solid/system/common.hpp"

#if defined(SOLID_USE_EPOLL)

#include <sys/epoll.h>
#include <sys/eventfd.h>

#elif defined(SOLID_USE_KQUEUE)

#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>

#elif defined(SOLID_USE_WSAPOLL)
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#endif

#include <cerrno>
#include <cstring>
#include <deque>
#include <queue>
#include <vector>

#include "solid/system/device.hpp"
#include "solid/system/error.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/system/nanotime.hpp"
#include <mutex>
#include <thread>

#include "solid/utility/event.hpp"
#include "solid/utility/queue.hpp"
#include "solid/utility/stack.hpp"

#include "solid/frame/actor.hpp"
#include "solid/frame/common.hpp"
#include "solid/frame/service.hpp"
#include "solid/frame/timestore.hpp"

#include "solid/frame/aio/aioactor.hpp"
#include "solid/frame/aio/aiocompletion.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioreactorcontext.hpp"
#include "solid/frame/aio/aiotimer.hpp"

using namespace std;

namespace solid {
namespace frame {
namespace aio {

//=============================================================================

namespace {

void dummy_completion(CompletionHandler&, ReactorContext&)
{
}
#if defined(SOLID_USE_KQUEUE)
inline void* indexToVoid(const size_t _idx)
{
    return reinterpret_cast<void*>(_idx);
}

inline size_t voidToIndex(const void* _ptr)
{
    return reinterpret_cast<size_t>(_ptr);
}
#endif
} //namespace

//=============================================================================

typedef std::atomic<bool>   AtomicBoolT;
typedef std::atomic<size_t> AtomicSizeT;
typedef Reactor::TaskT      TaskT;

//=============================================================================
//  EventHandler
//=============================================================================

struct EventHandler : CompletionHandler {
    static void on_init(CompletionHandler&, ReactorContext&);
    static void on_completion(CompletionHandler&, ReactorContext&);

    EventHandler(ActorProxy const& _rop)
        : CompletionHandler(_rop, &on_init)
    {
    }

    void write(Reactor& _rreactor);

    bool init();
#if defined(SOLID_USE_EPOLL)
    Device::DescriptorT descriptor() const
    {
        return dev.descriptor();
    }

#elif defined(SOLID_USE_WSAPOLL)
    SocketDevice::DescriptorT descriptor() const
    {
        return dev.descriptor();
    }
#endif

private:
#if defined(SOLID_USE_EPOLL) || defined(SOLID_USE_KQUEUE)
    Device dev;
#elif defined(SOLID_USE_WSAPOLL)
    SocketDevice dev;
#endif
};

//-----------------------------------------------------------------------------

/*static*/ void EventHandler::on_init(CompletionHandler& _rch, ReactorContext& _rctx)
{
    EventHandler& rthis = static_cast<EventHandler&>(_rch);

    rthis.contextBind(_rctx);

#if defined(SOLID_USE_EPOLL)
    rthis.reactor(_rctx).addDevice(_rctx, rthis.dev, ReactorWaitRead);
#elif defined(SOLID_USE_KQUEUE)
    rthis.reactor(_rctx).addDevice(_rctx, rthis.dev, ReactorWaitUser);
#elif defined(SOLID_USE_WSAPOLL)
    rthis.reactor(_rctx).addDevice(_rctx, rthis.dev, ReactorWaitRead);
#endif
    rthis.completionCallback(&on_completion);
}

//-----------------------------------------------------------------------------

/*static*/ void EventHandler::on_completion(CompletionHandler& _rch, ReactorContext& _rctx)
{
    EventHandler& rthis = static_cast<EventHandler&>(_rch);
#if defined(SOLID_USE_EPOLL)
    uint64_t v = -1;
    ssize_t  rv;

    do {
        rv = rthis.dev.read(reinterpret_cast<char*>(&v), sizeof(v));
        solid_dbg(logger, Info, "Read from event " << rv << " value = " << v);
    } while (rv == sizeof(v));
#elif defined(SOLID_USE_WSAPOLL)
    constexpr size_t buf_sz = 64;
    char             buf[buf_sz];
    bool             can_retry;
    ErrorCodeT       err;
    rthis.dev.recv(buf, buf_sz, can_retry, err);
    rthis.contextBind(_rctx);
    rthis.reactor(_rctx).modDevice(_rctx, rthis.dev, ReactorWaitRead);
#endif
    rthis.reactor(_rctx).doCompleteEvents(_rctx);
}

//-----------------------------------------------------------------------------

bool EventHandler::init()
{
#if defined(SOLID_USE_EPOLL)
    dev = Device(eventfd(0, EFD_NONBLOCK));

    if (!dev) {
        solid_dbg(logger, Error, "eventfd: " << last_system_error().message());
        return false;
    }
#elif defined(SOLID_USE_WSAPOLL)
    ErrorCodeT err;
    err = dev.create(SocketInfo::Inet4, SocketInfo::Datagram);
    if (err)
        return false;

    err = dev.bind(SocketAddressInet4("127.0.0.1", 0));
    if (err)
        return false;

    SocketAddress sa;
    err = dev.localAddress(sa);
    if (err)
        return false;
    dev.enableLoopbackFastPath();
    err = dev.connect(SocketAddressInet4("127.0.0.1", sa.port()));
    if (err)
        return false;

    dev.makeNonBlocking();
#endif
    return true;
}

//=============================================================================
//  EventActor
//=============================================================================

class EventActor : public Actor {
public:
    EventActor()
        : eventhandler(proxy())
        , dummyhandler(proxy(), dummy_completion)
    {
        use();
    }

    void stop()
    {
        eventhandler.deactivate();
        eventhandler.unregister();
        dummyhandler.deactivate();
        dummyhandler.unregister();
    }

    template <class F>
    void post(ReactorContext& _rctx, F _f)
    {
        Actor::post(_rctx, _f);
    }

    EventHandler      eventhandler;
    CompletionHandler dummyhandler;
};

//=============================================================================

struct NewTaskStub {
    NewTaskStub(
        UniqueId const& _ruid, TaskT&& _ractptr, Service& _rsvc, Event&& _revent)
        : uid(_ruid)
        , actptr(std::move(_ractptr))
        , rsvc(_rsvc)
        , event(std::move(_revent))
    {
    }

    NewTaskStub(const NewTaskStub&) = delete;

    NewTaskStub(
        NewTaskStub&& _unts) noexcept
        : uid(_unts.uid)
        , actptr(std::move(_unts.actptr))
        , rsvc(_unts.rsvc)
        , event(std::move(_unts.event))
    {
    }

    UniqueId uid;
    TaskT    actptr;
    Service& rsvc;
    Event    event;
};

//=============================================================================

struct RaiseEventStub {
    RaiseEventStub(
        UniqueId const& _ruid, Event&& _revent)
        : uid(_ruid)
        , event(std::move(_revent))
    {
    }

    RaiseEventStub(
        UniqueId const& _ruid, Event const& _revent)
        : uid(_ruid)
        , event(_revent)
    {
    }

    RaiseEventStub(const RaiseEventStub&) = delete;
    RaiseEventStub(
        RaiseEventStub&& _uevs) noexcept
        : uid(_uevs.uid)
        , event(std::move(_uevs.event))
    {
    }

    UniqueId uid;
    Event    event;
};

//=============================================================================

struct CompletionHandlerStub {
    CompletionHandlerStub(
        CompletionHandler* _pch    = nullptr,
        const size_t       _actidx = InvalidIndex())
        : pch(_pch)
        , actidx(_actidx)
        , unique(0)
#if defined(SOLID_USE_WSAPOLL)
        , connectidx(InvalidIndex())
#endif
    {
    }

    CompletionHandler* pch;
    size_t             actidx;
    UniqueT            unique;
#if defined(SOLID_USE_WSAPOLL)
    size_t connectidx;
#endif
};

//=============================================================================

struct ActorStub {
    ActorStub()
        : unique(0)
        , psvc(nullptr)
    {
    }

    UniqueT  unique;
    Service* psvc;
    TaskT    actptr;
};

//=============================================================================

enum {
    MinEventCapacity = 32,
    MaxEventCapacity = 1024 * 64
};

//=============================================================================

struct ExecStub {
    template <class F>
    ExecStub(
        UniqueId const& _ruid, F _f, Event&& _uevent = Event())
        : actuid(_ruid)
        , exefnc(_f)
        , event(std::move(_uevent))
    {
    }

    template <class F>
    ExecStub(
        UniqueId const& _ruid, F _f, UniqueId const& _rchnuid, Event&& _uevent = Event())
        : actuid(_ruid)
        , chnuid(_rchnuid)
        , exefnc(std::move(_f))
        , event(std::move(_uevent))
    {
    }

    ExecStub(
        UniqueId const& _ruid, Event&& _uevent = Event())
        : actuid(_ruid)
        , event(std::move(_uevent))
    {
    }

    ExecStub(const ExecStub&) = delete;

    ExecStub(
        ExecStub&& _res) noexcept
        : actuid(_res.actuid)
        , chnuid(_res.chnuid)
        , event(std::move(_res.event))
    {
        std::swap(exefnc, _res.exefnc);
    }

    UniqueId                actuid;
    UniqueId                chnuid;
    Reactor::EventFunctionT exefnc;
    Event                   event;
};

//=============================================================================

typedef std::vector<NewTaskStub>    NewTaskVectorT;
typedef std::vector<RaiseEventStub> RaiseEventVectorT;

#if defined(SOLID_USE_EPOLL)

typedef std::vector<epoll_event> EventVectorT;

#elif defined(SOLID_USE_KQUEUE)

typedef std::vector<struct kevent> EventVectorT;
#elif defined(SOLID_USE_WSAPOLL)
struct PollStub : WSAPOLLFD {
    PollStub()
    {
        clear();
    }
    void clear()
    {
        this->fd = SocketDevice::invalidDescriptor();
        this->events = 0;
        this->revents = 0;
    }
};
using EventVectorT = std::vector<PollStub>;

#endif

using CompletionHandlerDequeT = std::deque<CompletionHandlerStub>;
using UidVectorT              = std::vector<UniqueId>;
using ActorDequeT             = std::deque<ActorStub>;
using ExecQueueT              = Queue<ExecStub>;
using SizeStackT              = Stack<size_t>;
using TimeStoreT              = TimeStore<size_t>;
using SizeTVectorT            = std::vector<size_t>;

//=============================================================================
//  Reactor::Data
//=============================================================================
struct Reactor::Data {
    Data(

        )
        : reactor_fd(-1)
        , running(false)
        , crtpushtskvecidx(0)
        , crtraisevecidx(0)
        , crtpushvecsz(0)
        , crtraisevecsz(0)
        , devcnt(0)
        , actcnt(0)
        , timestore(MinEventCapacity)
    {
    }
#if defined(SOLID_USE_EPOLL)
    int computeWaitTimeMilliseconds(NanoTime const& _rcrt) const
    {

        if (!exeq.empty()) {
            return 0;
        }

        if (timestore.size() != 0u) {

            if (_rcrt < timestore.next()) {

                const int64_t maxwait = 1000 * 60 * 10; //ten minutes
                int64_t       diff    = 0;
                const auto    crt_tp  = _rcrt.timePointCast<std::chrono::steady_clock::time_point>();
                const auto    next_tp = timestore.next().timePointCast<std::chrono::steady_clock::time_point>();
                diff                  = std::chrono::duration_cast<std::chrono::milliseconds>(next_tp - crt_tp).count();

                if (diff > maxwait) {
                    return maxwait;
                }
                return static_cast<int>(diff);
            }

            return 0;
        }
        return -1;
    }
#elif defined(SOLID_USE_KQUEUE)
    NanoTime computeWaitTimeMilliseconds(NanoTime const& _rcrt) const
    {

        if (exeq.size()) {
            return NanoTime();
        } else if (timestore.size()) {

            if (_rcrt < timestore.next()) {
                const auto crt_tp = _rcrt.timePointCast<std::chrono::steady_clock::time_point>();
                const auto next_tp = timestore.next().timePointCast<std::chrono::steady_clock::time_point>();
                const auto delta = next_tp - crt_tp;

                if (delta <= std::chrono::minutes(10)) {
                    return delta;
                } else {
                    return std::chrono::minutes(10);
                }

            } else {
                return NanoTime();
            }
        } else {
            return NanoTime::maximum;
        }
    }
#elif defined(SOLID_USE_WSAPOLL)
    int computeWaitTimeMilliseconds(NanoTime const& _rcrt) const
    {
        if (exeq.size()) {
            return 0;
        } else if (timestore.size()) {

            if (_rcrt < timestore.next()) {

                constexpr int64_t maxwait = 1000 * 60 * 10; //ten minutes
                int64_t diff = 0;
                const auto crt_tp = _rcrt.timePointCast<std::chrono::steady_clock::time_point>();
                const auto next_tp = timestore.next().timePointCast<std::chrono::steady_clock::time_point>();
                diff = std::chrono::duration_cast<std::chrono::milliseconds>(next_tp - crt_tp).count();

                if (diff > maxwait) {
                    return connectvec.empty() ? maxwait : 1000; //wait 1 sec when connect opperations are in place
                } else {
                    return connectvec.empty() ? static_cast<int>(diff) : 1000;
                }

            } else {
                return 0;
            }

        } else {
            return connectvec.empty() ? -1 : 1000;
        }
    }
#endif

    UniqueId dummyCompletionHandlerUid() const
    {
        const size_t idx = eventact.dummyhandler.idxreactor;
        return UniqueId(idx, chdq[idx].unique);
    }

    int                     reactor_fd;
    AtomicBoolT             running;
    size_t                  crtpushtskvecidx;
    size_t                  crtraisevecidx;
    AtomicSizeT             crtpushvecsz;
    AtomicSizeT             crtraisevecsz;
    size_t                  devcnt;
    size_t                  actcnt;
    TimeStoreT              timestore;
    mutex                   mtx;
    EventVectorT            eventvec;
    NewTaskVectorT          pushtskvec[2];
    RaiseEventVectorT       raisevec[2];
    EventActor              eventact;
    CompletionHandlerDequeT chdq;
    UidVectorT              freeuidvec;
    ActorDequeT             actdq;
    ExecQueueT              exeq;
    SizeStackT              chposcache;
#if defined(SOLID_USE_WSAPOLL)
    SizeTVectorT connectvec;
#endif
};
//-----------------------------------------------------------------------------
void EventHandler::write(Reactor& _rreactor)
{
#if defined(SOLID_USE_EPOLL)
    const uint64_t v = 1;
    dev.write(reinterpret_cast<const char*>(&v), sizeof(v));
#elif defined(SOLID_USE_KQUEUE)
    struct kevent ev;

    solid_dbg(logger, Verbose, "trigger user event");

    EV_SET(&ev, dev.descriptor(), EVFILT_USER, 0, NOTE_TRIGGER, 0, indexToVoid(this->indexWithinReactor()));

    if (kevent(_rreactor.impl_->reactor_fd, &ev, 1, nullptr, 0, nullptr)) {
        solid_dbg(logger, Error, "kevent: " << last_system_error().message());
        solid_assert(false);
    }
#elif defined(SOLID_USE_WSAPOLL)
    const uint32_t v = 1;
    bool can_retry;
    ErrorCodeT err;
    dev.send(reinterpret_cast<const char*>(&v), sizeof(v), can_retry, err);
#endif
}
//=============================================================================
//-----------------------------------------------------------------------------
//  Reactor
//-----------------------------------------------------------------------------

Reactor::Reactor(
    SchedulerBase& _rsched,
    const size_t   _idx)
    : ReactorBase(_rsched, _idx)
    , impl_(make_pimpl<Data>())
{
    solid_dbg(logger, Verbose, "");
}

//-----------------------------------------------------------------------------

Reactor::~Reactor()
{
    solid_dbg(logger, Verbose, "");
}

//-----------------------------------------------------------------------------

bool Reactor::start()
{

    solid_dbg(logger, Verbose, "");

    doStoreSpecific();

#if defined(SOLID_USE_EPOLL)
    impl_->reactor_fd = epoll_create(MinEventCapacity);
    if (impl_->reactor_fd < 0) {
        solid_dbg(logger, Error, "reactor create: " << last_system_error().message());
        return false;
    }
#elif defined(SOLID_USE_KQUEUE)
    impl_->reactor_fd = kqueue();
    if (impl_->reactor_fd < 0) {
        solid_dbg(logger, Error, "reactor create: " << last_system_error().message());
        return false;
    }
#endif

    if (!impl_->eventact.eventhandler.init()) {
        return false;
    }

    impl_->actdq.push_back(ActorStub());
    impl_->actdq.back().actptr = &impl_->eventact;

    popUid(*impl_->actdq.back().actptr);

    impl_->eventact.registerCompletionHandlers();

    impl_->eventvec.resize(MinEventCapacity);
    impl_->eventvec.resize(impl_->eventvec.capacity());
    impl_->running = true;

    return true;
}

//-----------------------------------------------------------------------------

/*virtual*/ bool Reactor::raise(UniqueId const& _ractuid, Event&& _uevent)
{
    solid_dbg(logger, Verbose, (void*)this << " uid = " << _ractuid.index << ',' << _ractuid.unique << " event = " << _uevent);
    bool   rv         = true;
    size_t raisevecsz = 0;
    {
        lock_guard<std::mutex> lock(impl_->mtx);

        impl_->raisevec[impl_->crtraisevecidx].emplace_back(_ractuid, std::move(_uevent));
        raisevecsz           = impl_->raisevec[impl_->crtraisevecidx].size();
        impl_->crtraisevecsz = raisevecsz;
    }
    if (raisevecsz == 1) {
        impl_->eventact.eventhandler.write(*this);
    }
    return rv;
}

//-----------------------------------------------------------------------------

/*virtual*/ bool Reactor::raise(UniqueId const& _ractuid, const Event& _revent)
{
    solid_dbg(logger, Verbose, (void*)this << " uid = " << _ractuid.index << ',' << _ractuid.unique << " event = " << _revent);
    bool   rv         = true;
    size_t raisevecsz = 0;
    {
        lock_guard<std::mutex> lock(impl_->mtx);

        impl_->raisevec[impl_->crtraisevecidx].push_back(RaiseEventStub(_ractuid, _revent));
        raisevecsz           = impl_->raisevec[impl_->crtraisevecidx].size();
        impl_->crtraisevecsz = raisevecsz;
    }
    if (raisevecsz == 1) {
        impl_->eventact.eventhandler.write(*this);
    }
    return rv;
}

//-----------------------------------------------------------------------------

/*virtual*/ void Reactor::stop()
{
    solid_dbg(logger, Verbose, "");
    impl_->running = false;
    impl_->eventact.eventhandler.write(*this);
}

//-----------------------------------------------------------------------------

//Called from outside reactor's thread
bool Reactor::push(TaskT&& _ract, Service& _rsvc, Event&& _uevent)
{
    solid_dbg(logger, Verbose, (void*)this);
    bool   rv        = true;
    size_t pushvecsz = 0;
    {
        lock_guard<std::mutex> lock(impl_->mtx);
        const UniqueId         uid = this->popUid(*_ract);

        solid_dbg(logger, Verbose, (void*)this << " uid = " << uid.index << ',' << uid.unique << " event = " << _uevent);

        impl_->pushtskvec[impl_->crtpushtskvecidx].push_back(NewTaskStub(uid, std::move(_ract), _rsvc, std::move(_uevent)));
        pushvecsz           = impl_->pushtskvec[impl_->crtpushtskvecidx].size();
        impl_->crtpushvecsz = pushvecsz;
    }

    if (pushvecsz == 1) {
        impl_->eventact.eventhandler.write(*this);
    }
    return rv;
}

//-----------------------------------------------------------------------------

/*NOTE:

    We MUST call doCompleteEvents before doCompleteExec
    because we must ensure that on successful Event notification from
    frame::Manager, the Actor actually receives the Event before stopping.

    For that, on Actor::postStop, we mark the Actor as “unable to
    receive any notifications” (we do not unregister it, because the
    Actor may want access to it’s mutex on events already waiting
    to be delivered to the actor.

*/
void Reactor::run()
{
    solid_dbg(logger, Info, "<enter>");
    long     selcnt;
    bool     running = true;
    NanoTime crttime;
    int      waitmsec;
    NanoTime waittime;

    while (running) {
        crttime = std::chrono::steady_clock::now();

        crtload = impl_->actcnt + impl_->devcnt + impl_->exeq.size();
#if defined(SOLID_USE_EPOLL)
        waitmsec = impl_->computeWaitTimeMilliseconds(crttime);

        solid_dbg(logger, Verbose, "epoll_wait msec = " << waitmsec);

        selcnt = epoll_wait(impl_->reactor_fd, impl_->eventvec.data(), static_cast<int>(impl_->eventvec.size()), waitmsec);
#elif defined(SOLID_USE_KQUEUE)
        waittime = impl_->computeWaitTimeMilliseconds(crttime);

        solid_dbg(logger, Verbose, "kqueue msec = " << waittime.seconds() << ':' << waittime.nanoSeconds());

        selcnt = kevent(impl_->reactor_fd, nullptr, 0, impl_->eventvec.data(), static_cast<int>(impl_->eventvec.size()), waittime != NanoTime::maximum ? &waittime : nullptr);
#elif defined(SOLID_USE_WSAPOLL)
        waitmsec = impl_->computeWaitTimeMilliseconds(crttime);
        solid_dbg(logger, Verbose, "wsapoll wait msec = " << waitmsec);
        selcnt = WSAPoll(impl_->eventvec.data(), impl_->eventvec.size(), waitmsec);
#endif
        crttime = std::chrono::steady_clock::now();

#if defined(SOLID_USE_WSAPOLL)
        if (selcnt > 0 || impl_->connectvec.size()) {
#else
        if (selcnt > 0) {
#endif
            crtload += selcnt;
            doCompleteIo(crttime, selcnt);
        } else if (selcnt < 0 && errno != EINTR) {
            solid_dbg(logger, Error, "epoll_wait errno  = " << last_system_error().message());
            running = false;
        } else {
            solid_dbg(logger, Verbose, "epoll_wait done");
        }

        crttime = std::chrono::steady_clock::now();
        doCompleteTimer(crttime);

        crttime = std::chrono::steady_clock::now();
        doCompleteEvents(crttime); //See NOTE above
        doCompleteExec(crttime);

        running = impl_->running || (impl_->actcnt != 0) || !impl_->exeq.empty();
    }

    impl_->eventact.stop();
    doClearSpecific();
    solid_dbg(logger, Info, "<exit>");
    (void)waitmsec;
    (void)waittime;
} // namespace aio

//-----------------------------------------------------------------------------
#if defined(SOLID_USE_EPOLL)
inline ReactorEventsE systemEventsToReactorEvents(const uint32_t _events)
{
    ReactorEventsE retval = ReactorEventNone;

    switch (_events) {
    case EPOLLIN:
        retval = ReactorEventRecv;
        break;
    case EPOLLOUT:
        retval = ReactorEventSend;
        break;
    case EPOLLOUT | EPOLLIN:
        retval = ReactorEventRecvSend;
        break;
    case EPOLLPRI:
        retval = ReactorEventOOB;
        break;
    case EPOLLOUT | EPOLLPRI:
        retval = ReactorEventOOBSend;
        break;
    case EPOLLERR:
        retval = ReactorEventError;
        break;
    case EPOLLHUP:
    case EPOLLHUP | EPOLLOUT:
    case EPOLLHUP | EPOLLIN:
    case EPOLLHUP | EPOLLERR | EPOLLIN | EPOLLOUT:
    case EPOLLIN | EPOLLOUT | EPOLLHUP:
    case EPOLLERR | EPOLLOUT | EPOLLIN:
    case EPOLLERR | EPOLLOUT | EPOLLHUP:
    case EPOLLERR | EPOLLIN | EPOLLHUP:
    case EPOLLERR | EPOLLIN:
    case EPOLLERR | EPOLLOUT:
        retval = ReactorEventHangup;
        break;
#ifdef SOLID_USE_EPOLLRDHUP
    case EPOLLRDHUP:
        retval = ReactorEventRecvHangup;
        break;
#endif
    default:
        solid_assert(false);
        break;
    }
    return retval;
}

#elif defined(SOLID_USE_KQUEUE)

inline constexpr ReactorEventsE systemEventsToReactorEvents(const unsigned short _flags, const short _filter)
{
    ReactorEventsE retval = ReactorEventNone;
    if (_flags & (EV_EOF | EV_ERROR)) {
        return ReactorEventHangup;
    }
    if (_filter == EVFILT_READ) {
        return ReactorEventRecv;
    } else if (_filter == EVFILT_WRITE) {
        return ReactorEventSend;
    } else if (_filter == EVFILT_USER) {
        return ReactorEventRecv;
    }

    return retval;
}
#elif defined(SOLID_USE_WSAPOLL)
inline ReactorEventsE systemEventsToReactorEvents(const uint32_t _events, decltype(WSAPOLLFD::events)& _revs)
{
    if (_events & POLLERR) {
        _revs = 0;
        return ReactorEventError;
    }
    if (_events & POLLHUP) {
        _revs = 0;
        return ReactorEventHangup;
    }
    ReactorEventsE retval = ReactorEventNone;
    switch (_events) {
    case POLLPRI:
        retval = ReactorEventOOB;
        break;
    case POLLRDNORM:
        retval = ReactorEventRecv;
        _revs &= ~(POLLRDNORM);
        break;
    case POLLWRNORM:
        retval = ReactorEventSend;
        _revs &= ~(POLLWRNORM);
        break;
    case POLLRDNORM | POLLWRNORM:
        retval = ReactorEventRecvSend;
        _revs = 0;
        break;
    default:
        solid_assert(false);
        break;
    }
    return retval;
}
#endif

//-----------------------------------------------------------------------------
#if defined(SOLID_USE_EPOLL)
inline uint32_t reactorRequestsToSystemEvents(const ReactorWaitRequestsE _requests)
{
    uint32_t evs = 0;
    switch (_requests) {
    case ReactorWaitNone:
        break;
    case ReactorWaitRead:
        evs = EPOLLET | EPOLLIN;
        break;
    case ReactorWaitWrite:
        evs = EPOLLET | EPOLLOUT;
        break;
    case ReactorWaitReadOrWrite:
        evs = EPOLLET | EPOLLIN | EPOLLOUT;
        break;
    default:
        solid_assert(false);
    }
    return evs;
}
#elif defined(SOLID_USE_WSAPOLL)
inline uint32_t reactorRequestsToSystemEvents(const ReactorWaitRequestsE _requests)
{
    uint32_t evs = 0;
    switch (_requests) {
    case ReactorWaitNone:
        break;
    case ReactorWaitRead:
        evs = POLLRDNORM;
        break;
    case ReactorWaitWrite:
        evs = POLLWRNORM;
        break;
    case ReactorWaitReadOrWrite:
        evs = POLLWRNORM | POLLRDNORM;
        break;
    default:
        solid_assert(false);
    }
    return evs;
}
#endif
//-----------------------------------------------------------------------------

inline UniqueId Reactor::actorUid(ReactorContext const& _rctx) const
{
    return UniqueId(_rctx.actor_index_, impl_->actdq[_rctx.actor_index_].unique);
}

//-----------------------------------------------------------------------------

Service& Reactor::service(ReactorContext const& _rctx) const
{
    return *impl_->actdq[_rctx.actor_index_].psvc;
}

//-----------------------------------------------------------------------------

Actor& Reactor::actor(ReactorContext const& _rctx) const
{
    return *impl_->actdq[_rctx.actor_index_].actptr;
}

//-----------------------------------------------------------------------------

CompletionHandler* Reactor::completionHandler(ReactorContext const& _rctx) const
{
    return impl_->chdq[_rctx.channel_index_].pch;
}

//-----------------------------------------------------------------------------

void Reactor::doPost(ReactorContext& _rctx, Reactor::EventFunctionT&& _revfn, Event&& _uev)
{
    solid_dbg(logger, Verbose, "exeq " << impl_->exeq.size());
    impl_->exeq.push(ExecStub(actorUid(_rctx), std::move(_uev)));
    impl_->exeq.back().exefnc = std::move(_revfn);
    impl_->exeq.back().chnuid = impl_->dummyCompletionHandlerUid();
}

//-----------------------------------------------------------------------------

void Reactor::doPost(ReactorContext& _rctx, Reactor::EventFunctionT&& _revfn, Event&& _uev, CompletionHandler const& _rch)
{
    solid_dbg(logger, Verbose, "exeq " << impl_->exeq.size() << ' ' << &_rch);
    impl_->exeq.push(ExecStub(_rctx.actorUid(), std::move(_uev)));
    impl_->exeq.back().exefnc = std::move(_revfn);
    impl_->exeq.back().chnuid = UniqueId(_rch.idxreactor, impl_->chdq[_rch.idxreactor].unique);
}

//-----------------------------------------------------------------------------

/*static*/ void Reactor::stop_actor(ReactorContext& _rctx, Event&&)
{
    Reactor& rthis = _rctx.reactor();
    rthis.doStopActor(_rctx);
}

//-----------------------------------------------------------------------------

/*static*/ void Reactor::stop_actor_repost(ReactorContext& _rctx, Event&&)
{
    Reactor& rthis = _rctx.reactor();

    rthis.impl_->exeq.push(ExecStub(_rctx.actorUid()));
    rthis.impl_->exeq.back().exefnc = &stop_actor;
    rthis.impl_->exeq.back().chnuid = rthis.impl_->dummyCompletionHandlerUid();
}

//-----------------------------------------------------------------------------

/*NOTE:
    We do not stop the actor rightaway - we make sure that any
    pending Events are delivered to the actor before we stop
*/
void Reactor::postActorStop(ReactorContext& _rctx)
{
    impl_->exeq.push(ExecStub(_rctx.actorUid()));
    impl_->exeq.back().exefnc = &stop_actor_repost;
    impl_->exeq.back().chnuid = impl_->dummyCompletionHandlerUid();
}

//-----------------------------------------------------------------------------

void Reactor::doStopActor(ReactorContext& _rctx)
{
    ActorStub& ras = this->impl_->actdq[_rctx.actor_index_];

    this->stopActor(*ras.actptr, ras.psvc->manager());

    ras.actptr.clear();
    ras.psvc = nullptr;
    ++ras.unique;
    --this->impl_->actcnt;
    this->impl_->freeuidvec.push_back(UniqueId(_rctx.actor_index_, ras.unique));
}

//-----------------------------------------------------------------------------

void Reactor::doCompleteIo(NanoTime const& _rcrttime, const size_t _sz)
{
    ReactorContext ctx(*this, _rcrttime);

    solid_dbg(logger, Verbose, "selcnt = " << _sz);

#if defined(SOLID_USE_EPOLL)
    for (size_t i = 0; i < _sz; ++i) {
        epoll_event&           rev = impl_->eventvec[i];
        CompletionHandlerStub& rch = impl_->chdq[rev.data.u64];

        ctx.reactor_event_ = systemEventsToReactorEvents(rev.events);
        ctx.channel_index_ = rev.data.u64;
#elif defined(SOLID_USE_KQUEUE)
    for (size_t i = 0; i < _sz; ++i) {
        struct kevent& rev = impl_->eventvec[i];
        CompletionHandlerStub& rch = impl_->chdq[voidToIndex(rev.udata)];

        ctx.reactor_event_ = systemEventsToReactorEvents(rev.flags, rev.filter);
        ctx.channel_index_ = voidToIndex(rev.udata);
#elif defined(SOLID_USE_WSAPOLL)
    const size_t vecsz = impl_->eventvec.size();
    size_t evcnt = _sz;
    for (size_t i = 0; i < vecsz; ++i) {
        if (evcnt == 0)
            break;

        WSAPOLLFD& rev = impl_->eventvec[i];
        if (rev.revents == 0 || rev.revents & POLLNVAL)
            continue;
        --evcnt;
        CompletionHandlerStub& rch = impl_->chdq[i];
        ctx.reactor_event_ = systemEventsToReactorEvents(rev.revents, rev.events);
        ctx.channel_index_ = i;
        if (rch.connectidx != InvalidIndex()) {
            //we have events on a connecting socket
            //so we remove it from connect waiting list
            remConnect(ctx);
        }
#endif
        ctx.actor_index_ = rch.actidx;

        rch.pch->handleCompletion(ctx);
        ctx.clearError();
    }
#if defined(SOLID_USE_WSAPOLL)
    for (size_t j = 0; j < impl_->connectvec.size();) {
        const size_t           i   = impl_->connectvec[j];
        WSAPOLLFD&             rev = impl_->eventvec[i];
        CompletionHandlerStub& rch = impl_->chdq[i];

        if (SocketDevice::error(rev.fd)) {

            ctx.reactor_event_ = systemEventsToReactorEvents(POLLERR, rev.events);
            ctx.channel_index_ = i;
            ctx.actor_index_   = rch.actidx;

            remConnect(ctx);

            rch.pch->handleCompletion(ctx);
            ctx.clearError();
        } else {
            ++j;
        }
    }
#endif
}

//-----------------------------------------------------------------------------
#if defined(SOLID_USE_WSAPOLL)
void Reactor::addConnect(ReactorContext& _rctx)
{
    CompletionHandlerStub& rch = impl_->chdq[_rctx.channel_index_];
    rch.connectidx             = impl_->connectvec.size();
    impl_->connectvec.emplace_back(_rctx.channel_index_);
}

void Reactor::remConnect(ReactorContext& _rctx)
{
    CompletionHandlerStub& rch                       = impl_->chdq[_rctx.channel_index_];
    impl_->connectvec[rch.connectidx]                = impl_->connectvec.back();
    impl_->chdq[impl_->connectvec.back()].connectidx = rch.connectidx;
    rch.connectidx                                   = InvalidIndex();
    impl_->connectvec.pop_back();
}
#endif
//-----------------------------------------------------------------------------

struct ChangeTimerIndexCallback {
    Reactor& r;
    ChangeTimerIndexCallback(Reactor& _r)
        : r(_r)
    {
    }

    void operator()(const size_t _chidx, const size_t _newidx, const size_t _oldidx) const
    {
        r.doUpdateTimerIndex(_chidx, _newidx, _oldidx);
    }
};

struct TimerCallback {
    Reactor&        r;
    ReactorContext& rctx;
    TimerCallback(Reactor& _r, ReactorContext& _rctx)
        : r(_r)
        , rctx(_rctx)
    {
    }

    void operator()(const size_t _tidx, const size_t _chidx) const
    {
        r.onTimer(rctx, _tidx, _chidx);
    }
};

void Reactor::onTimer(ReactorContext& _rctx, const size_t /*_tidx*/, const size_t _chidx)
{
    CompletionHandlerStub& rch = impl_->chdq[_chidx];

    _rctx.reactor_event_ = ReactorEventTimer;
    _rctx.channel_index_ = _chidx;
    _rctx.actor_index_   = rch.actidx;

    rch.pch->handleCompletion(_rctx);
    _rctx.clearError();
}

//-----------------------------------------------------------------------------

void Reactor::doCompleteTimer(NanoTime const& _rcrttime)
{
    ReactorContext ctx(*this, _rcrttime);
    TimerCallback  tcbk(*this, ctx);
    impl_->timestore.pop(_rcrttime, tcbk, ChangeTimerIndexCallback(*this));
}

//-----------------------------------------------------------------------------

void Reactor::doCompleteExec(NanoTime const& _rcrttime)
{
    ReactorContext ctx(*this, _rcrttime);
    size_t         sz = impl_->exeq.size();

    while ((sz--) != 0) {

        solid_dbg(logger, Verbose, sz << " qsz = " << impl_->exeq.size());

        ExecStub&              rexe(impl_->exeq.front());
        ActorStub&             ras(impl_->actdq[static_cast<size_t>(rexe.actuid.index)]);
        CompletionHandlerStub& rcs(impl_->chdq[static_cast<size_t>(rexe.chnuid.index)]);

        if (ras.unique == rexe.actuid.unique && rcs.unique == rexe.chnuid.unique) {
            ctx.clearError();
            ctx.channel_index_ = static_cast<size_t>(rexe.chnuid.index);
            ctx.actor_index_   = static_cast<size_t>(rexe.actuid.index);
            rexe.exefnc(ctx, std::move(rexe.event));
        }
        impl_->exeq.pop();
    }
}

//-----------------------------------------------------------------------------

void Reactor::doCompleteEvents(NanoTime const& _rcrttime)
{
    ReactorContext ctx(*this, _rcrttime);
    doCompleteEvents(ctx);
}

//-----------------------------------------------------------------------------

void Reactor::doCompleteEvents(ReactorContext const& _rctx)
{
    solid_dbg(logger, Verbose, "");

    if (impl_->crtpushvecsz != 0u || impl_->crtraisevecsz != 0u) {
        size_t crtpushvecidx;
        size_t crtraisevecidx;
        {
            lock_guard<std::mutex> lock(impl_->mtx);

            crtpushvecidx  = impl_->crtpushtskvecidx;
            crtraisevecidx = impl_->crtraisevecidx;

            impl_->crtpushtskvecidx = ((crtpushvecidx + 1) & 1);
            impl_->crtraisevecidx   = ((crtraisevecidx + 1) & 1);

            for (const auto& v : impl_->freeuidvec) {
                this->pushUid(v);
            }
            impl_->freeuidvec.clear();

            impl_->crtpushvecsz = impl_->crtraisevecsz = 0;
        }

        NewTaskVectorT&    crtpushvec  = impl_->pushtskvec[crtpushvecidx];
        RaiseEventVectorT& crtraisevec = impl_->raisevec[crtraisevecidx];

        ReactorContext ctx(_rctx);

        impl_->actcnt += crtpushvec.size();

        solid_dbg(logger, Verbose, impl_->exeq.size());

        for (auto& rnewact : crtpushvec) {
            if (rnewact.uid.index >= impl_->actdq.size()) {
                impl_->actdq.resize(static_cast<size_t>(rnewact.uid.index + 1));
            }
            ActorStub& ras = impl_->actdq[static_cast<size_t>(rnewact.uid.index)];

            solid_assert(ras.unique == rnewact.uid.unique);

            {
                //NOTE: we must lock the mutex of the actor
                //in order to ensure that actor is fully registered onto the manager

                lock_guard<std::mutex> lock(rnewact.rsvc.mutex(*rnewact.actptr));
            }

            ras.actptr = std::move(rnewact.actptr);
            ras.psvc   = &rnewact.rsvc;

            ctx.clearError();
            ctx.channel_index_ = InvalidIndex();
            ctx.actor_index_   = static_cast<size_t>(rnewact.uid.index);

            ras.actptr->registerCompletionHandlers();

            impl_->exeq.push(ExecStub(rnewact.uid, &call_actor_on_event, impl_->dummyCompletionHandlerUid(), std::move(rnewact.event)));
        }

        solid_dbg(logger, Verbose, impl_->exeq.size());
        crtpushvec.clear();

        for (auto& revent : crtraisevec) {
            impl_->exeq.push(ExecStub(revent.uid, &call_actor_on_event, impl_->dummyCompletionHandlerUid(), std::move(revent.event)));
        }

        solid_dbg(logger, Verbose, impl_->exeq.size());

        crtraisevec.clear();
    }
}

//-----------------------------------------------------------------------------

/*static*/ void Reactor::call_actor_on_event(ReactorContext& _rctx, Event&& _uevent)
{
    _rctx.actor().onEvent(_rctx, std::move(_uevent));
}

//-----------------------------------------------------------------------------

/*static*/ void Reactor::increase_event_vector_size(ReactorContext& _rctx, Event&& /*_rev*/)
{
    Reactor& rthis = _rctx.reactor();

    solid_dbg(logger, Info, "" << rthis.impl_->devcnt << " >= " << rthis.impl_->eventvec.size());

    if (rthis.impl_->devcnt >= rthis.impl_->eventvec.size()) {
        rthis.impl_->eventvec.resize(rthis.impl_->devcnt);
        rthis.impl_->eventvec.resize(rthis.impl_->eventvec.capacity());
    }
}

//-----------------------------------------------------------------------------

bool Reactor::addDevice(ReactorContext& _rctx, Device const& _rsd, const ReactorWaitRequestsE _req)
{
    solid_dbg(logger, Info, _rsd.descriptor());

    //solid_assert(_rctx.channel_index_ == _rch.idxreactor);

#if defined(SOLID_USE_EPOLL)
    epoll_event ev;
    ev.data.u64 = _rctx.channel_index_;
    ev.events   = reactorRequestsToSystemEvents(_req);

    if (epoll_ctl(impl_->reactor_fd, EPOLL_CTL_ADD, _rsd.Device::descriptor(), &ev) != 0) {
        solid_dbg(logger, Error, "epoll_ctl: " << last_system_error().message());
        solid_throw("epoll_ctl");
        return false;
    }
    ++impl_->devcnt;
    if (impl_->devcnt == (impl_->eventvec.size() + 1)) {
        impl_->eventact.post(_rctx, &Reactor::increase_event_vector_size);
    }
#elif defined(SOLID_USE_KQUEUE)
    int read_flags = EV_ADD;
    int write_flags = EV_ADD;

    switch (_req) {
    case ReactorWaitNone:
        read_flags |= EV_DISABLE;
        write_flags |= EV_DISABLE;
    case ReactorWaitRead:
        read_flags |= EV_ENABLE;
        write_flags |= EV_DISABLE;
        break;
    case ReactorWaitWrite:
        read_flags |= EV_DISABLE;
        write_flags |= EV_ENABLE;
        break;
    case ReactorWaitReadOrWrite:
        read_flags |= EV_ENABLE;
        write_flags |= EV_ENABLE;
        break;
    case ReactorWaitUser: {
        struct kevent ev;
        EV_SET(&ev, _rsd.descriptor(), EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, indexToVoid(_rctx.channel_index_));
        if (kevent(impl_->reactor_fd, &ev, 1, nullptr, 0, nullptr)) {
            solid_dbg(logger, Error, "kevent: " << last_system_error().message());
            solid_assert(false);
            return false;
        } else {
            ++impl_->devcnt;
            if (impl_->devcnt == (impl_->eventvec.size() + 1)) {
                impl_->eventact.post(_rctx, &Reactor::increase_event_vector_size);
            }
        }
        return true;
    }
    default:
        solid_assert(false);
        return false;
    }

    struct kevent ev[2];
    EV_SET(&ev[0], _rsd.descriptor(), EVFILT_READ, read_flags | EV_CLEAR, 0, 0, indexToVoid(_rctx.channel_index_));
    EV_SET(&ev[1], _rsd.descriptor(), EVFILT_WRITE, write_flags | EV_CLEAR, 0, 0, indexToVoid(_rctx.channel_index_));
    if (kevent(impl_->reactor_fd, ev, 2, nullptr, 0, nullptr)) {
        solid_dbg(logger, Error, "kevent: " << last_system_error().message());
        solid_assert(false);
        return false;
    } else {
        ++impl_->devcnt;
        if (impl_->devcnt == (impl_->eventvec.size() + 1)) {
            impl_->eventact.post(_rctx, &Reactor::increase_event_vector_size);
        }
    }
#elif defined(SOLID_USE_WSAPOLL)
    if (_rctx.channel_index_ >= impl_->eventvec.size()) {
        impl_->eventvec.resize(_rctx.channel_index_ + 1);
    }

    impl_->eventvec[_rctx.channel_index_].fd = reinterpret_cast<SocketDevice::DescriptorT>(_rsd.descriptor());
    impl_->eventvec[_rctx.channel_index_].events = reactorRequestsToSystemEvents(_req);
#endif
    return true;
}

//-----------------------------------------------------------------------------

bool Reactor::modDevice(ReactorContext& _rctx, Device const& _rsd, const ReactorWaitRequestsE _req)
{
    solid_dbg(logger, Info, _rsd.descriptor());
#if defined(SOLID_USE_EPOLL)
    epoll_event ev;

    ev.data.u64 = _rctx.channel_index_;
    ev.events   = reactorRequestsToSystemEvents(_req);

    if (epoll_ctl(impl_->reactor_fd, EPOLL_CTL_MOD, _rsd.Device::descriptor(), &ev) != 0) {
        solid_dbg(logger, Error, "epoll_ctl: " << last_system_error().message());
        solid_throw("epoll_ctl");
        return false;
    }
#elif defined(SOLID_USE_KQUEUE)
    int read_flags = 0;
    int write_flags = 0;
    struct kevent ev[2];

    switch (_req) {
    case ReactorWaitNone:
        read_flags |= EV_DISABLE;
        write_flags |= EV_DISABLE;
    case ReactorWaitRead:
        read_flags |= (EV_ENABLE | EV_CLEAR);
        write_flags |= EV_DISABLE;
        break;
    case ReactorWaitWrite:
        read_flags |= EV_DISABLE;
        write_flags |= (EV_ENABLE | EV_CLEAR);
        break;
    case ReactorWaitReadOrWrite:
        read_flags |= (EV_ENABLE | EV_CLEAR);
        write_flags |= (EV_ENABLE | EV_CLEAR);
        break;
    case ReactorWaitUser: {

        struct kevent ev;

        EV_SET(&ev, _rsd.descriptor(), EVFILT_USER, EV_ADD, 0, 0, indexToVoid(_rctx.channel_index_));

        if (kevent(impl_->reactor_fd, &ev, 1, nullptr, 0, nullptr)) {
            solid_dbg(logger, Error, "kevent: " << last_system_error().message());
            solid_assert(false);
            return false;
        } else {
            ++impl_->devcnt;
            if (impl_->devcnt == (impl_->eventvec.size() + 1)) {
                impl_->eventact.post(_rctx, &Reactor::increase_event_vector_size);
            }
        }
        return true;
    }
    default:
        solid_assert(false);
        return false;
    }

    EV_SET(&ev[0], _rsd.descriptor(), EVFILT_READ, read_flags, 0, 0, indexToVoid(_rctx.channel_index_));
    EV_SET(&ev[1], _rsd.descriptor(), EVFILT_WRITE, write_flags, 0, 0, indexToVoid(_rctx.channel_index_));

    if (kevent(impl_->reactor_fd, ev, 2, nullptr, 0, nullptr)) {
        solid_dbg(logger, Error, "kevent: " << last_system_error().message());
        solid_assert(false);
        return false;
    } else {
        ++impl_->devcnt;
        if (impl_->devcnt == (impl_->eventvec.size() + 1)) {
            impl_->eventact.post(_rctx, &Reactor::increase_event_vector_size);
        }
    }
#elif defined(SOLID_USE_WSAPOLL)
    if (_req != ReactorWaitNone) {
        impl_->eventvec[_rctx.channel_index_].events |= reactorRequestsToSystemEvents(_req);
    } else {
        impl_->eventvec[_rctx.channel_index_].events = reactorRequestsToSystemEvents(_req);
    }
#endif
    return true;
}

//-----------------------------------------------------------------------------

bool Reactor::remDevice(CompletionHandler const& _rch, Device const& _rsd)
{
    solid_dbg(logger, Info, _rsd.descriptor());
#if defined(SOLID_USE_EPOLL)
    epoll_event ev;

    if (!_rsd) {
        return false;
    }

    if (epoll_ctl(impl_->reactor_fd, EPOLL_CTL_DEL, _rsd.Device::descriptor(), &ev) != 0) {
        solid_dbg(logger, Error, "epoll_ctl: " << last_system_error().message());
        solid_throw("epoll_ctl");
        return false;
    }

    --impl_->devcnt;
#elif defined(SOLID_USE_KQUEUE)
    struct kevent ev[2];

    if (_rsd) {
        EV_SET(&ev[0], _rsd.descriptor(), EVFILT_READ, EV_DELETE, 0, 0, 0);
        EV_SET(&ev[1], _rsd.descriptor(), EVFILT_WRITE, EV_DELETE, 0, 0, 0);
        if (kevent(impl_->reactor_fd, ev, 2, nullptr, 0, nullptr)) {
            solid_dbg(logger, Error, "kevent: " << last_system_error().message());
            solid_assert(false);
            return false;
        } else {
            --impl_->devcnt;
        }
    } else {
        EV_SET(ev, _rsd.descriptor(), EVFILT_USER, EV_DELETE, 0, 0, 0);
        if (kevent(impl_->reactor_fd, ev, 1, nullptr, 0, nullptr)) {
            solid_dbg(logger, Error, "kevent: " << last_system_error().message());
            solid_assert(false);
            return false;
        } else {
            --impl_->devcnt;
        }
    }
#elif defined(SOLID_USE_WSAPOLL)
    if (!_rsd) {
        return false;
    }
    impl_->eventvec[_rch.idxreactor].clear();
#endif
    return true;
}

//-----------------------------------------------------------------------------

bool Reactor::addTimer(CompletionHandler const& _rch, NanoTime const& _rt, size_t& _rstoreidx)
{
    if (_rstoreidx != InvalidIndex()) {
        size_t idx = impl_->timestore.change(_rstoreidx, _rt);
        solid_assert(idx == _rch.idxreactor);
    } else {
        _rstoreidx = impl_->timestore.push(_rt, _rch.idxreactor);
    }
    return true;
}

//-----------------------------------------------------------------------------

void Reactor::doUpdateTimerIndex(const size_t _chidx, const size_t _newidx, const size_t _oldidx)
{
    CompletionHandlerStub& rch = impl_->chdq[_chidx];
    solid_assert(static_cast<SteadyTimer*>(rch.pch)->storeidx == _oldidx);
    static_cast<SteadyTimer*>(rch.pch)->storeidx = _newidx;
}

//-----------------------------------------------------------------------------

bool Reactor::remTimer(CompletionHandler const& /*_rch*/, size_t const& _rstoreidx)
{
    if (_rstoreidx != InvalidIndex()) {
        impl_->timestore.pop(_rstoreidx, ChangeTimerIndexCallback(*this));
    }
    return true;
}

//-----------------------------------------------------------------------------

void Reactor::registerCompletionHandler(CompletionHandler& _rch, Actor const& _ract)
{
    size_t idx;

    if (!impl_->chposcache.empty()) {
        idx = impl_->chposcache.top();
        impl_->chposcache.pop();
    } else {
        idx = impl_->chdq.size();
        impl_->chdq.push_back(CompletionHandlerStub());
    }

    CompletionHandlerStub& rcs = impl_->chdq[idx];

    rcs.actidx = static_cast<size_t>(_ract.ActorBase::runId().index);
    rcs.pch    = &_rch;

    _rch.idxreactor = idx;

    solid_dbg(logger, Info, "idx " << idx << " chdq.size = " << impl_->chdq.size() << " this " << this);

    {
        NanoTime       dummytime;
        ReactorContext ctx(*this, dummytime);

        ctx.reactor_event_ = ReactorEventInit;
        ctx.actor_index_   = rcs.actidx;
        ctx.channel_index_ = idx;

        _rch.handleCompletion(ctx);
    }
}

//-----------------------------------------------------------------------------

void Reactor::unregisterCompletionHandler(CompletionHandler& _rch)
{
    solid_dbg(logger, Info, "");

    CompletionHandlerStub& rcs = impl_->chdq[_rch.idxreactor];

    {
        NanoTime       dummytime;
        ReactorContext ctx(*this, dummytime);

        ctx.reactor_event_ = ReactorEventClear;
        ctx.actor_index_   = rcs.actidx;
        ctx.channel_index_ = _rch.idxreactor;

        _rch.handleCompletion(ctx);
    }

    impl_->chposcache.push(_rch.idxreactor);
    rcs.pch    = &impl_->eventact.dummyhandler;
    rcs.actidx = 0;
    ++rcs.unique;
}

//-----------------------------------------------------------------------------

thread_local Reactor* thread_local_reactor = nullptr;

/*static*/ Reactor* Reactor::safeSpecific()
{
    return thread_local_reactor;
}

void Reactor::doStoreSpecific()
{
    thread_local_reactor = this;
}
void Reactor::doClearSpecific()
{
    thread_local_reactor = nullptr;
}

/*static*/ Reactor& Reactor::specific()
{
    solid_dbg(logger, Verbose, "");
    return *safeSpecific();
}

//-----------------------------------------------------------------------------
//      ReactorContext
//-----------------------------------------------------------------------------

Actor& ReactorContext::actor() const
{
    return reactor().actor(*this);
}

//-----------------------------------------------------------------------------

Service& ReactorContext::service() const
{
    return reactor().service(*this);
}

//-----------------------------------------------------------------------------

Manager& ReactorContext::manager() const
{
    return reactor().service(*this).manager();
}

//-----------------------------------------------------------------------------

std::mutex& ReactorContext::actorMutex() const
{
    return reactor().service(*this).mutex(reactor().actor(*this));
}

//-----------------------------------------------------------------------------

UniqueId ReactorContext::actorUid() const
{
    return reactor().actorUid(*this);
}

//-----------------------------------------------------------------------------

CompletionHandler* ReactorContext::completionHandler() const
{
    return reactor().completionHandler(*this);
}

//-----------------------------------------------------------------------------
} //namespace aio
} //namespace frame
} //namespace solid
