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

#include "solid/frame/common.hpp"
#include "solid/frame/objectbase.hpp"
#include "solid/frame/schedulerbase.hpp"
#include "solid/system/error.hpp"
#include "solid/system/function.hpp"
#include "solid/system/pimpl.hpp"
#include "solid/utility/delegate.hpp"
#include "solid/utility/dynamicpointer.hpp"

//#include "solid/utility/functor.hpp"

namespace solid {

struct Event;

namespace frame {

class Manager;
class Service;
class ObjectBase;
class SchedulerBase;
class ReactorBase;

struct ServiceStub;

class Manager final {
public:
    class VisitContext {
        friend class Manager;
        Manager&     rm_;
        ReactorBase& rr_;
        ObjectBase&  ro_;

        VisitContext(Manager& _rm, ReactorBase& _rr, ObjectBase& _ro)
            : rm_(_rm)
            , rr_(_rr)
            , ro_(_ro)
        {
        }

        VisitContext(const VisitContext&);
        VisitContext& operator=(const VisitContext&);

    public:
        ObjectBase& object() const
        {
            return ro_;
        }

        bool raiseObject(Event const& _revt) const;
        bool raiseObject(Event&& _uevt) const;
    };

    Manager(
        const size_t _svcmtxcnt     = 0,
        const size_t _objmtxcnt     = 0,
        const size_t _objbucketsize = 0);

    Manager(const Manager&) = delete;
    Manager(Manager&&)      = delete;
    Manager& operator=(const Manager&) = delete;
    Manager& operator=(Manager&&) = delete;

    virtual ~Manager();

    void stop();

    void start();

    bool notify(ObjectIdT const& _ruid, Event&& _uevt);

    //bool notifyAll(Event const &_revt, const size_t _sigmsk = 0);

    template <class F>
    bool visit(ObjectIdT const& _ruid, F _f)
    {
        //ObjectVisitFunctionT fct(std::cref(_f));
        return doVisit(_ruid, ObjectVisitFunctionT{_f});
    }

    template <class T, class F>
    bool visitDynamicCast(ObjectIdT const& _ruid, F _f)
    {
        auto l = [&_f](VisitContext& _rctx) {
            T* pt = dynamic_cast<T*>(&_rctx.object());
            if (pt) {
                return _f(_rctx, *pt);
            }
            return false;
        };
        ObjectVisitFunctionT fct(std::cref(l));
        return doVisit(_ruid, fct);
    }

    template <class T, class F>
    bool visitExplicitCast(ObjectIdT const& _ruid, F _f)
    {
        auto visit_lambda = [&_f](VisitContext& _rctx) {
            const std::type_info& req_type = typeid(T);
            const std::type_info& val_type = doGetTypeId(*(&_rctx.object()));

            if (std::type_index(req_type) == std::type_index(val_type)) {
                return _f(_rctx, static_cast<T&>(_rctx.object()));
            }
            return false;
        };
        //ObjectVisitFunctionT fct(std::cref(l));
        return doVisit(_ruid, ObjectVisitFunctionT{visit_lambda});
    }

    ObjectIdT id(const ObjectBase& _robj) const;

    Service& service(const ObjectBase& _robj) const;

private:
    friend class Service;
    friend class ObjectBase;
    friend class ReactorBase;
    friend class SchedulerBase;
    friend class Manager::VisitContext;

    using ObjectVisitFunctionT = Delegate<bool(VisitContext&)>;

    size_t serviceCount() const;

    static const std::type_info& doGetTypeId(const ObjectBase& _robj)
    {
        return typeid(_robj);
    }

    static bool notify_object(
        ObjectBase& _robj, ReactorBase& _rreact,
        Event const& _revt, const size_t _sigmsk);

    static bool notify_object(
        ObjectBase& _robj, ReactorBase& _rreact,
        Event&& _uevt, const size_t _sigmsk);

    bool registerService(Service& _rsvc);
    void unregisterService(Service& _rsvc);

    void unregisterObject(ObjectBase& _robj);
    bool disableObjectVisits(ObjectBase& _robj);

    ObjectIdT unsafeId(const ObjectBase& _robj) const;

    std::mutex& mutex(const Service& _rsvc) const;
    std::mutex& mutex(const ObjectBase& _robj) const;

    ObjectIdT registerObject(
        const Service&     _rsvc,
        ObjectBase&        _robj,
        ReactorBase&       _rr,
        ScheduleFunctionT& _rfct,
        ErrorConditionT&   _rerr);

    size_t notifyAll(const Service& _rsvc, Event const& _revt);

    template <typename F>
    size_t forEachServiceObject(const Service& _rsvc, F _f)
    {
        ObjectVisitFunctionT fct(_f);
        return doForEachServiceObject(_rsvc, fct);
    }

    bool raise(const ObjectBase& _robj, Event const& _re);

    void stopService(Service& _rsvc, bool _wait);
    bool startService(Service& _rsvc);

    size_t doForEachServiceObject(const Service& _rsvc, const ObjectVisitFunctionT _rfct);
    size_t doForEachServiceObject(const size_t _chkidx, const ObjectVisitFunctionT _rfct);
    bool   doVisit(ObjectIdT const& _ruid, const ObjectVisitFunctionT _fctor);
    void   doUnregisterService(ServiceStub& _rss);

private:
    struct Data;
    PimplT<Data> impl_;
};

} //namespace frame
} //namespace solid
