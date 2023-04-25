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
#include "solid/utility/innerlist.hpp"

namespace solid {

struct ThreadPoolStatistic : solid::Statistic {
    ThreadPoolStatistic();

    std::ostream& print(std::ostream& _ros) const override;
    void          clear();
};

template <class TaskOne, class TaskAll, class Stats = ThreadPoolStatistic>
class ThreadPool;

template <class TP, class ContextStub>
class SynchronizationContext {
    TP*          pthread_pool_ = nullptr;
    ContextStub* pcontext_     = nullptr;

    template <class TaskOne, class TaskAll, class Stats>
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
        pthread_pool_->impl_.doPushOne(std::forward<Task>(_task), pcontext_);
    }
};

namespace tpimpl {

template <class Task>
class TaskData {
    std::aligned_storage_t<sizeof(Task), alignof(Task)> data_;

public:
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
};

template <class Task>
class TaskList {
    enum {
        InnerLinkOrder = 0,
        InnerLinkFree  = 0,
        InnerLinkCount
    };
    struct TaskNode : TaskData<Task>, inner::Node<InnerLinkCount> {
        uint64_t id_ = 0;
    };
    using TaskDqT    = std::deque<TaskNode>;
    using OrderListT = inner::List<TaskDqT, InnerLinkOrder>;
    using FreeListT  = inner::List<TaskDqT, InnerLinkFree>;

    TaskDqT    tasks_;
    OrderListT order_tasks_;
    FreeListT  free_tasks_;

public:
    TaskList()
        : order_tasks_(tasks_)
        , free_tasks_(tasks_)
    {
    }

    void push(Task&& _task, const uint64_t _id)
    {
        size_t index = -1;
        if (!free_tasks_.empty()) {
            index = free_tasks_.backIndex();
            free_tasks_.popBack();
        } else {
            index = tasks_.size();
            tasks_.emplace_back();
        }
        auto& rnode = tasks_[index];
        rnode.id_   = _id;
        rnode.task(std::move(_task));

        if (order_tasks_.empty()) {
            order_tasks_.pushBack(index);
        } else {
            auto tmp_index = order_tasks_.backIndex();
            while (tmp_index != InvalidIndex() && overflow_safe_less(_id, tasks_[tmp_index].id_)) {
                tmp_index = order_tasks_.previousIndex(tmp_index);
            }
            if (tmp_index != InvalidIndex()) {
                order_tasks_.insertAfter(tmp_index, index);
            } else {
                order_tasks_.pushFront(index);
            }
        }
    }

    bool pop(TaskData<Task>& _task_data, const uint64_t _id)
    {
        if (!order_tasks_.empty() && order_tasks_.front().id_ == _id) {
            _task_data.task(std::move(order_tasks_.front().task()));
            order_tasks_.front().destroy();
            free_tasks_.pushBack(order_tasks_.popFront());
            return true;
        }
        return false;
    }
};

/*
NOTE:
    Because we want to execute each TaskOne within the exact TaskAll context
    acquired at pushOne, we cannot notify the Threads on pushAll.
    In case multiple TaskAll were pushed and after that we have a TaskOne,
    the processing will get delayed because we need to build the All context by
    running all the TaskAll.
*/
template <class TaskOne, class TaskAll, class Stats>
class ThreadPool : NonCopyable {
public:
    struct ContextStub {
        using TaskQueueT = TaskList<TaskOne>;
        std::atomic_size_t        use_count_;
        std::atomic_uint_fast64_t produce_id_;
        std::atomic_uint_fast64_t consume_id_;
        ContextStub*              pnext_ = nullptr;
        SpinLock                  spin_;
        TaskQueueT                task_q_;

        ContextStub()
            : use_count_{1}
            , produce_id_{0}
            , consume_id_{0}
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

        void push(TaskOne&& _task, const uint64_t _consume_id)
        {
            task_q_.push(std::move(_task), _consume_id);
        }

        bool pop(TaskData<TaskOne>& _task_data, const uint64_t _consume_id)
        {
            return task_q_.pop(_task_data, _consume_id);
        }
    };

private:
    enum struct LockE : uint_fast32_t {
        Empty = 0,
        Filled,
        Stop,
        Wake,
    };
    struct /*alignas(std::hardware_destructive_interference_size)*/ OneStub : TaskData<TaskOne> {

        std::atomic_flag          pushing_ = ATOMIC_FLAG_INIT;
        std::atomic_flag          popping_ = ATOMIC_FLAG_INIT;
        std::atomic_uint_fast32_t lock_;
        ContextStub*              pcontext_           = nullptr;
        uint64_t                  all_id_             = 0;
        uint64_t                  context_produce_id_ = 0;

        OneStub()
            : lock_{to_underlying(LockE::Empty)}
        {
        }

        void clear() noexcept
        {
            pcontext_           = nullptr;
            all_id_             = 0;
            context_produce_id_ = 0;
        }

        void waitWhilePushOne() noexcept
        {
            while (true) {
                const bool already_pushing = pushing_.test_and_set(std::memory_order_acquire);
                if (!already_pushing) {
                    //  wait for lock to be 0.
                    uint_fast32_t value = lock_.load();
                    while (value != to_underlying(LockE::Empty)) {
                        lock_.wait(value);
                        value = lock_.load();
                    }
                    return;
                } else {
                    pushing_.wait(true);
                }
            }
        }

        void notifyWhilePushOne() noexcept
        {
            lock_.store(to_underlying(LockE::Filled));
            lock_.notify_one();
            pushing_.clear(std::memory_order_release);
            pushing_.notify_one();
        }

        void waitWhileStop() noexcept
        {
            waitWhilePushOne();
        }

        void waitWhilePushAll() noexcept
        {
            waitWhilePushOne();
        }

        void notifyWhileStop() noexcept
        {
            lock_.store(to_underlying(LockE::Stop));
            lock_.notify_one();
            pushing_.clear(std::memory_order_release);
            pushing_.notify_one();
        }

        void notifyWhilePushAll() noexcept
        {
            lock_.store(to_underlying(LockE::Wake));
            lock_.notify_one();
            pushing_.clear(std::memory_order_release);
            pushing_.notify_one();
        }

        LockE waitWhilePop() noexcept
        {
            while (true) {
                const bool already_popping = popping_.test_and_set(std::memory_order_acquire);
                if (!already_popping) {
                    // wait for lock to be 1 or 2.
                    uint_fast32_t value = lock_.load();
                    while (value == to_underlying(LockE::Empty)) {
                        lock_.wait(value);
                        value = lock_.load();
                    }
                    return static_cast<LockE>(value);
                } else {
                    popping_.wait(true);
                }
            }
        }

        void notifyWhilePop() noexcept
        {
            lock_.store(to_underlying(LockE::Empty));
            lock_.notify_one();
            popping_.clear(std::memory_order_release);
            popping_.notify_one();
        }
    };

    struct /*alignas(std::hardware_destructive_interference_size)*/ AllStub : TaskData<TaskAll> {
        std::atomic_flag          pushing_ = ATOMIC_FLAG_INIT;
        std::atomic_uint_fast32_t lock_;
        std::atomic_uint_fast32_t use_count_;

        AllStub()
            : lock_{to_underlying(LockE::Empty)}
            , use_count_{0}
        {
        }

        void clear() noexcept
        {
        }

        void waitWhilePushAll() noexcept
        {
            while (true) {
                const bool already_pushing = pushing_.test_and_set(std::memory_order_acquire);
                if (!already_pushing) {
                    //  wait for lock to be 0.
                    uint_fast32_t value = lock_.load();
                    while (value != to_underlying(LockE::Empty)) {
                        lock_.wait(value);
                        value = lock_.load();
                    }
                    return;
                } else {
                    pushing_.wait(true);
                }
            }
        }

        void notifyWhilePushAll() noexcept
        {
            use_count_.store(threads_.size());
            lock_.store(to_underlying(LockE::Filled));
            pushing_.clear(std::memory_order_release);
        }
    };

    using ThreadVectorT = std::vector<std::thread>;

    std::atomic<bool>          running_;
    std::atomic_size_t         push_one_index_;
    std::atomic_size_t         pop_one_index_;
    std::atomic_uint_fast64_t  push_all_index_;
    std::atomic_uint_fast64_t  pop_all_index_;
    std::atomic_uint_fast64_t  commited_all_index_;
    size_t                     one_capacity_ = 0;
    size_t                     all_capacity_ = 0;
    std::unique_ptr<OneStub[]> one_tasks_;
    std::unique_ptr<AllStub[]> all_tasks_;
    ThreadVectorT              threads_;
    Stats                      statistic_;

    size_t pushOneIndex() noexcept
    {
        return push_one_index_.fetch_add(1) % one_capacity_;
    }
    size_t popOneIndex() noexcept
    {
        return pop_one_index_.fetch_add(1) % one_capacity_;
    }

    uint64_t pushAllIndex() noexcept
    {
        return push_all_index_.fetch_add(1) % all_capacity_;
    }
    size_t popAllIndex() noexcept
    {
        return pop_all_index_.fetch_add(1) % all_capacity_;
    }

    void commitAllIndex(const uint64_t _index)
    {
        uint64_t index = _index - 1;
        while (!commited_all_index_.compare_exchange_weak(index, _index)) {
            index = _index - 1;
            cpu_pause();
        }
    }

    bool shouldWakeThreads(const uint64_t _index) const
    {
        return (pop_all_index_.load() + 1) == _index;
    }

public:
    ThreadPool()
        : running_(false)
        , push_one_index_(0)
        , pop_one_index_(0)
        , commited_all_index_(0)
    {
    }

    template <
        class StartFnc,
        class StopFnc,
        class OneFnc,
        class AllFnc,
        typename... Args>
    void doStart(
        const size_t _thread_count,
        const size_t _one_capacity,
        const size_t _all_capacity,
        StartFnc     _start_fnc,
        StopFnc      _stop_fnc,
        OneFnc       _one_fnc,
        AllFnc       _all_fnc,
        Args&&... _args);

    void doStop();

    template <class Tsk>
    void doPushOne(Tsk&& _task, ContextStub* _pctx);

    template <class Tsk>
    void doPushAll(Tsk&& _task);

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
        class OneFnc,
        class AllFnc,
        typename... Args>
    void doRun(
        const size_t _thread_index,
        OneFnc&      _one_fnc,
        AllFnc&      _all_fnc,
        Args&&... _args);
};

} // namespace tpimpl

template <class TaskOne, class TaskAll, class Stats>
class ThreadPool {
    template <class TP, class ContextStub>
    friend class SynchronizationContext;

    using ImplT = tpimpl::ThreadPool<TaskOne, TaskAll, Stats>;
    using ThisT = ThreadPool<TaskOne, TaskAll, Stats>;

    ImplT impl_;

public:
    using SynchronizationContextT = SynchronizationContext<ThisT, typename ImplT::ContextStub>;

    ThreadPool() = default;

    template <
        class StartFnc,
        class StopFnc,
        class OneFnc,
        class AllFnc,
        typename... Args>
    ThreadPool(
        const size_t _thread_count,
        const size_t _one_capacity,
        const size_t _all_capacity,
        StartFnc     _start_fnc,
        StopFnc      _stop_fnc,
        OneFnc       _one_fnc,
        AllFnc       _all_fnc,
        Args&&... _args)
    {
        impl_.doStart(
            _thread_count,
            _one_capacity,
            _all_capacity,
            _start_fnc,
            _stop_fnc,
            _one_fnc,
            _all_fnc,
            std::forward<Args>(_args)...);
    }

    template <
        class StartFnc,
        class StopFnc,
        class OneFnc,
        class AllFnc,
        typename... Args>
    void start(
        const size_t _thread_count,
        const size_t _one_capacity,
        const size_t _all_capacity,
        StartFnc     _start_fnc,
        StopFnc      _stop_fnc,
        OneFnc       _one_fnc,
        AllFnc       _all_fnc,
        Args&&... _args)
    {
        impl_.doStart(
            _thread_count,
            _one_capacity,
            _all_capacity,
            _start_fnc,
            _stop_fnc,
            _one_fnc,
            _all_fnc,
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
    void pushOne(Tsk&& _task)
    {
        impl_.doPushOne(std::forward<Tsk>(_task), nullptr);
    }

    template <class Tsk>
    void pushAll(Tsk&& _task)
    {
        impl_.doPushAll(std::forward<Tsk>(_task));
    }

private:
    void release(typename ImplT::ContextStub* _pctx)
    {
        impl_.release(_pctx);
    }
};

template <class... ArgTypes, size_t OneFunctionDataSize, size_t AllFunctionDataSize, class Stats>
class ThreadPool<Function<void(ArgTypes...), OneFunctionDataSize>, Function<void(ArgTypes...), AllFunctionDataSize>, Stats> {
    template <class TP, class ContextStub>
    friend class SynchronizationContext;

    using OneFunctionT = Function<void(ArgTypes...), OneFunctionDataSize>;
    using AllFunctionT = Function<void(ArgTypes...), AllFunctionDataSize>;
    using ImplT        = tpimpl::ThreadPool<OneFunctionT, AllFunctionT, Stats>;
    using ThisT        = ThreadPool<OneFunctionT, AllFunctionT, Stats>;

    ImplT impl_;

public:
    using SynchronizationContextT = SynchronizationContext<ThisT, typename ImplT::ContextStub>;

    template <class T>
    static constexpr bool is_small_one_type()
    {
        return OneFunctionT::template is_small_type<T>();
    }

    template <class T>
    static constexpr bool is_small_all_type()
    {
        return AllFunctionT::template is_small_type<T>();
    }

    ThreadPool() = default;

    template <class StartFnc,
        class StopFnc, typename... Args>
    ThreadPool(
        const size_t _thread_count,
        const size_t _one_capacity,
        const size_t _all_capacity,
        StartFnc     _start_fnc,
        StopFnc      _stop_fnc,
        Args&&... _args)
    {
        impl_.doStart(
            _thread_count,
            _one_capacity,
            _all_capacity,
            _start_fnc,
            _stop_fnc,
            [](OneFunctionT& _rfnc, Args&&... _args) {
                _rfnc(std::forward<ArgTypes>(_args)...);
                _rfnc.reset();
            },
            [](AllFunctionT& _rfnc, Args&&... _args) {
                _rfnc(std::forward<ArgTypes>(_args)...);
            },
            std::forward<Args>(_args)...);
    }

    template <class StartFnc,
        class StopFnc, typename... Args>
    void start(const size_t _thread_count,
        const size_t        _one_capacity,
        const size_t        _all_capacity,
        StartFnc            _start_fnc,
        StopFnc             _stop_fnc, Args... _args)
    {
        impl_.doStart(
            _thread_count,
            _one_capacity,
            _all_capacity,
            _start_fnc,
            _stop_fnc,
            [](OneFunctionT& _rfnc, Args&&... _args) {
                _rfnc(std::forward<ArgTypes>(_args)...);
                _rfnc.reset();
            },
            [](AllFunctionT& _rfnc, Args&&... _args) {
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
    void pushOne(Tsk&& _task)
    {
        impl_.doPushOne(std::forward<Tsk>(_task), nullptr);
    }

    template <class Tsk>
    void pushAll(Tsk&& _task)
    {
        impl_.doPushAll(std::forward<Tsk>(_task));
    }

private:
    void release(typename ImplT::ContextStub* _pctx)
    {
        impl_.release(_pctx);
    }
};

namespace tpimpl {
//-----------------------------------------------------------------------------
template <class TaskOne, class TaskAll, class Stats>
template <class StartFnc,
    class StopFnc,
    class OneFnc,
    class AllFnc, typename... Args>
void ThreadPool<TaskOne, TaskAll, Stats>::doStart(
    const size_t _thread_count,
    const size_t _one_capacity,
    const size_t _all_capacity,
    StartFnc     _start_fnc,
    StopFnc      _stop_fnc,
    OneFnc       _one_fnc,
    AllFnc       _all_fnc,
    Args&&... _args)
{
    bool expect = false;

    if (!running_.compare_exchange_strong(expect, true)) {
        return;
    }

    // tasks_.resize(std::max(_capacity, _thread_count));
    one_capacity_ = _one_capacity;
    one_tasks_.reset(new OneStub[_one_capacity]);
    all_capacity_ = _all_capacity;
    all_tasks_.reset(new AllStub[_all_capacity]);

    for (size_t i = 0; i < _thread_count; ++i) {
        threads_.emplace_back(
            std::thread{
                [this, i, start_fnc = std::move(_start_fnc), stop_fnc = std::move(_stop_fnc), one_fnc = std::move(_one_fnc), all_fnc = std::move(_all_fnc)](Args&&... _args) {
                    start_fnc(i, _args...);
                    doRun(i, one_fnc, all_fnc, std::forward<Args>(_args)...);
                    stop_fnc(i, _args...);
                },
                _args...});
    }
}
//-----------------------------------------------------------------------------
template <class TaskOne, class TaskAll, class Stats>
void ThreadPool<TaskOne, TaskAll, Stats>::doStop()
{
    bool expect = true;

    if (running_.compare_exchange_strong(expect, false)) {
    } else {
        return;
    }

    for (size_t i = 0; i < threads_.size(); ++i) {
        auto& rstub = one_tasks_[pushOneIndex()];

        rstub.waitWhileStop();
        rstub.notifyWhileStop();
    }

    for (auto& t : threads_) {
        t.join();
    }
}
//-----------------------------------------------------------------------------
template <class TaskOne, class TaskAll, class Stats>
template <
    class OneFnc,
    class AllFnc,
    typename... Args>
void ThreadPool<TaskOne, TaskAll, Stats>::doRun(
    const size_t _thread_index,
    OneFnc&      _one_fnc,
    AllFnc&      _all_fnc,
    Args&&... _args)
{
    uint64_t local_all_id = 0;
    while (true) {
        const size_t index = popOneIndex();
        auto&        rstub = one_tasks_[index];
        const auto   lock  = rstub.waitWhilePop();

        if (lock == LockE::Filled) {
            auto  context_produce_id = rstub.context_produce_id_;
            auto* pctx               = rstub.pcontext_;
            const auto all_id = rstub.all_id_;
            {
                TaskOne task{std::move(rstub.task())};

                rstub.destroy();
                rstub.clear();

                rstub.notifyWhilePop();

                consumeAll(local_all_id, all_id);

                if (pctx == nullptr) {
                    _one_fnc(task, std::forward<Args>(_args)...);
                    continue;
                } else if (context_produce_id == pctx->consume_id_.load()) {
                    _one_fnc(task, std::forward<Args>(_args)...);
                } else {
                    pctx->spin_.lock();
                    if (context_produce_id != pctx->consume_id_.load()) {
                        pctx->push(std::move(task), context_produce_id);
                        pctx->spin_.unlock();
                        continue;
                    } else {
                        pctx->spin_.unlock();
                        _one_fnc(task, std::forward<Args>(_args)...);
                    }
                }
            }

            context_produce_id = pctx->consume_id_.fetch_add(1) + 1;

            do {
                TaskData<TaskOne> task_data;
                {
                    SpinGuardT lock{pctx->spin_};
                    if (pctx->pop(task_data, context_produce_id)) {
                    } else {
                        break;
                    }
                }

                _one_fnc(task_data.task(), std::forward<Args>(_args)...);

                task_data.destroy();

                context_produce_id = pctx->consume_id_.fetch_add(1) + 1;
                pctx->release();
            } while (true);

            if (pctx->release()) {
                delete pctx;
            }

            consumeAll(local_all_id);
        } else if (lock == LockE::Wake) {
            rstub.notifyWhilePop();
            consumeAll(local_all_id);
        } else if (lock == LockE::Stop) {
            rstub.notifyWhilePop();
            consumeAll(local_all_id);
            break;
        }
    }
}
//-----------------------------------------------------------------------------
template <class TaskOne, class TaskAll, class Stats>
template <class Tsk>
void ThreadPool<TaskOne, TaskAll, Stats>::consumeAll()
{

}
//-----------------------------------------------------------------------------
template <class TaskOne, class TaskAll, class Stats>
template <class Tsk>
void ThreadPool<TaskOne, TaskAll, Stats>::consumeAll(const uint64_t _all_id)
{

}
//-----------------------------------------------------------------------------
template <class TaskOne, class TaskAll, class Stats>
template <class Tsk>
void ThreadPool<TaskOne, TaskAll, Stats>::doPushOne(Tsk&& _task, ContextStub* _pctx)
{
    const auto index = pushOneIndex();
    auto&      rstub = one_tasks_[index];

    rstub.waitWhilePushOne();

    rstub.task(std::forward<Tsk>(_task));
    rstub.pcontext_ = _pctx;
    rstub.all_id_   = commited_all_index_.load();

    if (_pctx) {
        _pctx->acquire();
        rstub.context_produce_id_ = _pctx->produce_id_.fetch_add(1);
    }

    rstub.notifyWhilePushOne();
}
//-----------------------------------------------------------------------------
// NOTE:
// we cannot guarantee that a onetask at X (OT_X) and another onetask at X+a (OT_Xa) will 
//  always have OT_X.all_id < OT_Xa.all_id. This means that we cannot guarantee that
//  a OneTask will always be consumed within the exact all_id context (it might be a newer context).
template <class TaskOne, class TaskAll, class Stats>
template <class Tsk>
void ThreadPool<TaskOne, TaskAll, Stats>::doPushAll(Tsk&& _task)
{
    const auto index = pushAllIndex();
    auto&      rstub = all_tasks_[index];

    rstub.waitWhilePushAll();

    rstub.task(std::forward<Tsk>(_task));

    rstub.notifyWhilePushAll();

    commitAllIndex(index);

    if (shouldWakeThreads(index)) {
        for (size_t i = 0; i < threads_.size(); ++i) {
            auto& rstub = one_tasks_[pushOneIndex()];

            rstub.waitWhilePushAll();

            rstub.all_id_ = index;

            rstub.notifyWhilePushAll();
        }
    }
}
//-----------------------------------------------------------------------------
template <class TaskOne, class TaskAll, class Stats>
typename ThreadPool<TaskOne, TaskAll, Stats>::ContextStub* ThreadPool<TaskOne, TaskAll, Stats>::doCreateContext()
{
    return new ContextStub{};
}

//-----------------------------------------------------------------------------
} // namespace tpimpl

} // namespace solid