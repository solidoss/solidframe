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
#include <atomic>
#include <bit>
#include <deque>
#include <functional>
#include <thread>

#if !defined(__cpp_lib_atomic_wait)
#include "solid/utility/atomic_wait"
#endif
#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/system/spinlock.hpp"
#include "solid/system/statistic.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/innerlist.hpp"

namespace solid {

struct ThreadPoolStatistic : solid::Statistic {
    std::atomic_uint_fast64_t create_context_count_      = {0};
    std::atomic_uint_fast64_t delete_context_count_      = {0};
    std::atomic_uint_fast64_t run_one_free_count_        = {0};
    std::atomic_uint_fast64_t max_run_one_free_count_    = {0};
    std::atomic_uint_fast64_t run_one_context_count_     = {0};
    std::atomic_uint_fast64_t max_run_one_context_count_ = {0};
    std::atomic_uint_fast64_t run_one_push_count_        = {0};
    std::atomic_uint_fast64_t max_run_one_push_count_    = {0};
    std::atomic_uint_fast64_t run_all_wake_count_        = {0};
    std::atomic_uint_fast64_t max_run_all_wake_count_    = {0};
    std::atomic_uint_fast64_t push_one_count_[2];
    std::atomic_uint_fast64_t push_all_count_              = {0};
    std::atomic_uint_fast64_t push_all_wake_count_         = {0};
    std::atomic_uint_fast64_t max_consume_all_count_       = {0};
    std::atomic_uint_fast64_t run_all_count_               = {0};
    std::atomic_uint_fast64_t max_run_all_count_           = {0};
    std::atomic_uint_fast64_t push_one_wait_lock_count_    = {0};
    std::atomic_uint_fast64_t push_one_wait_pushing_count_ = {0};
    std::atomic_uint_fast64_t pop_one_wait_lock_count_     = {0};
    std::atomic_uint_fast64_t pop_one_wait_popping_count_  = {0};
    std::atomic_uint_fast64_t push_all_wait_lock_count_    = {0};
    std::atomic_uint_fast64_t push_all_wait_pushing_count_ = {0};
    std::atomic_uint_fast64_t push_one_latency_min_us_     = {0};
    std::atomic_uint_fast64_t push_one_latency_max_us_     = {0};
    std::atomic_uint_fast64_t push_one_latency_sum_us_     = {0};

    ThreadPoolStatistic();

    void createContext()
    {
        ++create_context_count_;
    }
    void deleteContext()
    {
        ++delete_context_count_;
    }

    void runOneFreeCount(const uint64_t _count)
    {
        ++run_one_free_count_;
        solid_statistic_max(max_run_one_free_count_, _count);
    }
    void runOneContextPush(const uint64_t _count)
    {
        ++run_one_push_count_;
        solid_statistic_max(max_run_one_push_count_, _count);
    }
    void runOneContextCount(const uint64_t _inc, const uint64_t _count)
    {
        run_one_context_count_ += _inc;
        solid_statistic_max(max_run_one_context_count_, _count);
    }
    void runWakeCount(const uint64_t _count)
    {
        ++run_all_wake_count_;
        solid_statistic_max(max_run_all_wake_count_, _count);
    }

    void tryConsumeAnAllTaskFilled(const bool _should_retry, const uint64_t _count)
    {
        ++run_all_count_;
        solid_statistic_max(max_run_all_count_, _count);
    }

    void tryConsumeAnAllTaskNotFilled(const bool _should_retry)
    {
    }

    void consumeAll(const uint64_t _count)
    {
        solid_statistic_max(max_consume_all_count_, _count);
    }

    void pushOne(const bool _with_context, const uint64_t _duration_us)
    {
        ++push_one_count_[_with_context];

        solid_statistic_min(push_one_latency_min_us_, _duration_us);
        solid_statistic_max(push_one_latency_max_us_, _duration_us);
        push_one_latency_sum_us_ += _duration_us;
    }
    void pushAll(const bool _should_wake)
    {
        ++push_all_count_;
    }
    void pushOneWaitLock()
    {
        ++push_one_wait_lock_count_;
    }
    void pushOneWaitPushing()
    {
        ++push_one_wait_pushing_count_;
    }
    void popOneWaitLock()
    {
        ++pop_one_wait_lock_count_;
    }
    void popOneWaitPopping()
    {
        ++pop_one_wait_popping_count_;
    }
    void pushAllWaitLock()
    {
        ++push_all_wait_lock_count_;
    }
    void pushAllWaitPushing()
    {
        ++push_all_wait_pushing_count_;
    }

    std::ostream& print(std::ostream& _ros) const override;
    void          clear();
};

struct EmptyThreadPoolStatistic : solid::Statistic {

    void createContext() {}
    void deleteContext() {}

    void runOneFreeCount(const uint64_t _count) {}
    void runOneContextPush(const uint64_t _count) {}
    void runOneContextCount(const uint64_t _inc, const uint64_t _count) {}
    void runWakeCount(const uint64_t _count) {}

    void tryConsumeAnAllTaskFilled(const bool _should_retry, const uint64_t _count) {}
    void tryConsumeAnAllTaskNotFilled(const bool _should_retry) {}
    void consumeAll(const uint64_t _count) {}
    void pushOne(const bool _with_context) {}
    void pushAll(const bool _should_wake) {}
    void pushOneWaitLock() {}
    void pushOneWaitPushing() {}
    void popOneWaitLock() {}
    void popOneWaitPopping() {}
    void pushAllWaitLock() {}
    void pushAllWaitPushing() {}

    std::ostream& print(std::ostream& _ros) const override { return _ros; }
    void          clear() {}
};

struct ThreadPoolConfiguration {
    static constexpr size_t default_one_capacity = 8 * 1024;
    static constexpr size_t default_all_capacity = 1024;

    size_t thread_count_ = 1;
    size_t one_capacity_ = default_one_capacity;
    size_t all_capacity_ = default_all_capacity;
    size_t spin_count_   = 1;

    ThreadPoolConfiguration(
        const size_t _thread_count = 1,
        const size_t _one_capacity = 10 * 1024,
        const size_t _all_capacity = 1024,
        const size_t _spin_count   = 1)
        : thread_count_(_thread_count)
        , one_capacity_(_one_capacity)
        , all_capacity_(_all_capacity)
        , spin_count_(_spin_count)
    {
    }

    auto& threadCount(const size_t _value)
    {
        thread_count_ = _value;
        return *this;
    }
    auto& oneCapacity(const size_t _value)
    {
        one_capacity_ = _value;
        return *this;
    }
    auto& allCapacity(const size_t _value)
    {
        all_capacity_ = _value;
        return *this;
    }

    auto& spinCount(const size_t _value)
    {
        spin_count_ = _value;
        return *this;
    }
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

    SynchronizationContext(SynchronizationContext&& _other) noexcept
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

    SynchronizationContext& operator=(SynchronizationContext&& _other) noexcept
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
class alignas(hardware_destructive_interference_size) TaskData {
    std::aligned_storage_t<sizeof(Task), alignof(Task)> data_;

public:
    Task& task() noexcept
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
        uint64_t id_     = 0;
        uint64_t all_id_ = 0;
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

    void push(Task&& _task, const uint64_t _all_id, const uint64_t _id)
    {
        size_t index = -1;
        if (!free_tasks_.empty()) {
            index = free_tasks_.backIndex();
            free_tasks_.popBack();
        } else {
            index = tasks_.size();
            tasks_.emplace_back();
        }
        auto& rnode   = tasks_[index];
        rnode.id_     = _id;
        rnode.all_id_ = _all_id;
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

    bool pop(TaskData<Task>& _task_data, uint64_t& _rall_id, const uint64_t _id)
    {
        if (!order_tasks_.empty() && order_tasks_.front().id_ == _id) {
            _task_data.task(std::move(order_tasks_.front().task()));
            _rall_id = order_tasks_.front().all_id_;
            order_tasks_.front().destroy();
            free_tasks_.pushBack(order_tasks_.popFront());
            return true;
        }
        return false;
    }
};
struct LocalContext {
    uint64_t next_all_id_            = 1;
    uint64_t all_count_              = 0;
    uint64_t one_free_count_         = 0;
    uint64_t one_context_count_      = 0;
    uint64_t one_context_push_count_ = 0;
    uint64_t wake_count_             = 0;
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
    static constexpr size_t spin_count = 10;
    struct ContextStub {
        using TaskQueueT = TaskList<TaskOne>;
        std::atomic_size_t        use_count_{1};
        std::atomic_uint_fast64_t produce_id_{0};
        SpinLock                  spin_;
        TaskQueueT                task_q_;
        alignas(hardware_destructive_interference_size) std::atomic_uint_fast64_t consume_id_{0};

        void acquire()
        {
            ++use_count_;
        }

        bool release()
        {
            return use_count_.fetch_sub(1) == 1;
        }

        void push(TaskOne&& _task, const uint64_t _all_id, const uint64_t _consume_id)
        {
            task_q_.push(std::move(_task), _all_id, _consume_id);
        }

        bool pop(TaskData<TaskOne>& _task_data, uint64_t& _rall_id, const uint64_t _consume_id)
        {
            return task_q_.pop(_task_data, _rall_id, _consume_id);
        }
    };

private:
    enum struct EventE : uint8_t {
        Fill,
        Stop,
        Wake,
    };

    using AtomicCounterT      = std::atomic<uint8_t>;
    using AtomicCounterValueT = AtomicCounterT::value_type;
    template <class IndexT>
    inline constexpr static auto computeCounter(const IndexT _index, const size_t _capacity) noexcept
    {
        return (_index / _capacity) & std::numeric_limits<AtomicCounterValueT>::max();
    }

    struct OneStub {
        AtomicCounterT     produce_count_{0};
        AtomicCounterT     consume_count_{static_cast<AtomicCounterValueT>(-1)};
        std::uint8_t       event_              = {to_underlying(EventE::Fill)};
        TaskData<TaskOne>* data_ptr_           = nullptr;
        ContextStub*       pcontext_           = nullptr;
        uint64_t           all_id_             = 0;
        uint64_t           context_produce_id_ = 0;

        auto& task() noexcept
        {
            return data_ptr_->task();
        }
        template <class T>
        void task(T&& _rt)
        {
            data_ptr_->task(std::forward<T>(_rt));
        }

        void destroy()
        {
            data_ptr_->destroy();
        }

        void clear() noexcept
        {
            pcontext_           = nullptr;
            all_id_             = 0;
            context_produce_id_ = 0;
        }

        void waitWhilePushOne(Stats& _rstats, const AtomicCounterValueT _count, const size_t _spin_count) noexcept
        {
            auto spin = _spin_count;
            while (true) {
                const auto cnt = produce_count_.load();
                if (cnt == _count) {
                    break;
                } else if (_spin_count && !spin--) {
                    _rstats.pushOneWaitLock();
                    std::atomic_wait_explicit(&produce_count_, cnt, std::memory_order_relaxed);
                    spin = _spin_count;
                }
            }
        }

        void notifyWhilePushOne(std::chrono::time_point<std::chrono::steady_clock> const& _start, uint64_t& _rduration) noexcept
        {
            using namespace std::chrono;
            event_ = to_underlying(EventE::Fill);
            ++consume_count_;
            std::atomic_notify_all(&consume_count_);
            _rduration = duration_cast<microseconds>(steady_clock::now() - _start).count();
        }

        void waitWhileStop(Stats& _rstats, const AtomicCounterValueT _count, const size_t _spin_count) noexcept
        {
            waitWhilePushOne(_rstats, _count, _spin_count);
        }

        void waitWhilePushAll(Stats& _rstats, const AtomicCounterValueT _count, const size_t _spin_count) noexcept
        {
            waitWhilePushOne(_rstats, _count, _spin_count);
        }

        void notifyWhileStop() noexcept
        {
            event_ = to_underlying(EventE::Stop);
            ++consume_count_;
            std::atomic_notify_all(&consume_count_);
        }

        void notifyWhilePushAll() noexcept
        {
            event_ = to_underlying(EventE::Wake);
            ++consume_count_;
            std::atomic_notify_all(&consume_count_);
        }

        template <
            class Fnc,
            class AllFnc,
            typename... Args>
        EventE waitWhilePop(Stats& _rstats, const AtomicCounterValueT _count, const size_t _spin_count, const Fnc& _try_consume_an_all_fnc, AllFnc& _all_fnc, Args&&... _args) noexcept
        {
            auto spin = _spin_count;
            while (true) {
                const auto cnt = consume_count_.load();
                if (cnt == _count) {
                    return static_cast<EventE>(event_);
                } else if (!_try_consume_an_all_fnc(&consume_count_, _count, _all_fnc, std::forward<Args>(_args)...) && _spin_count && !spin--) {

                    std::atomic_wait_explicit(&consume_count_, cnt, std::memory_order_relaxed);

                    _rstats.popOneWaitPopping();
                    spin = _spin_count;
                }
            }
        }

        void notifyWhilePop() noexcept
        {
            ++produce_count_;
            std::atomic_notify_all(&produce_count_);
        }
    };

    struct AllStub {
        AtomicCounterT       produce_count_{0};
        AtomicCounterT       consume_count_{static_cast<AtomicCounterValueT>(-1)};
        std::atomic_uint32_t use_count_ = {0};
        std::atomic_uint64_t id_        = {0};
        TaskData<TaskAll>*   data_ptr_  = nullptr;

        auto& task() noexcept
        {
            return data_ptr_->task();
        }
        template <class T>
        void task(T&& _rt)
        {
            data_ptr_->task(std::forward<T>(_rt));
        }

        void destroy()
        {
            data_ptr_->destroy();
        }

        void waitWhilePushAll(Stats& _rstats, const AtomicCounterValueT _count, const size_t _spin_count) noexcept
        {
            auto spin = _spin_count;
            while (true) {
                const auto cnt = produce_count_.load();
                if (cnt == _count) {
                    break;
                } else if (_spin_count && !spin--) {
                    _rstats.pushOneWaitLock();
                    std::atomic_wait_explicit(&produce_count_, cnt, std::memory_order_relaxed);
                    spin = _spin_count;
                }
            }
        }

        void notifyWhilePushAll(const uint32_t _thread_count, const uint64_t _id) noexcept
        {
            use_count_.store(_thread_count);
            id_.store(_id);
            ++consume_count_;
        }

        bool notifyWhilePop() noexcept
        {
            if (use_count_.fetch_sub(1) == 1) {
                destroy();
                ++produce_count_;
                std::atomic_notify_all(&produce_count_);
                return true;
            }
            return false;
        }

        bool isFilled(const uint64_t _id, const size_t _capacity) const
        {
            const auto                count          = consume_count_.load(std::memory_order_relaxed);
            const AtomicCounterValueT expected_count = computeCounter(_id, _capacity);
            return count == expected_count && id_.load() == _id;
        }
    };
    using AllStubT      = AllStub;
    using OneStubT      = OneStub;
    using ThreadVectorT = std::vector<std::thread>;

    size_t spin_count_ = 1;
    /* alignas(hardware_constructive_interference_size) */ struct {
        size_t                               capacity_{0};
        std::unique_ptr<OneStubT[]>          tasks_;
        std::unique_ptr<TaskData<TaskOne>[]> datas_;
    } one_;

    /* alignas(hardware_constructive_interference_size) */ struct {
        size_t                               capacity_{0};
        std::atomic_size_t                   pending_count_{0};
        std::atomic_uint_fast64_t            push_index_{1};
        std::atomic_uint_fast64_t            commited_index_{0};
        std::unique_ptr<AllStubT[]>          tasks_;
        std::unique_ptr<TaskData<TaskAll>[]> datas_;
    } all_;

    Stats statistic_;
    using AtomicIndexT      = std::atomic_size_t;
    using AtomicIndexValueT = std::atomic_size_t::value_type;

    alignas(hardware_destructive_interference_size) AtomicIndexT push_one_index_{0};
    alignas(hardware_destructive_interference_size) AtomicIndexT pop_one_index_{0};
    ThreadVectorT     threads_;
    std::atomic<bool> running_{false};

    std::tuple<AtomicIndexValueT, AtomicCounterValueT> pushOneIndex() noexcept
    {
        const auto index = push_one_index_.fetch_add(1);
        return {index % one_.capacity_, computeCounter(index, one_.capacity_)};
    }
    std::tuple<AtomicIndexValueT, AtomicCounterValueT> popOneIndex() noexcept
    {
        const auto index = pop_one_index_.fetch_add(1);
        return {index % one_.capacity_, computeCounter(index, one_.capacity_)};
    }

    auto pushAllId() noexcept
    {
        return all_.push_index_.fetch_add(1);
    }

    void commitAllId(const uint64_t _id)
    {
        uint64_t id = _id - 1;
        while (!all_.commited_index_.compare_exchange_weak(id, _id)) {
            id = _id - 1;
            cpu_pause();
        }
    }

public:
    ThreadPool() = default;

    template <
        class StartFnc,
        class StopFnc,
        class OneFnc,
        class AllFnc,
        typename... Args>
    void doStart(
        const ThreadPoolConfiguration& _config,
        StartFnc                       _start_fnc,
        StopFnc                        _stop_fnc,
        OneFnc                         _one_fnc,
        AllFnc                         _all_fnc,
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
                statistic_.deleteContext();
                delete _pctx;
            }
        }
    }

    ContextStub* doCreateContext();

    size_t capacityOne() const
    {
        return one_.capacity_;
    }
    size_t capacityAll() const
    {
        return all_.capacity_;
    }

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

    template <
        class AllFnc,
        typename... Args>
    bool tryConsumeAnAllTask(AtomicCounterT* _pcounter,
        const AtomicCounterValueT _count, LocalContext& _rlocal_context, AllFnc& _all_fnc, Args&&... _args);
    template <
        class AllFnc,
        typename... Args>
    void consumeAll(LocalContext& _rlocal_context, const uint64_t _all_id, AllFnc& _all_fnc, Args&&... _args);
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
    using ConfigurationT          = ThreadPoolConfiguration;

    ThreadPool() = default;

    template <
        class StartFnc,
        class StopFnc,
        class OneFnc,
        class AllFnc,
        typename... Args>
    ThreadPool(
        const ThreadPoolConfiguration& _config,
        StartFnc                       _start_fnc,
        StopFnc                        _stop_fnc,
        OneFnc                         _one_fnc,
        AllFnc                         _all_fnc,
        Args&&... _args)
    {
        impl_.doStart(
            _config,
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
        const ThreadPoolConfiguration& _config,
        StartFnc                       _start_fnc,
        StopFnc                        _stop_fnc,
        OneFnc                         _one_fnc,
        AllFnc                         _all_fnc,
        Args&&... _args)
    {
        impl_.doStart(
            _config,
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
    size_t capacityOne() const
    {
        return impl_.capacityOne();
    }
    size_t capacityAll() const
    {
        return impl_.capacityAll();
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
    using ConfigurationT          = ThreadPoolConfiguration;

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
        const ThreadPoolConfiguration& _config,
        StartFnc                       _start_fnc,
        StopFnc                        _stop_fnc,
        Args&&... _args)
    {
        impl_.doStart(
            _config,
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
    void start(const ThreadPoolConfiguration& _config,
        StartFnc                              _start_fnc,
        StopFnc                               _stop_fnc, Args... _args)
    {
        impl_.doStart(
            _config,
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
    size_t capacityOne() const
    {
        return impl_.capacityOne();
    }
    size_t capacityAll() const
    {
        return impl_.capacityAll();
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
template <
    class StartFnc,
    class StopFnc,
    class OneFnc,
    class AllFnc,
    typename... Args>
void ThreadPool<TaskOne, TaskAll, Stats>::doStart(
    const ThreadPoolConfiguration& _config,
    StartFnc                       _start_fnc,
    StopFnc                        _stop_fnc,
    OneFnc                         _one_fnc,
    AllFnc                         _all_fnc,
    Args&&... _args)
{
    static_assert(
        (std::numeric_limits<AtomicIndexValueT>::max() % std::bit_ceil(ThreadPoolConfiguration::default_one_capacity)) == (std::bit_ceil(ThreadPoolConfiguration::default_one_capacity) - 1) && (std::numeric_limits<AtomicIndexValueT>::max() % std::bit_ceil(ThreadPoolConfiguration::default_all_capacity)) == (std::bit_ceil(ThreadPoolConfiguration::default_all_capacity) - 1));
    bool expect = false;

    if (!running_.compare_exchange_strong(expect, true)) {
        return;
    }

    solid_dbg(generic_logger, Error, "sizeof(OneStub) = " << sizeof(OneStubT) << " sizeof(AllStub) = " << sizeof(AllStubT));

    threads_.clear();

    const auto thread_count = _config.thread_count_ ? _config.thread_count_ : std::thread::hardware_concurrency();
    threads_.reserve(thread_count);

    one_.capacity_ = std::bit_ceil(std::max(_config.one_capacity_, thread_count));
    one_.tasks_.reset(new OneStubT[one_.capacity_]);
    one_.datas_.reset(new TaskData<TaskOne>[one_.capacity_]);

    for (size_t i = 0; i < one_.capacity_; ++i) {
        one_.tasks_[i].data_ptr_ = &one_.datas_[i];
    }

    all_.capacity_ = std::bit_ceil(_config.all_capacity_ ? _config.all_capacity_ : 1);
    all_.tasks_.reset(new AllStubT[all_.capacity_]);
    all_.datas_.reset(new TaskData<TaskAll>[all_.capacity_]);

    solid_check(
        (std::numeric_limits<AtomicIndexValueT>::max() % one_.capacity_) == (one_.capacity_ - 1) && (std::numeric_limits<AtomicIndexValueT>::max() % all_.capacity_) == (all_.capacity_ - 1));

    for (size_t i = 0; i < all_.capacity_; ++i) {
        all_.tasks_[i].data_ptr_ = &all_.datas_[i];
    }
    all_.tasks_[0].produce_count_ = 1; //+
    all_.tasks_[0].consume_count_ = 0; // first entry is skipped on the first iteration

    spin_count_ = _config.spin_count_;

    for (size_t i = 0; i < thread_count; ++i) {
        threads_.emplace_back(
            std::thread{
                [this, i, start_fnc = std::move(_start_fnc), stop_fnc = std::move(_stop_fnc), one_fnc = std::move(_one_fnc), all_fnc = std::move(_all_fnc)](Args&&... _args) mutable {
                    start_fnc(i, std::forward<Args>(_args)...);
                    doRun(i, one_fnc, all_fnc, std::forward<Args>(_args)...);
                    stop_fnc(i, std::forward<Args>(_args)...);
                },
                std::forward<Args>(_args)...});
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
        const auto [index, count] = pushOneIndex();
        auto& rstub               = one_.tasks_[index];

        rstub.waitWhileStop(statistic_, count, spin_count_);
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
    LocalContext local_context;

    while (true) {
        const auto [index, count]        = popOneIndex();
        auto&    rstub                   = one_.tasks_[index];
        uint64_t local_one_context_count = 0;

        const auto event = rstub.waitWhilePop(
            statistic_,
            count,
            spin_count_,
            [this, &local_context](
                AtomicCounterT*           _pcounter,
                const AtomicCounterValueT _count,
                AllFnc&                   _all_fnc,
                Args&&... _args) {
                // we need to make sure that, after processing an all_task, no new one_task can have
                //  the all_id less than the all task that we have just processed.
                return tryConsumeAnAllTask(_pcounter, _count, local_context, _all_fnc, std::forward<Args>(_args)...);
            },
            _all_fnc,
            std::forward<Args>(_args)...);

        if (event == EventE::Fill) {
            auto  context_produce_id = rstub.context_produce_id_;
            auto* pctx               = rstub.pcontext_;
            auto  all_id             = rstub.all_id_;
            {
                TaskOne task{std::move(rstub.task())};

                rstub.destroy();
                rstub.clear();
                rstub.notifyWhilePop();

                if (pctx == nullptr) {
                    consumeAll(local_context, all_id, _all_fnc, std::forward<Args>(_args)...);

                    _one_fnc(task, std::forward<Args>(_args)...);
                    ++local_context.one_free_count_;
                    statistic_.runOneFreeCount(local_context.one_free_count_);
                    continue;
                } else if (context_produce_id == pctx->consume_id_.load(std::memory_order_relaxed)) {
                    consumeAll(local_context, all_id, _all_fnc, std::forward<Args>(_args)...);

                    _one_fnc(task, std::forward<Args>(_args)...);
                    ++local_one_context_count;
                } else {
                    pctx->spin_.lock();
                    if (context_produce_id != pctx->consume_id_.load(std::memory_order_relaxed)) {
                        pctx->push(std::move(task), all_id, context_produce_id);
                        pctx->spin_.unlock();
                        ++local_context.one_context_push_count_;
                        statistic_.runOneContextPush(local_context.one_context_push_count_);
                        continue;
                    } else {
                        pctx->spin_.unlock();

                        consumeAll(local_context, all_id, _all_fnc, std::forward<Args>(_args)...);

                        _one_fnc(task, std::forward<Args>(_args)...);
                        ++local_one_context_count;
                    }
                }
            }

            context_produce_id = pctx->consume_id_.fetch_add(1) + 1;

            do {
                TaskData<TaskOne> task_data;
                {
                    SpinGuardT lock{pctx->spin_};
                    if (pctx->pop(task_data, all_id, context_produce_id)) {
                    } else {
                        break;
                    }
                }

                consumeAll(local_context, all_id, _all_fnc, std::forward<Args>(_args)...);

                _one_fnc(task_data.task(), std::forward<Args>(_args)...);

                task_data.destroy();

                context_produce_id = pctx->consume_id_.fetch_add(1) + 1;
                pctx->release();
                ++local_one_context_count;
            } while (true);

            if (pctx->release()) {
                delete pctx;
            }

            local_context.one_context_count_ += local_one_context_count;

            statistic_.runOneContextCount(local_one_context_count, local_context.one_context_count_);
        } else if (event == EventE::Wake) {
            const auto all_id = rstub.all_id_;
            consumeAll(local_context, all_id, _all_fnc, std::forward<Args>(_args)...);

            ++local_context.wake_count_;
            statistic_.runWakeCount(local_context.wake_count_);
            rstub.notifyWhilePop();
        } else if (event == EventE::Stop) {
            rstub.notifyWhilePop();
            break;
        }
    }
}
//-----------------------------------------------------------------------------
template <class TaskOne, class TaskAll, class Stats>
template <
    class AllFnc,
    typename... Args>
bool ThreadPool<TaskOne, TaskAll, Stats>::tryConsumeAnAllTask(AtomicCounterT* _pcounter,
    const AtomicCounterValueT _count, LocalContext& _rlocal_context, AllFnc& _all_fnc, Args&&... _args)
{
    auto& rstub = all_.tasks_[_rlocal_context.next_all_id_ % all_.capacity_];
    if (rstub.isFilled(_rlocal_context.next_all_id_, all_.capacity_)) {
        // NOTE: first we fetch the commited_all_index then we check if the
        //  current stub is reserved (some thread is starting to push something)
        //  - this is to ensure that we are not processing an all task prior to being
        //  used by any one task. This is guranteed because when adding a new one task,
        //  before attaching the last commited_all_index to the current one task,
        //  we're atomicaly marking the one stub as Pushing.
        const auto commited_all_index = all_.commited_index_.load();

        if (_pcounter && _pcounter->load(/* std::memory_order_relaxed */) == _count) {
            // NOTE: this is to ensure that pushOnes and pushAlls from
            //  the same producer are processed in the same order they
            //  were produced.
            return true;
        }

        if (overflow_safe_less(commited_all_index, _rlocal_context.next_all_id_)) {
            cpu_pause();
            return true;
        }

        TaskAll task{rstub.task()};
        bool    should_retry = true;

        if (rstub.notifyWhilePop()) {
            should_retry = all_.pending_count_.fetch_sub(1) != 1;
        }

        _all_fnc(task, std::forward<Args>(_args)...);

        ++_rlocal_context.next_all_id_;
        ++_rlocal_context.all_count_;
        statistic_.tryConsumeAnAllTaskFilled(should_retry, _rlocal_context.all_count_);
        return should_retry;
    }
    const auto should_retry = all_.pending_count_.load() != 0;
    statistic_.tryConsumeAnAllTaskNotFilled(should_retry);
    return should_retry;
}
//-----------------------------------------------------------------------------
template <class TaskOne, class TaskAll, class Stats>
template <
    class AllFnc,
    typename... Args>
void ThreadPool<TaskOne, TaskAll, Stats>::consumeAll(LocalContext& _rlocal_context, const uint64_t _all_id, AllFnc& _all_fnc, Args&&... _args)
{
    size_t repeat_count = 0;
    while (overflow_safe_less(_rlocal_context.next_all_id_, _all_id) || _rlocal_context.next_all_id_ == _all_id) {
        tryConsumeAnAllTask(nullptr, 0, _rlocal_context, _all_fnc, std::forward<Args>(_args)...);
        ++repeat_count;
    }
    statistic_.consumeAll(repeat_count);
}
//-----------------------------------------------------------------------------
template <class TaskOne, class TaskAll, class Stats>
template <class Tsk>
void ThreadPool<TaskOne, TaskAll, Stats>::doPushOne(Tsk&& _task, ContextStub* _pctx)
{
    using namespace std::chrono;
    const auto start          = steady_clock::now();
    const auto [index, count] = pushOneIndex();
    auto& rstub               = one_.tasks_[index];

    rstub.waitWhilePushOne(statistic_, count, spin_count_);

    rstub.task(std::forward<Tsk>(_task));
    rstub.pcontext_ = _pctx;
    rstub.all_id_   = all_.commited_index_.load();

    if (_pctx) {
        _pctx->acquire();
        rstub.context_produce_id_ = _pctx->produce_id_.fetch_add(1);
    }
    uint64_t duration;
    rstub.notifyWhilePushOne(start, duration);
    // const uint64_t duration = duration_cast<microseconds>(steady_clock::now() - start).count();
    statistic_.pushOne(_pctx != nullptr, duration);
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
    const auto id    = pushAllId();
    auto&      rstub = all_.tasks_[id % all_.capacity_];

    rstub.waitWhilePushAll(statistic_, computeCounter(id, all_.capacity_), spin_count_);

    rstub.task(std::forward<Tsk>(_task));

    const auto should_wake_threads = all_.pending_count_.fetch_add(1) == 0;

    rstub.notifyWhilePushAll(static_cast<uint32_t>(threads_.size()), id);

    commitAllId(id);

    if (should_wake_threads) {
        for (size_t i = 0; i < threads_.size(); ++i) {
            const auto [index, count] = pushOneIndex(); // TODO:
            auto& rstub               = one_.tasks_[index];

            rstub.waitWhilePushAll(statistic_, count, spin_count_);

            rstub.all_id_ = id;

            rstub.notifyWhilePushAll();
        }
    }
    statistic_.pushAll(should_wake_threads);
}
//-----------------------------------------------------------------------------
template <class TaskOne, class TaskAll, class Stats>
typename ThreadPool<TaskOne, TaskAll, Stats>::ContextStub* ThreadPool<TaskOne, TaskAll, Stats>::doCreateContext()
{
    statistic_.createContext();
    return new ContextStub{};
}

//-----------------------------------------------------------------------------
} // namespace tpimpl

} // namespace solid
