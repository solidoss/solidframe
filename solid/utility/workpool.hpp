// solid/utility/workpool_atomic.hpp
//
// Copyright (c) 2018 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once
#include "solid/utility/common.hpp"

//#define SOLID_USE_WORKPOOL_MUTEX

#ifdef SOLID_USE_WORKPOOL_MUTEX
#include "solid/utility/workpool_mutex.hpp"
#else
#include "solid/utility/workpool_atomic.hpp"
#endif

namespace solid {

template <class>
class CallPool;

template <class R, class... ArgTypes>
class CallPool<R(ArgTypes...)>{
    using FunctionT = std::function<R(ArgTypes...)>;
    using WorkPoolT = WorkPool<FunctionT>;
    WorkPoolT  wp_;
public:
    CallPool(const WorkPoolConfiguration& _cfg):wp_(_cfg){}
    
    template <typename... Args>
    CallPool(
        const WorkPoolConfiguration& _cfg,
        const size_t   _start_wkr_cnt,
        Args&&... _args
    ): wp_(
        _cfg,
        _start_wkr_cnt,
        [](FunctionT &_rfnc, Args&&... _args){
            _rfnc(std::forward<ArgTypes>(_args)...);
        }, std::forward<Args>(_args)...
    ){}
    
    template <typename... Args>
    void start(const size_t   _start_wkr_cnt, Args... _args){
        wp_.start(_start_wkr_cnt,
            [](FunctionT &_rfnc, Args... _args){
                _rfnc(std::forward<Args>(_args)...);
            }, std::forward<Args>(_args)...);
    }
    
    template <class JT>
    void push(const JT& _jb){
        wp_.push(_jb);
    }

    template <class JT>
    void push(JT&& _jb){
        wp_.push(std::forward<JT>(_jb));
    }
};

#if 0
template <typename Ctx = void>
class FunctionWorkPool;

template <>
class FunctionWorkPool<void> : public WorkPool<std::function<void()>> {
    using WorkPoolT = WorkPool<std::function<void()>>;

public:
    FunctionWorkPool(
        const size_t                 _start_wkr_cnt,
        const WorkPoolConfiguration& _cfg)
        : WorkPoolT(
              _start_wkr_cnt,
              _cfg,
              [](std::function<void()>& _rfnc) {
                  _rfnc();
              })
    {
    }

    FunctionWorkPool(
        const WorkPoolConfiguration& _cfg)
        : WorkPoolT(
              0,
              _cfg,
              [](std::function<void()>& _rfnc) {
                  _rfnc();
              })
    {
    }
};

template <typename Ctx>
class FunctionWorkPool : public WorkPool<std::function<void(Ctx&)>> {
    using WorkPoolT = WorkPool<std::function<void(Ctx&)>>;

    Ctx& rctx_;

public:
    FunctionWorkPool(
        const size_t                 _start_wkr_cnt,
        const WorkPoolConfiguration& _cfg,
        Ctx&                         _rctx)
        : WorkPoolT(
              _start_wkr_cnt,
              _cfg,
              [this](std::function<void(Ctx&)>& _rfnc) {
                  _rfnc(this->context());
              })
        , rctx_(_rctx)
    {
    }

    FunctionWorkPool(
        const WorkPoolConfiguration& _cfg,
        Ctx&                         _rctx)
        : WorkPoolT(
              0,
              _cfg,
              [this](std::function<void(Ctx&)>& _rfnc) {
                  _rfnc(this->context());
              })
        , rctx_(_rctx)
    {
    }

    Ctx& context() const
    {
        return rctx_;
    }
};
#endif
} //namespace solid
