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
#include "solid/frame/schedulerbase.hpp"
#include "solid/system/error.hpp"
#include "solid/system/log.hpp"
#include "solid/system/pimpl.hpp"
#include "solid/utility/delegate.hpp"
#include "solid/utility/function.hpp"

namespace solid {

struct Event;

namespace frame {

extern const LoggerT logger;

class Manager;
class Service;
class ActorBase;
class SchedulerBase;
class ReactorBase;

struct ServiceStub;

class Manager final : NonCopyable {
    using LockedFunctionT   = std::function<void()>;
    using UnlockedFunctionT = std::function<void()>;

public:
    class VisitContext {
        friend class Manager;
        Manager&     rmanager_;
        ReactorBase& rreactor_;
        ActorBase&   ractor_;

        VisitContext(Manager& _rmanager, ReactorBase& _rreactor, ActorBase& _ractor)
            : rmanager_(_rmanager)
            , rreactor_(_rreactor)
            , ractor_(_ractor)
        {
        }

        VisitContext(const VisitContext&);
        VisitContext& operator=(const VisitContext&);

    public:
        ActorBase& actor() const
        {
            return ractor_;
        }

        bool raiseActor(Event const& _revent) const;
        bool raiseActor(Event&& _uevent) const;
    };

    Manager(
        const size_t _svcmtxcnt     = 0,
        const size_t _actmtxcnt     = 0,
        const size_t _actbucketsize = 0);

    virtual ~Manager();

    void stop();

    void start();

    bool notify(ActorIdT const& _ruid, Event&& _uevent);

    template <class F>
    bool visit(ActorIdT const& _ruid, F _f);

    template <class T, class F>
    bool visitDynamicCast(ActorIdT const& _ruid, F _f);

    template <class T, class F>
    bool visitExplicitCast(ActorIdT const& _ruid, F _f);

    ActorIdT id(const ActorBase& _ractor) const;

    Service& service(const ActorBase& _ractor) const;

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
        Event const& _revent, const size_t _sigmsk);

    static bool notify_actor(
        ActorBase& _ractor, ReactorBase& _rreactor,
        Event&& _uevent, const size_t _sigmsk);

    bool registerService(Service& _rservice, const bool _start);
    void unregisterService(Service& _rservice);

    void unregisterActor(ActorBase& _ractor);
    bool disableActorVisits(ActorBase& _ractor);

    ActorIdT unsafeId(const ActorBase& _ractor) const;

    std::mutex& mutex(const Service& _rservice) const;
    std::mutex& mutex(const ActorBase& _ractor) const;

    ActorIdT registerActor(
        const Service&     _rservice,
        ActorBase&         _ractor,
        ReactorBase&       _rr,
        ScheduleFunctionT& _rfct,
        ErrorConditionT&   _rerr);

    size_t notifyAll(const Service& _rservice, Event const& _revent);

    template <typename F>
    size_t forEachServiceActor(const Service& _rservice, F _f);

    void stopService(Service& _rservice, bool _wait);

    template <typename LockedFnc, typename UnlockedFnc>
    void startService(Service& _rservice, LockedFnc&& _lf, UnlockedFnc&& _uf);

    size_t doForEachServiceActor(const Service& _rservice, const ActorVisitFunctionT _rvisit_fnc);
    size_t doForEachServiceActor(const size_t _chkidx, const ActorVisitFunctionT _rvisit_fnc);
    bool   doVisit(ActorIdT const& _actor_id, const ActorVisitFunctionT _rvisit_fnc);
    void   doCleanupService(const size_t _service_index, Service& _rservice);
    void   doStartService(Service& _rservice, const LockedFunctionT& _locked_fnc, const UnlockedFunctionT& _unlocked_fnc);

private:
    struct Data;
    PimplT<Data> impl_;
};

//-----------------------------------------------------------------------------

template <class F>
inline bool Manager::visit(ActorIdT const& _ruid, F _f)
{
    return doVisit(_ruid, ActorVisitFunctionT{_f});
}

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

template <typename F>
inline size_t Manager::forEachServiceActor(const Service& _rservice, F _f)
{
    return doForEachServiceActor(_rservice, ActorVisitFunctionT{_f});
}

template <typename LockedFnc, typename UnlockedFnc>
inline void Manager::startService(Service& _rservice, LockedFnc&& _lf, UnlockedFnc&& _uf)
{
    doStartService(_rservice, LockedFunctionT{std::forward<LockedFnc>(_lf)}, UnlockedFunctionT{std::forward<UnlockedFnc>(_uf)});
}

//-----------------------------------------------------------------------------

} //namespace frame
} //namespace solid
