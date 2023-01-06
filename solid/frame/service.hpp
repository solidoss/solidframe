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
#include "solid/utility/any.hpp"
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

struct UseServiceShell : NonCopyable {
    Manager& rmanager;

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

class Service : NonCopyable {
    enum struct StatusE {
        Stopped,
        Running,
        Stopping,
    };
    Manager&            rm_;
    std::atomic<size_t> idx_;
    Any<> any_;

protected:
    explicit Service(
        UseServiceShell _force_shell, const bool _start = true);

    template <typename A>
    Service(
        UseServiceShell _force_shell, A&& _a, const bool _start = true);

public:
    virtual ~Service();

    bool registered() const;

    void notifyAll(Event const& _e);

    template <class F>
    bool forEach(F& _rf);

    void stop(const bool _wait = true);

    Manager& manager();

    std::mutex& mutex(const ActorBase& _ract) const;

    ActorIdT id(const ActorBase& _ract) const;

    auto& any()
    {
        return any_;
    }
    const auto& any() const
    {
        return any_;
    }

protected:
    std::mutex& mutex() const;

    ServiceStatusE status(std::unique_lock<std::mutex>& _rlock);

    void doStart();

    template <typename A>
    void doStart(A&& _a);

    template <typename A, typename F>
    void doStartWithAny(A&& _a, F&& _f);

    template <typename F>
    void doStartWithoutAny(F&& _f);

private:
    friend class Manager;
    friend class SchedulerBase;

    ActorIdT registerActor(ActorBase& _ract, ReactorBase& _rr, ScheduleFunctionT& _rfct, ErrorConditionT& _rerr);
 
    size_t       index() const;
    void         index(const size_t _idx);
    virtual void onLockedStoppingBeforeActors();
};

inline Service::Service(
    UseServiceShell _force_shell, const bool _start)
    : rm_(_force_shell.rmanager)
    , idx_(static_cast<size_t>(InvalidIndex()))
{
    rm_.registerService(*this, _start);
}

template <typename AnyType>
inline Service::Service(
    UseServiceShell _force_shell, AnyType&& _any, const bool _start)
    : rm_(_force_shell.rmanager)
    , idx_(static_cast<size_t>(InvalidIndex()))
    , any_(std::forward<AnyType>(_any))
{
    rm_.registerService(*this, _start);
}

inline Service::~Service()
{
    stop(true);
    rm_.unregisterService(*this);
}

template <class F>
inline bool Service::forEach(F& _rf)
{
    return rm_.forEachServiceActor(*this, _rf);
}

inline Manager& Service::manager()
{
    return rm_;
}

inline bool Service::registered() const
{
    return idx_.load(/*std::memory_order_seq_cst*/) != InvalidIndex();
}

inline size_t Service::index() const
{
    return idx_.load();
}

inline void Service::index(const size_t _idx)
{
    idx_.store(_idx);
}

inline void Service::notifyAll(Event const& _revt)
{
    rm_.notifyAll(*this, _revt);
}

inline void Service::doStart()
{
    rm_.startService(
        *this, [](std::unique_lock<std::mutex>&) {});
}

template <typename AnyType>
inline void Service::doStart(AnyType&& _any)
{
    Any<> any{std::forward<AnyType>(_any)};
    rm_.startService(
        *this, [this, &any](std::unique_lock<std::mutex>&) { any_ = std::move(any); });
}

template <typename AnyType, typename F>
inline void Service::doStartWithAny(AnyType&& _any, F&& _on_locked_start)
{
    Any<> any{std::forward<AnyType>(_any)};
    rm_.startService(
        *this, [this, &any, &_on_locked_start](std::unique_lock<std::mutex>& _lock) { any_ = std::move(any); _on_locked_start(_lock); });
}

template <typename F>
inline void Service::doStartWithoutAny(F&& _on_locked_start)
{
    rm_.startService(
        *this, [&_on_locked_start](std::unique_lock<std::mutex>& _lock) { _on_locked_start(_lock); });
}

inline void Service::stop(const bool _wait)
{
    rm_.stopService(*this, _wait);
}

inline std::mutex& Service::mutex(const ActorBase& _ract) const
{
    return rm_.mutex(_ract);
}

inline ActorIdT Service::id(const ActorBase& _ract) const
{
    return rm_.id(_ract);
}

inline std::mutex& Service::mutex() const
{
    return rm_.mutex(*this);
}

inline ServiceStatusE Service::status(std::unique_lock<std::mutex>& _rlock)
{
    return rm_.status(*this, _rlock);
}

inline ActorIdT Service::registerActor(ActorBase& _ract, ReactorBase& _rr, ScheduleFunctionT& _rfct, ErrorConditionT& _rerr)
{
    return rm_.registerActor(*this, _ract, _rr, _rfct, _rerr);
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

    template <typename... Args>
    void start(Args&&... _args)
    {
        S::doStart(std::forward<Args>(_args)...);
    }
};

using ServiceT = ServiceShell<>;

} // namespace frame
} // namespace solid
