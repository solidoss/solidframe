// solid/utility/workpool.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/system/statistic.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/queue.hpp"

namespace solid {

extern const LoggerT workpool_logger;

template <typename Job>
class WorkPool2{
    using ThisT = WorkPool2<Job>;
    using JobVectorT = std::vector<Job>;
public:
    WorkPool2()
    {
    }

    ~WorkPool2()
    {
        stop(true);
    }

    template <class JT>
    void push(const JT& _jb)
    {
    }

    template <class JT>
    void push(JT&& _jb)
    {
    }

    template <class I>
    void push(I _i, const I& _end)
    {
    }

    void start(const size_t _min_wkr_cnt = 0, const size_t _max_wkr_cnt = std::thread::hardware_concurrency())
    {
        solid_dbg(workpool_logger, Verbose, this << " start " << _minwkrcn_min_wkr_cnt <<" "<<_max_wkr_cnt);
        
    }
    
    void stop(bool _wait = true)
    {
        solid_dbg(workpool_logger, Verbose, this << " stop " << _wait);
        
    }
};

} //namespace solid
