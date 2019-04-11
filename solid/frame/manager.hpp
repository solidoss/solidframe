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
#include "solid/utility/dynamicpointer.hpp"
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
        Manager&     rm_;
        ReactorBase& rr_;
        ActorBase&   ra_;

        VisitContext(Manager& _rm, ReactorBase& _rr, ActorBase& _ra)
            : rm_(_rm)
            , rr_(_rr)
            , ra_(_ra)
        {
        }

        VisitContext(const VisitContext&);
        VisitContext& operator=(const VisitContext&);

    public:
        ActorBase& actor() const
        {
            return ra_;
        }

        bool raiseActor(Event const& _revt) const;
        bool raiseActor(Event&& _uevt) const;
    };

    Manager(
        const size_t _svcmtxcnt     = 0,
        const size_t _actmtxcnt     = 0,
        const size_t _actbucketsize = 0);

    virtual ~Manager();

    void stop();

    void start();

    bool notify(ActorIdT const& _ruid, Event&& _uevt);

    //bool notifyAll(Event const &_revt, const size_t _sigmsk = 0);

    template <class F>
    bool visit(ActorIdT const& _ruid, F _f)
    {
        //ActorVisitFunctionT fct(std::cref(_f));
        return doVisit(_ruid, ActorVisitFunctionT{_f});
    }

    template <class T, class F>
    bool visitDynamicCast(ActorIdT const& _ruid, F _f)
    {
        auto l = [&_f](VisitContext& _rctx) {
            T* pt = dynamic_cast<T*>(&_rctx.actor());
            if (pt) {
                return _f(_rctx, *pt);
            }
            return false;
        };
        ActorVisitFunctionT fct(std::cref(l));
        return doVisit(_ruid, fct);
    }

    template <class T, class F>
    bool visitExplicitCast(ActorIdT const& _ruid, F _f)
    {
        auto visit_lambda = [&_f](VisitContext& _rctx) {
            const std::type_info& req_type = typeid(T);
            const std::type_info& val_type = doGetTypeId(*(&_rctx.actor()));

            if (std::type_index(req_type) == std::type_index(val_type)) {
                return _f(_rctx, static_cast<T&>(_rctx.actor()));
            }
            return false;
        };
        //ActorVisitFunctionT fct(std::cref(l));
        return doVisit(_ruid, ActorVisitFunctionT{visit_lambda});
    }

    ActorIdT id(const ActorBase& _ract) const;

    Service& service(const ActorBase& _ract) const;

private:
    friend class Service;
    friend class ActorBase;
    friend class ReactorBase;
    friend class SchedulerBase;
    friend class Manager::VisitContext;

    using ActorVisitFunctionT = Delegate<bool(VisitContext&)>;

    size_t serviceCount() const;

    static const std::type_info& doGetTypeId(const ActorBase& _ract)
    {
        return typeid(_ract);
    }

    static bool notify_actor(
        ActorBase& _ract, ReactorBase& _rreact,
        Event const& _revt, const size_t _sigmsk);

    static bool notify_actor(
        ActorBase& _ract, ReactorBase& _rreact,
        Event&& _uevt, const size_t _sigmsk);

    bool registerService(Service& _rsvc, const bool _start);
    void unregisterService(Service& _rsvc);

    void unregisterActor(ActorBase& _ract);
    bool disableActorVisits(ActorBase& _ract);

    ActorIdT unsafeId(const ActorBase& _ract) const;

    std::mutex& mutex(const Service& _rsvc) const;
    std::mutex& mutex(const ActorBase& _ract) const;

    ActorIdT registerActor(
        const Service&     _rsvc,
        ActorBase&         _ract,
        ReactorBase&       _rr,
        ScheduleFunctionT& _rfct,
        ErrorConditionT&   _rerr);

    size_t notifyAll(const Service& _rsvc, Event const& _revt);

    template <typename F>
    size_t forEachServiceActor(const Service& _rsvc, F _f)
    {
        ActorVisitFunctionT fct(_f);
        return doForEachServiceActor(_rsvc, fct);
    }

    bool raise(const ActorBase& _ract, Event const& _re);

    void stopService(Service& _rsvc, bool _wait);

    template <typename LockedFnc, typename UnlockedFnc>
    void startService(Service& _rsvc, LockedFnc&& _lf, UnlockedFnc&& _uf)
    {
        LockedFunctionT   lf{std::forward<LockedFnc>(_lf)};
        UnlockedFunctionT uf{std::forward<UnlockedFnc>(_uf)};

        doStartService(_rsvc, lf, uf);
    }

    size_t doForEachServiceActor(const Service& _rsvc, const ActorVisitFunctionT _rfct);
    size_t doForEachServiceActor(const size_t _chkidx, const ActorVisitFunctionT _rfct);
    bool   doVisit(ActorIdT const& _ruid, const ActorVisitFunctionT _fctor);
    void   doUnregisterService(ServiceStub& _rss);
    void   doStartService(Service& _rsvc, LockedFunctionT& _locked_fnc, UnlockedFunctionT& _unlocked_fnc);

private:
    struct Data;
    PimplT<Data> impl_;
};

} //namespace frame
} //namespace solid
