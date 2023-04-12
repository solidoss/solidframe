// solid/utility/threadpool.hpp
//
// Copyright (c) 2023 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once
#include <deque>
#include <functional>
#include <thread>

#include "solid/system/exception.hpp"
#include "solid/system/spinlock.hpp"
#include "solid/system/statistic.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/queue.hpp"

namespace solid {

struct ThreadPoolStatistic : solid::Statistic {
    ThreadPoolStatistic();

    std::ostream& print(std::ostream& _ros) const override;
    void          clear();
};

template <class Task, class MCast, class Stats = ThreadPoolStatistic>
class ThreadPool;

template <class TP, class ContextStub>
class SynchronizationContext {
    TP*          pthread_pool_ = nullptr;
    ContextStub* pcontext_     = nullptr;

    template <class Task, class MCast, class Stats>
    friend class ThreadPool;

    SynchronizationContext(TP* _pthread_pool, ContextStub* _pcontext)
        : pthread_pool_(_pthread_pool)
        , pcontext_(_pcontext)
    {
    }

public:
    SynchronizationContext() {}

    SynchronizationContext(const SynchronizationContext& _other) = delete;

    SynchronizationContext(SynchronizationContext&& _other)
        : pthread_pool_(_other.pthread_pool_)
        , pcontext_(_other.pcontext_)
    {
        _other.pthread_pool_ = nullptr;
        _other.pcontext_     = nullptr;
    }

    ~SynchronizationContext()
    {
        clear();
    }

    SynchronizationContext& operator=(const SynchronizationContext& _other) = delete;

    SynchronizationContext& operator=(SynchronizationContext&& _other)
    {
        if (this != &_other) {
            clear();
            pthread_pool_    = _other.pthread_pool_;
            pcontext_        = _other.pcontext_;
            _other.pcontext_ = nullptr;
            _other.pcontext_ = nullptr;
        }
        return *this;
    }

    void clear()
    {
        if (!empty()) {
            pthread_pool_->release(pcontext_);
            pcontext_     = nullptr;
            pthread_pool_ = nullptr;
        }
    }

    bool empty() const
    {
        return pthread_pool_ == nullptr;
    }

    template <class Task>
    void push(Task&& _task)
    {
        solid_check(!empty());
        pthread_pool_->impl_.doPush(std::forward<Task>(_task), pcontext_);
    }
};

namespace tpimpl {
template <class Task, class MCast, class Stats>
class ThreadPool : NonCopyable {
public:
    struct ContextStub {
        using TaskQueueT = Queue<Task>;
        std::atomic_size_t        use_count_;
        std::atomic_uint_fast32_t lock_count_;
        ContextStub*              pnext_ = nullptr;
        SpinLock                  lock_;
        TaskQueueT                task_q_;

        ContextStub()
            : use_count_{1}
            , lock_count_{0}
        {
        }

        void acquire()
        {
            ++use_count_;
        }
        bool release()
        {
            return use_count_.fetch_sub(1) == 1;
        }

        bool lock()
        {
            return lock_count_.fetch_add(1) == 0;
        }

        bool unlock()
        {
            return lock_count_.fetch_sub(1) == 1;
        }
    };

private:
    struct TaskStub {
        std::atomic_uint_fast32_t                           lock_;
        std::aligned_storage_t<sizeof(Task), alignof(Task)> data_;
        ContextStub*                                        pcontext_ = nullptr;
        size_t                                              mcast_id_ = 0;
        TaskStub*                                           pnext_    = nullptr;

        TaskStub()
            : lock_{0}
        {
        }

        void clear()
        {
            pcontext_ = nullptr;
            mcast_id_ = 0;
        }

        Task& task()
        {
            return *std::launder(reinterpret_cast<Task*>(&data_));
        }
        template <class T>
        void task(T&& _rt)
        {
            ::new (&data_) Task(std::forward<T>(_rt));
        }

        void destroy()
        {
            std::destroy_at(std::launder(reinterpret_cast<Task*>(&data_)));
        }

        void waitWhilePush()
        {
            // wait for lock to be 0.
            uint_fast32_t value = lock_.load();
            while (value != 0) {
                lock_.wait(value);
                value = lock_.load();
            }
        }

        void notifyWhilePush()
        {
            lock_.store(1);
            lock_.notify_one();
        }

        void waitWhileStop()
        {
            waitWhilePush();
        }

        void notifyWhileStop()
        {
            lock_.store(2);
            lock_.notify_one();
        }

        bool waitWhilePop()
        {
            // wait for lock to be 1 or 2.
            uint_fast32_t value = lock_.load();
            while (value == 0) {
                lock_.wait(value);
                value = lock_.load();
            }
            return value == 1;
        }

        void notifyWhilePop()
        {
            lock_.store(0);
            lock_.notify_one();
        }
    };
    using ThreadVectorT = std::vector<std::thread>;

    std::atomic<bool>    running_;
    std::atomic_size_t   push_index_;
    std::atomic_size_t   pop_index_;
    std::deque<TaskStub> tasks_;
    ThreadVectorT        threads_;

    Stats statistic_;

    size_t pushIndex()
    {
        return push_index_.fetch_add(1) % tasks_.size();
    }
    size_t popIndex()
    {
        return pop_index_.fetch_add(1) % tasks_.size();
    }

public:
    ThreadPool()
        : running_(false)
        , push_index_(0)
        , pop_index_(0)
    {
    }

    template <
        class StartFnc,
        class StopFnc,
        class TaskFnc,
        class MCastFnc,
        typename... Args>
    void doStart(
        const size_t _thread_count,
        const size_t _capacity,
        StartFnc     _start_fnc,
        StopFnc      _stop_fnc,
        TaskFnc      _task_fnc,
        MCastFnc     _mcast_fnc,
        Args&&... _args);

    void doStop();

    template <class Tsk>
    void doPush(Tsk&& _task, ContextStub* _pctx);

    const Stats& statistic() const
    {
        return statistic_;
    }

    void clearStatistic()
    {
        statistic_.clear();
    }

    void release(ContextStub* _pctx)
    {
        if (_pctx) {
            if (_pctx->release()) {
                delete _pctx;
            }
        }
    }

    ContextStub* doCreateContext();

private:
    template <
        class TaskFnc,
        class MCastFnc,
        typename... Args>
    void doRun(
        const size_t _thread_index,
        TaskFnc&     _task_fnc,
        MCastFnc&    _mcast_fnc,
        Args&&... _args);
};

} // namespace tpimpl

template <class Task, class MCast, class Stats>
class ThreadPool {
    template <class TP, class ContextStub>
    friend class SynchronizationContext;

    using ImplT = tpimpl::ThreadPool<Task, MCast, Stats>;
    using ThisT = ThreadPool<Task, MCast, Stats>;

    ImplT impl_;

public:
    using SynchronizationContextT = SynchronizationContext<ThisT, typename ImplT::ContextStub>;

    ThreadPool() = default;

    template <
        class StartFnc,
        class StopFnc,
        class TaskFnc,
        class MCastFnc,
        typename... Args>
    ThreadPool(
        const size_t _thread_count,
        const size_t _capacity,
        StartFnc     _start_fnc,
        StopFnc      _stop_fnc,
        TaskFnc      _task_fnc,
        MCastFnc     _mcast_fnc,
        Args&&... _args)
    {
        impl_.doStart(
            _thread_count,
            _capacity,
            _start_fnc,
            _stop_fnc,
            _task_fnc,
            _mcast_fnc,
            std::forward<Args>(_args)...);
    }

    template <
        class StartFnc,
        class StopFnc,
        class TaskFnc,
        class MCastFnc,
        typename... Args>
    void start(
        const size_t _thread_count,
        const size_t _capacity,
        StartFnc     _start_fnc,
        StopFnc      _stop_fnc,
        TaskFnc      _task_fnc,
        MCastFnc     _mcast_fnc,
        Args&&... _args)
    {
        impl_.doStart(
            _thread_count,
            _capacity,
            _start_fnc,
            _stop_fnc,
            _task_fnc,
            _mcast_fnc,
            std::forward<Args>(_args)...);
    }

    void stop()
    {
        impl_.doStop();
    }

    ~ThreadPool()
    {
        impl_.doStop();
    }

    SynchronizationContextT createSynchronizationContext()
    {
        return SynchronizationContextT{this, impl_.doCreateContext()};
    }

    const Stats& statistic() const
    {
        return impl_.statistic();
    }

    void clearStatistic()
    {
        impl_.clearStatistic();
    }

    template <class Tsk>
    void push(Tsk&& _task)
    {
        impl_.doPush(std::forward<Tsk>(_task), nullptr);
    }

private:
    void release(typename ImplT::ContextStub* _pctx)
    {
        impl_.release(_pctx);
    }
};

template <class... ArgTypes, size_t TaskFunctionDataSize, size_t MCastFunctionDataSize, class Stats>
class ThreadPool<Function<void(ArgTypes...), TaskFunctionDataSize>, Function<void(ArgTypes...), MCastFunctionDataSize>, Stats> {
    template <class TP, class ContextStub>
    friend class SynchronizationContext;

    using TaskFunctionT  = Function<void(ArgTypes...), TaskFunctionDataSize>;
    using MCastFunctionT = Function<void(ArgTypes...), MCastFunctionDataSize>;
    using ImplT          = tpimpl::ThreadPool<TaskFunctionT, MCastFunctionT, Stats>;
    using ThisT          = ThreadPool<TaskFunctionT, MCastFunctionT, Stats>;

    ImplT impl_;

public:
    using SynchronizationContextT = SynchronizationContext<ThisT, typename ImplT::ContextStub>;

    template <class T>
    static constexpr bool is_small_task_type()
    {
        return TaskFunctionT::template is_small_type<T>();
    }

    template <class T>
    static constexpr bool is_small_mcast_type()
    {
        return MCastFunctionT::template is_small_type<T>();
    }

    ThreadPool() = default;

    template <class StartFnc,
        class StopFnc, typename... Args>
    ThreadPool(
        const size_t _thread_count,
        const size_t _capacity,
        StartFnc     _start_fnc,
        StopFnc      _stop_fnc,
        Args&&... _args)
    {
        impl_.doStart(
            _thread_count,
            _capacity,
            _start_fnc,
            _stop_fnc,
            [](TaskFunctionT& _rfnc, Args&&... _args) {
                _rfnc(std::forward<ArgTypes>(_args)...);
                _rfnc.reset();
            },
            [](MCastFunctionT& _rfnc, Args&&... _args) {
                _rfnc(std::forward<ArgTypes>(_args)...);
            },
            std::forward<Args>(_args)...);
    }

    template <class StartFnc,
        class StopFnc, typename... Args>
    void start(const size_t _thread_count,
        const size_t        _capacity,
        StartFnc            _start_fnc,
        StopFnc             _stop_fnc, Args... _args)
    {
        impl_.doStart(
            _thread_count,
            _capacity,
            _start_fnc,
            _stop_fnc,
            [](TaskFunctionT& _rfnc, Args&&... _args) {
                _rfnc(std::forward<ArgTypes>(_args)...);
                _rfnc.reset();
            },
            [](MCastFunctionT& _rfnc, Args&&... _args) {
                _rfnc(std::forward<ArgTypes>(_args)...);
            },
            std::forward<Args>(_args)...);
    }

    void stop()
    {
        impl_.doStop();
    }

    SynchronizationContextT createSynchronizationContext()
    {
        return SynchronizationContextT{this, impl_.doCreateContext()};
    }

    const Stats& statistic() const
    {
        return impl_.statistic();
    }

    void clearStatistic()
    {
        impl_.clearStatistic();
    }

    template <class Tsk>
    void push(Tsk&& _task)
    {
        impl_.doPush(std::forward<Tsk>(_task), nullptr);
    }

private:
    void release(typename ImplT::ContextStub* _pctx)
    {
        impl_.release(_pctx);
    }
};

namespace tpimpl {
//-----------------------------------------------------------------------------
template <class Task, class MCast, class Stats>
template <class StartFnc,
    class StopFnc,
    class TaskFnc,
    class MCastFnc, typename... Args>
void ThreadPool<Task, MCast, Stats>::doStart(
    const size_t _thread_count,
    const size_t _capacity,
    StartFnc     _start_fnc,
    StopFnc      _stop_fnc,
    TaskFnc      _task_fnc,
    MCastFnc     _mcast_fnc,
    Args&&... _args)
{
    bool expect = false;

    if (!running_.compare_exchange_strong(expect, true)) {
        return;
    }

    tasks_.resize(std::max(_capacity, _thread_count));

    for (size_t i = 0; i < _thread_count; ++i) {
        threads_.emplace_back(
            std::thread{
                [this, i, start_fnc = std::move(_start_fnc), stop_fnc = std::move(_stop_fnc), task_fnc = std::move(_task_fnc), mcast_fnc = std::move(_mcast_fnc)](Args&&... _args) {
                    start_fnc(i, _args...);
                    doRun(i, task_fnc, task_fnc, std::forward<Args>(_args)...);
                    stop_fnc(i, _args...);
                },
                _args...});
    }
}
//-----------------------------------------------------------------------------
template <class Task, class MCast, class Stats>
void ThreadPool<Task, MCast, Stats>::doStop()
{
    bool expect = true;

    if (running_.compare_exchange_strong(expect, false)) {
    } else {
        return;
    }

    for (size_t i = 0; i < threads_.size(); ++i) {
        const auto index = pushIndex();
        auto&      rstub = tasks_[index];

        rstub.waitWhileStop();
        rstub.notifyWhileStop();
    }

    for (auto& t : threads_) {
        t.join();
    }
}
//-----------------------------------------------------------------------------
template <class Task, class MCast, class Stats>
template <
    class TaskFnc,
    class MCastFnc,
    typename... Args>
void ThreadPool<Task, MCast, Stats>::doRun(
    const size_t _thread_index,
    TaskFnc&     _task_fnc,
    MCastFnc&    _mcast_fnc,
    Args&&... _args)
{
    while (true) {
        const size_t index = popIndex();
        auto&        rstub = tasks_[index];

        if (rstub.waitWhilePop()) {
            Task task{std::move(rstub.task())};
            rstub.destroy();

            auto* pctx = rstub.pcontext_;
            rstub.clear();

            rstub.notifyWhilePop();

            if (pctx == nullptr) {
                _task_fnc(task, std::forward<Args>(_args)...);
                continue;
            } else if (pctx->lock()) {
                _task_fnc(task, std::forward<Args>(_args)...);
            } else {
                pctx->lock_.lock();

                const bool exec_now = pctx->lock_count_.load() == 1;

                if (!exec_now) {
                    pctx->task_q_.push(std::move(task));
                    pctx->lock_.unlock();
                    continue;
                } else {
                    pctx->lock_.unlock();
                    _task_fnc(task, std::forward<Args>(_args)...);
                }
            }

            while (!pctx->unlock()) {
                pctx->release();
                pctx->lock_.lock();
                task = std::move(pctx->task_q_.front());
                pctx->lock_.unlock();
                _task_fnc(task, std::forward<Args>(_args)...);
            }

            if (pctx->release()) {
                delete pctx;
            }
        } else {
            rstub.notifyWhilePop();
            break;
        }
    }
}
//-----------------------------------------------------------------------------
template <class Task, class MCast, class Stats>
template <class Tsk>
void ThreadPool<Task, MCast, Stats>::doPush(Tsk&& _task, ContextStub* _pctx)
{
    const auto index = pushIndex();
    auto&      rstub = tasks_[index];

    rstub.waitWhilePush();

    rstub.task(std::forward<Tsk>(_task));
    rstub.pcontext_ = _pctx;
    if (_pctx) {
        _pctx->acquire();
    }
    rstub.notifyWhilePush();
}
//-----------------------------------------------------------------------------
template <class Task, class MCast, class Stats>
typename ThreadPool<Task, MCast, Stats>::ContextStub* ThreadPool<Task, MCast, Stats>::doCreateContext()
{

    return new ContextStub{};
}

//-----------------------------------------------------------------------------
} // namespace tpimpl

} // namespace solid