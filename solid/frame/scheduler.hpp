// solid/frame/scheduler.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/manager.hpp"
#include "solid/frame/schedulerbase.hpp"

namespace solid {
namespace frame {

class Service;

constexpr size_t default_reactor_wake_capacity = 1024;

template <class R>
class Scheduler : private SchedulerBase {
    using ReactorT = R;

public:
    using ActorT        = typename ReactorT::ActorT;
    using EventT        = typename ReactorT::EventT;
    using ActorPointerT = std::shared_ptr<ActorT>;

private:
    struct Worker {
        static void run(SchedulerBase* _psched, const size_t _idx, const size_t _wake_capacity)
        {
            ReactorT reactor{*_psched, _idx, _wake_capacity};

            if (!reactor.prepareThread(reactor.start())) {
                return;
            }
            reactor.run();
            reactor.unprepareThread();
        }

        static bool create(SchedulerBase& _rsched, const size_t _idx, std::thread& _rthr, const size_t _wake_capacity)
        {
            bool rv = false;
            try {
                _rthr = std::thread(Worker::run, &_rsched, _idx, _wake_capacity);
                rv    = true;
            } catch (...) {
            }
            return rv;
        }
    };

    struct ScheduleCommand {
        ActorPointerT&& ractptr;
        Service&        rsvc;
        EventBase&&     revt;

        ScheduleCommand(ActorPointerT&& _ractptr, Service& _rsvc, EventBase&& _revt)
            : ractptr(std::move(_ractptr))
            , rsvc(_rsvc)
            , revt(std::move(_revt))
        {
        }

        bool operator()(ReactorBase& _rreactor)
        {
            return static_cast<ReactorT&>(_rreactor).push(std::move(ractptr), rsvc, std::move(revt));
        }
    };

public:
    Scheduler() = default;

    void start(const size_t _reactorcnt = 1, const size_t _wake_capacity = default_reactor_wake_capacity)
    {
        ThreadEnterFunctionT enf;
        ThreadExitFunctionT  exf;
        SchedulerBase::doStart(Worker::create, enf, exf, _reactorcnt, _wake_capacity);
    }

    template <class EnterFct, class ExitFct>
    void start(EnterFct _enf, ExitFct _exf, const size_t _reactorcnt = 1, const size_t _wake_capacity = default_reactor_wake_capacity)
    {
        ThreadEnterFunctionT enf(std::move(_enf));
        ThreadExitFunctionT  exf(std::move(_exf));
        SchedulerBase::doStart(Worker::create, enf, exf, _reactorcnt, _wake_capacity);
    }

    size_t workerCount() const
    {
        return SchedulerBase::workerCount();
    }

    void stop(const bool _wait = true)
    {
        SchedulerBase::doStop(_wait);
    }

    ActorIdT startActor(
        ActorPointerT&& _ractptr, Service& _rsvc,
        EventBase&& _revt, ErrorConditionT& _rerr)
    {
        ScheduleCommand   cmd(std::move(_ractptr), _rsvc, std::move(_revt));
        ScheduleFunctionT fct([&cmd](ReactorBase& _rreactor) { return cmd(_rreactor); });

        return doStartActor(*_ractptr, _rsvc, fct, _rerr);
    }

    ActorIdT startActor(
        ActorPointerT&& _ractptr, Service& _rsvc, const size_t _worker_index,
        EventBase&& _revt, ErrorConditionT& _rerr)
    {
        ScheduleCommand   cmd(std::move(_ractptr), _rsvc, std::move(_revt));
        ScheduleFunctionT fct([&cmd](ReactorBase& _rreactor) { return cmd(_rreactor); });

        return doStartActor(*_ractptr, _rsvc, _worker_index, fct, _rerr);
    }
};

template <class Actr, class Schd, class Srvc, class... P>
inline ActorIdT make_actor(Schd& _rschd, Srvc& _rsrvc, EventBase&& _revt, ErrorConditionT& _rerr, P&&... _p)
{
    return _rschd.startActor(std::make_shared<Actr>(std::forward<P>(_p)...), _rsrvc, std::move(_revt), _rerr);
}

template <class Actr, class Schd, class Srvc, class... P>
inline ActorIdT make_actor(Schd& _rschd, Srvc& _rsrvc, const size_t _worker_index, EventBase&& _revt, ErrorConditionT& _rerr, P&&... _p)
{
    return _rschd.startActor(std::make_shared<Actr>(std::forward<P>(_p)...), _rsrvc, _worker_index, std::move(_revt), _rerr);
}

} // namespace frame
} // namespace solid
