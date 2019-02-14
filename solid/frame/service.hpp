// solid/frame/service.hpp
//
// Copyright (c) 2013, 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/common.hpp"
#include "solid/frame/manager.hpp"
#include "solid/frame/schedulerbase.hpp"
#include "solid/utility/dynamictype.hpp"
#include <atomic>
#include <mutex>
#include <vector>

namespace solid {
struct Event;
namespace frame {

class ActorBase;

class Service;

template <class S = Service>
class ServiceShell;

struct UseServiceShell {
    Manager& rmanager;

    UseServiceShell()        = delete;
    UseServiceShell& operator=(const UseServiceShell&) = delete;
    UseServiceShell& operator=(UseServiceShell&&) = delete;

    UseServiceShell(UseServiceShell&& _uss)
        : rmanager(_uss.rmanager)
    {
    }
    UseServiceShell(const UseServiceShell& _uss)
        : rmanager(_uss.rmanager)
    {
    }

private:
    template <class S>
    friend class ServiceShell;

    explicit UseServiceShell(Manager& _rmanager)
        : rmanager(_rmanager)
    {
    }
};

class Service {
protected:
    explicit Service(
        UseServiceShell _force_shell);

public:
    Service(const Service&) = delete;
    Service(Service&&)      = delete;
    Service& operator=(const Service&) = delete;
    Service& operator=(Service&&) = delete;

    virtual ~Service();

    bool isRegistered() const;

    void notifyAll(Event const& _e);

    template <class F>
    bool forEach(F& _rf);

    void stop(const bool _wait = true);

    bool start();

    Manager& manager();

    std::mutex& mutex(const ActorBase& _ract) const;

    bool isRunning() const;

    ActorIdT id(const ActorBase& _ract) const;

protected:
    std::mutex& mutex() const;

private:
    friend class Manager;
    friend class SchedulerBase;

    void setRunning()
    {
        running = true;
    }

    void resetRunning()
    {
        running = false;
    }

    ActorIdT registerActor(ActorBase& _ract, ReactorBase& _rr, ScheduleFunctionT& _rfct, ErrorConditionT& _rerr);

private:
    Manager&            rm;
    std::atomic<size_t> idx;
    std::atomic<bool>   running;
};

template <class F>
inline bool Service::forEach(F& _rf)
{
    return rm.forEachServiceActor(*this, _rf);
}

inline Manager& Service::manager()
{
    return rm;
}

inline bool Service::isRegistered() const
{
    return idx.load(/*std::memory_order_seq_cst*/) != InvalidIndex();
}

inline bool Service::isRunning() const
{
    return running;
}

inline void Service::notifyAll(Event const& _revt)
{
    rm.notifyAll(*this, _revt);
}

inline bool Service::start()
{
    return rm.startService(*this);
}

inline void Service::stop(const bool _wait)
{
    rm.stopService(*this, _wait);
}

inline std::mutex& Service::mutex(const ActorBase& _ract) const
{
    return rm.mutex(_ract);
}

inline ActorIdT Service::id(const ActorBase& _ract) const
{
    return rm.id(_ract);
}

inline std::mutex& Service::mutex() const
{
    return rm.mutex(*this);
}

inline ActorIdT Service::registerActor(ActorBase& _ract, ReactorBase& _rr, ScheduleFunctionT& _rfct, ErrorConditionT& _rerr)
{
    return rm.registerActor(*this, _ract, _rr, _rfct, _rerr);
}

//! A Shell class for every Service
/*!
 * This class is provided for defensive C++ programming.
 * Actors from a Service use reference to their service.
 * Situation: we have ServiceA: public Service. Actors from ServiceA use reference to ServiceA.
 * If we only call Service::stop() from within frame::Service::~Service, when ServiceA gets destroyed,
 * existing actors (at the moment we call Service::stop) might be still accessing ServiceA actor layer
 * which was destroyed.
 * That is why we've introduce the ServiceShell which will stand as an upper layer for all Service
 * instantiations which will call Service::stop on its destructor, so that when the lower layer Service
 * gets destroyed no actor will exist.
 * ServiceShell is final to prevent inheriting from it.
 * More over, we introduce the UseServiceShell stub to force all Service instantiations to happen through
 * a ServiceShell.
 */
template <class S>
class ServiceShell final : public S {
public:
    template <typename... Args>
    explicit ServiceShell(Manager& _rm, Args&&... _args)
        : S(UseServiceShell(_rm), std::forward<Args>(_args)...)
    {
    }

    ~ServiceShell()
    {
        Service::stop(true);
    }
};

using ServiceT = ServiceShell<>;

} //namespace frame
} //namespace solid
