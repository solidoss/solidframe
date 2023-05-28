// solid/frame/manager.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <mutex>
#include <typeindex>
#include <typeinfo>

#include "solid/frame/actorbase.hpp"
#include "solid/frame/common.hpp"
#include "solid/frame/reactorbase.hpp"
#include "solid/frame/schedulerbase.hpp"
#include "solid/system/error.hpp"
#include "solid/system/log.hpp"
#include "solid/system/pimpl.hpp"
#include "solid/system/spinlock.hpp"
#include "solid/utility/delegate.hpp"
#include "solid/utility/function.hpp"

namespace solid {

class EventBase;

namespace frame {

extern const LoggerT frame_logger;

class Manager;
class Service;
class ActorBase;
class SchedulerBase;
class ReactorBase;
struct ServiceStub;

enum struct ServiceStatusE {
    Stopped,
    // Starting,
    Running,
    Stopping,
    Invalid,
};

class Manager final : NonCopyable {
    using OnLockedStartFunctionT = std::function<void(std::unique_lock<std::mutex>&)>;
    struct Data;
    PimplT<Data> pimpl_;

public:
    using ChunkMutexT   = SpinLock;
    using ActorMutexT   = ChunkMutexT;
    using ServiceMutexT = std::mutex;

    class VisitContext {
        using ActorRunIdT = decltype(std::declval<ActorBase>().runId());
        friend class Manager;
        Manager&          rmanager_;
        ReactorBasePtrT   reactor_ptr_;
        const ActorRunIdT actor_run_id_;

        VisitContext(Manager& _rmanager, const ReactorBasePtrT& _reactor_ptr, const ActorRunIdT& _actor_run_id)
            : rmanager_(_rmanager)
            , reactor_ptr_(_reactor_ptr)
            , actor_run_id_(_actor_run_id)
        {
        }

        VisitContext(const VisitContext&);
        VisitContext& operator=(const VisitContext&);

    public:
        bool wakeActor(EventBase const& _revent) const;
        bool wakeActor(EventBase&& _uevent) const;
    };

    Manager(
        const size_t _service_capacity    = 1024,
        const size_t _actor_capacity      = 1024 * 1024,
        const size_t _actor_bucket_size   = 0,
        const size_t _service_mutex_count = 0,
        const size_t _actor_mutex_count   = 0,
        const size_t _chunk_mutex_count   = 0);

    virtual ~Manager();

    void stop();

    void start();

    bool notify(ActorIdT const& _ruid, EventBase&& _uevent);

    bool notify(ActorIdT const& _ruid, const EventBase& _event);

    template <class F>
    bool visit(ActorIdT const& _ruid, F _f);

#if false // TODO:vapa:delete
    template <class T, class F>
    bool visitDynamicCast(ActorIdT const& _ruid, F _f);

    template <class T, class F>
    bool visitExplicitCast(ActorIdT const& _ruid, F _f);
#endif

    ActorIdT id(const ActorBase& _ractor) const;

    Service& service(const ActorBase& _ractor) const;
    size_t   actorCapacity() const;
    size_t   actorSize() const;
    size_t   serviceCapacity() const;
    size_t   serviceSize() const;

private:
    friend class Service;
    friend class ActorBase;
    friend class ReactorBase;
    friend class SchedulerBase;
    friend class Manager::VisitContext;

    using ActorVisitFunctionT = Delegate<bool(VisitContext&)>;

    size_t serviceCount() const;

    static const std::type_info& doGetTypeId(const ActorBase& _ractor)
    {
        return typeid(_ractor);
    }

    static bool notify_actor(
        ActorBase& _ractor, ReactorBase& _rreactor,
        EventBase const& _revent, const size_t _sigmsk);

    static bool notify_actor(
        ActorBase& _ractor, ReactorBase& _rreactor,
        EventBase&& _uevent, const size_t _sigmsk);

    bool registerService(Service& _rservice, const bool _start);
    void unregisterService(Service& _rservice);

    void unregisterActor(ActorBase& _ractor);
    bool disableActorVisits(ActorBase& _ractor);

    ActorIdT unsafeId(const ActorBase& _ractor) const;

    ServiceMutexT& mutex(const Service& _rservice) const;
    ActorMutexT&   mutex(const ActorBase& _ractor) const;

    ServiceStatusE status(const Service& _rservice, std::unique_lock<ServiceMutexT>& _rlock) const;

    ServiceStatusE status(const Service& _rservice);

    ActorIdT registerActor(
        const Service&     _rservice,
        ActorBase&         _ractor,
        ReactorBase&       _rr,
        ScheduleFunctionT& _rfct,
        ErrorConditionT&   _rerr);

    size_t notifyAll(const Service& _rservice, EventBase const& _revent);

    template <typename F>
    size_t forEachServiceActor(const Service& _rservice, F _f);

    void stopService(Service& _rservice, bool _wait);

    template <typename OnLockedStartF>
    void startService(Service& _rservice, OnLockedStartF&& _on_locked_fnc);

    size_t doForEachServiceActor(const Service& _rservice, const ActorVisitFunctionT& _rvisit_fnc);
    size_t doForEachServiceActor(const size_t _chkidx, const ActorVisitFunctionT& _rvisit_fnc);
    bool   doVisit(ActorIdT const& _actor_id, const ActorVisitFunctionT& _rvisit_fnc);

    bool doStopService(const size_t _service_index, const bool _wait);
    void doCleanupService(const size_t _service_index, Service& _rservice);
    void doStartService(Service& _rservice, const OnLockedStartFunctionT& _locked_fnc);
};

//-----------------------------------------------------------------------------

template <class F>
inline bool Manager::visit(ActorIdT const& _ruid, F _f)
{
    return doVisit(_ruid, ActorVisitFunctionT{_f});
}
#if false
template <class T, class F>
inline bool Manager::visitDynamicCast(ActorIdT const& _ruid, F _f)
{
    auto lambda = [&_f](VisitContext& _rctx) {
        T* pt = dynamic_cast<T*>(&_rctx.actor());
        if (pt) {
            return _f(_rctx, *pt);
        }
        return false;
    };
    ActorVisitFunctionT fct(std::ref(lambda));
    return doVisit(_ruid, fct);
}

template <class T, class F>
inline bool Manager::visitExplicitCast(ActorIdT const& _ruid, F _f)
{
    auto lambda = [&_f](VisitContext& _rctx) {
        const std::type_info& req_type = typeid(T);
        const std::type_info& val_type = doGetTypeId(*(&_rctx.actor()));

        if (std::type_index(req_type) == std::type_index(val_type)) {
            return _f(_rctx, static_cast<T&>(_rctx.actor()));
        }
        return false;
    };

    return doVisit(_ruid, ActorVisitFunctionT{lambda});
}
#endif

template <typename F>
inline size_t Manager::forEachServiceActor(const Service& _rservice, F _f)
{
    return doForEachServiceActor(_rservice, ActorVisitFunctionT{_f});
}

template <typename OnLockedStartF>
inline void Manager::startService(Service& _rservice, OnLockedStartF&& _on_locked_fnc)
{
    doStartService(_rservice, OnLockedStartFunctionT{std::forward<OnLockedStartF>(_on_locked_fnc)});
}

//-----------------------------------------------------------------------------

} // namespace frame
} // namespace solid
