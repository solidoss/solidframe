// solid/frame/schedulerbase.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/common.hpp"
#include "solid/system/error.hpp"
#include "solid/system/pimpl.hpp"
#include "solid/utility/function.hpp"
#include <thread>

namespace solid {

namespace frame {

class Service;
class ReactorBase;
class ActorBase;

//typedef FunctorReference<bool, ReactorBase&>  ScheduleFunctorT;
typedef solid_function_t(bool(ReactorBase&)) ScheduleFunctionT;

//! A base class for all schedulers
class SchedulerBase {
public:
protected:
    typedef bool (*CreateWorkerF)(SchedulerBase& _rsch, const size_t, std::thread& _rthr);

    typedef solid_function_t(bool()) ThreadEnterFunctionT;
    typedef solid_function_t(void()) ThreadExitFunctionT;

    ErrorConditionT doStart(
        CreateWorkerF         _pf,
        ThreadEnterFunctionT& _renf,
        ThreadExitFunctionT&  _rexf,
        size_t                _reactorcnt);

    void doStop(const bool _wait = true);

    ActorIdT doStartActor(ActorBase& _ract, Service& _rsvc, ScheduleFunctionT& _rfct, ErrorConditionT& _rerr);

protected:
    SchedulerBase();
    ~SchedulerBase();

private:
    friend class ReactorBase;

    bool   prepareThread(const size_t _idx, ReactorBase& _rsel, const bool _success);
    void   unprepareThread(const size_t _idx, ReactorBase& _rsel);
    size_t doComputeScheduleReactorIndex();

private:
    struct Data;
    PimplT<Data> impl_;
};

} //namespace frame
} //namespace solid
