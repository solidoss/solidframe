#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include "solid/system/statistic.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/threadpool.hpp"
#include <atomic>
#include <future>
#include <iostream>
#include <semaphore>

using namespace solid;
using namespace std;

namespace {
const LoggerT logger("test");
struct Context {
    atomic<uint64_t> min_{InvalidSize{}};
    atomic<uint64_t> max_{0};
    atomic<uint64_t> sum_{0};
    atomic<uint64_t> count_{0};

    ostream& print(ostream& _ros) const
    {
        const auto avg = count_ ? sum_ / count_ : 0;
        _ros << "min " << min_ << " max " << max_ << " avg " << avg;
        return _ros;
    }
};

ostream& operator<<(ostream& _ros, const Context& _rctx)
{
    return _rctx.print(_ros);
}

constexpr size_t one_task_size = 64;

using CallPoolT = ThreadPool<Function<void(Context&), one_task_size>, Function<void(Context&)>>;
struct Entry {
    CallPoolT::SynchronizationContextT ctx_;
};

constexpr size_t thread_count = 10;

#ifdef SOLID_ON_LINUX
vector<int> isolcpus = {/*3, 4, 5, 6,*/ 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};

void set_current_thread_affinity()
{
    if (std::thread::hardware_concurrency() < (thread_count + isolcpus[0])) {
        return;
    }
    static std::atomic<int> crtCore(0);

    const int isolCore = isolcpus[crtCore.fetch_add(1) % isolcpus.size()];
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(isolCore, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    // solid_check(rc == 0);
    (void)rc;
}
#else
void set_current_thread_affinity()
{
}
#endif

template <class Dur>
void busy_for(const Dur& _dur)
{
    const auto stop = chrono::steady_clock::now() + _dur;
    while (chrono::steady_clock::now() < stop)
        ;
}
using AtomicCounterT      = std::atomic<uint8_t>;
using AtomicCounterValueT = AtomicCounterT::value_type;
atomic_size_t push_one_index{0};
size_t        capacity = 8 * 1024;

template <class IndexT>
inline constexpr static auto computeCounter(const IndexT _index, const size_t _capacity) noexcept
{
    return (_index / _capacity) & std::numeric_limits<AtomicCounterValueT>::max();
}
std::tuple<size_t, AtomicCounterValueT> pushOneIndex() noexcept
{
    const auto index = push_one_index.fetch_add(1);
    return {index % capacity, computeCounter(index, capacity)};
}

} // namespace

int test_threadpool_batch(int argc, char* argv[])
{
    solid::log_start(std::cerr, {".*:EWXS", "test:VIEWS"});
    int    wait_seconds = 500;
    size_t entry_count  = 300;
    size_t repeat_count = 1000000;
    solid_log(logger, Verbose, "capacity " << capacity << " reminder " << (std::numeric_limits<atomic_size_t::value_type>::max() % capacity));
    {
        vector<AtomicCounterValueT> cnt_vec(capacity, 0);

        for (size_t i = 0; i < (capacity * std::numeric_limits<AtomicCounterValueT>::max() + capacity); ++i) {
            const auto [index, counter] = pushOneIndex();
            solid_check(cnt_vec[index] == counter, "" << (int)cnt_vec[index] << " == " << (int)counter << " index = " << index << " i = " << i);
            ++cnt_vec[index];
        }

        push_one_index = std::numeric_limits<atomic_size_t::value_type>::max() - (10 * capacity) + 1;

        for (size_t i = 0; i < capacity; ++i) {
            const auto [index, counter] = pushOneIndex();
            cnt_vec[index]              = counter;
            ++cnt_vec[index];
        }

        for (size_t i = 0; i < (capacity * std::numeric_limits<AtomicCounterValueT>::max() + capacity); ++i) {
            const auto [index, counter] = pushOneIndex();
            solid_check(cnt_vec[index] == counter, "" << (int)cnt_vec[index] << " == " << (int)counter << " index = " << index << " i = " << i << " one_index = " << push_one_index);
            ++cnt_vec[index];
        }
    }

#if 1
    auto lambda = [&]() {
        Context ctx;
        {
            solid_log(logger, Verbose, "start");
            CallPoolT wp{
                {thread_count, 12000, 100}, [](const size_t, Context&) {}, [](const size_t, Context& _rctx) {},
                std::ref(ctx)};
            solid_log(logger, Verbose, "TP capacity: one " << wp.capacityOne() << " all " << wp.capacityAll());
            solid_log(logger, Verbose, "create contexts");
            vector<Entry> entries;
            for (size_t i = 0; i < entry_count; ++i) {
                entries.emplace_back(wp.createSynchronizationContext());
            }

            solid_log(logger, Verbose, "wp started");
            uint64_t tmin{InvalidSize{}};
            uint64_t tmax{0};
            uint64_t tsum{0};
            uint64_t tcnt{0};
            for (size_t i = 0; i < 40; ++i) {
                auto start = chrono::steady_clock::now();

                for (size_t j = 0; j < entries.size(); ++j) {
                    auto& entry{entries[j]};
                    auto  lambda = [&entry, start, j](Context& _rctx) mutable {
                        const auto     now      = chrono::steady_clock::now();
                        const uint64_t duration = chrono::duration_cast<chrono::microseconds>(now - start).count();
                        store_min(_rctx.min_, duration);
                        store_max(_rctx.max_, duration);
                        _rctx.sum_ += duration;
                        ++_rctx.count_;
                        // this_thread::sleep_for(chrono::microseconds(1));
                    };
                    static_assert(sizeof(lambda) <= one_task_size);
                    entry.ctx_.push(std::move(lambda));
                    // wp.pushOne(std::move(lambda));
                }
                {
                    const uint64_t duration = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - start).count();
                    store_min(tmin, duration);
                    store_max(tmax, duration);
                    tsum += duration;
                    ++tcnt;
                }
                // this_thread::sleep_for(chrono::milliseconds(100));
            }
            solid_log(logger, Verbose, "min " << tmin << " max " << tmax << " avg " << tsum / tcnt << " " << ctx);
            solid_log(logger, Statistic, "ThreadPool statistic: " << wp.statistic());
        }
        solid_log(logger, Verbose, "after loop");
    };
#elif 0
    // clang-format off
    vector<size_t> busy_cons_vec{
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
        100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
        100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
        10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000,
        10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000,
        2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000,
        30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000,
        30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000,
        2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000,
        2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000,
        100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
        30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000,
        30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000,
        30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000,
        100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
        100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
        30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000,
        2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000,
        30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000,
        30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000, 30000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000,
    };
    vector<size_t> busy_prod_vec{
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
    };
    // clang-format on
    auto lambda = [&]() {
        Context ctx;
        {
            solid_log(logger, Verbose, "start");
            CallPoolT wp{
                thread_count, 12000, 100, [](const size_t, Context&) {}, [](const size_t, Context& _rctx) {},
                std::ref(ctx)};
            solid_log(logger, Verbose, "create contexts");
            vector<Entry> entries;
            for (size_t i = 0; i < entry_count; ++i) {
                entries.emplace_back(wp.createSynchronizationContext());
            }

            solid_log(logger, Verbose, "wp started");
            uint64_t tmin{InvalidSize{}};
            uint64_t tmax{0};
            uint64_t tsum{0};
            uint64_t tcnt{0};
            uint64_t prod_dur_ns = 0;
            uint64_t cons_dur_ns = 0;
            for (size_t i = 0; i < repeat_count; ++i) {
                const auto prod_ns = busy_prod_vec[i % busy_prod_vec.size()];
                busy_for(std::chrono::nanoseconds(prod_ns));
                prod_dur_ns += prod_ns;

                auto       start = chrono::steady_clock::now();
                auto&      entry{entries[i % entries.size()]};
                const auto busy_ns = std::chrono::nanoseconds(busy_cons_vec[i % busy_cons_vec.size()]);
                cons_dur_ns += busy_ns.count();
                auto lambda = [&entry, start, busy_ns](Context& _rctx) mutable {
                    const auto     now      = chrono::steady_clock::now();
                    const uint64_t duration = chrono::duration_cast<chrono::microseconds>(now - start).count();
                    store_min(_rctx.min_, duration);
                    store_max(_rctx.max_, duration);
                    _rctx.sum_ += duration;
                    ++_rctx.count_;
                    // this_thread::sleep_for(chrono::microseconds(1));
                    busy_for(busy_ns);
                };
                entry.ctx_.push(std::move(lambda));
                {
                    const uint64_t duration = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - start).count();
                    store_min(tmin, duration);
                    store_max(tmax, duration);
                    tsum += duration;
                    ++tcnt;
                }
                // this_thread::sleep_for(chrono::milliseconds(100));
            }

            // this_thread::sleep_for(chrono::seconds(100));
            chrono::microseconds prod_dur = chrono::duration_cast<chrono::microseconds>(chrono::nanoseconds(prod_dur_ns));
            chrono::microseconds cons_dur = chrono::duration_cast<chrono::microseconds>(chrono::nanoseconds(cons_dur_ns));
            solid_log(logger, Verbose, "min " << tmin << " max " << tmax << " avg " << tsum / tcnt << " " << ctx << " prod_dur(us): " << prod_dur.count() << " cons_dur(us): " << cons_dur.count());
            solid_log(logger, Statistic, "ThreadPool statistic: " << wp.statistic());
        }
        solid_log(logger, Verbose, "after loop");
    };
#elif 0
    using LockT                                 = atomic<int>;
    static constexpr LockT::value_type stopping = InvalidIndex{};
    static constexpr LockT::value_type popping  = 1;
    static constexpr LockT::value_type pushing  = 2;
    static constexpr LockT::value_type empty    = 3;
    static constexpr LockT::value_type filled   = 4;
    struct /* alignas(std::hardware_destructive_interference_size) */ ThreadContext {
        thread        thr_;
        atomic_size_t lock_  = {empty};
        size_t        value_ = 0;

        void push()
        {
            size_t value;
            while (!lock_.compare_exchange_weak(value = empty, pushing)) {
                std::atomic_wait(&lock_, value);
            }

            ++value_;

            lock_.store(filled);
            std::atomic_notify_one(&lock_);
        }

        void stop()
        {
            size_t value;
            while (!lock_.compare_exchange_weak(value = empty, pushing)) {
                std::atomic_wait(&lock_, value);
            }

            value_ = stopping;

            lock_.store(filled);
            std::atomic_notify_one(&lock_);
        }

        bool pop(size_t& _expected_value)
        {
            size_t value;
            while (!lock_.compare_exchange_weak(value = filled, popping)) {
                std::atomic_wait(&lock_, value);
            }

            if (value_ == stopping)
                return false;

            solid_check(value_ == _expected_value);
            ++_expected_value;

            lock_.store(empty);
            std::atomic_notify_one(&lock_);

            return true;
        }
    };
    auto lambda = [&]() {
        set_current_thread_affinity();
        unique_ptr<ThreadContext[]> ctxs{new ThreadContext[thread_count]};
        for (size_t i = 0; i < thread_count; ++i) {
            auto& ctx = ctxs[i];
            ctx.thr_  = thread(
                [](ThreadContext& _rctx) {
                    set_current_thread_affinity();
                    size_t expected_val = 1;
                    while (_rctx.pop(expected_val))
                        ;
                },
                ref(ctx));
        }
        uint64_t tmin{InvalidSize{}};
        uint64_t tmax{0};
        uint64_t tsum{0};
        uint64_t tcnt{0};
        for (size_t i = 0; i < 40; ++i) {
            const auto start = chrono::steady_clock::now();
            for (size_t j = 0; j < entry_count; ++j) {
                auto& rctx = ctxs[j % thread_count];
                rctx.push();
            }
            {
                const uint64_t duration = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - start).count();
                store_min(tmin, duration);
                store_max(tmax, duration);
                tsum += duration;
                ++tcnt;
            }
        }
        for (size_t i = 0; i < thread_count; ++i) {
            auto& ctx = ctxs[i];
            ctx.stop();
            ctx.thr_.join();
        }
        solid_log(logger, Verbose, "min " << tmin << " max " << tmax << " avg " << tsum / tcnt);
    };
#else
    static constexpr size_t stopping = InvalidIndex{};
    static constexpr size_t popping  = 1;
    static constexpr size_t pushing  = 2;
    static constexpr size_t empty    = 3;
    static constexpr size_t filled   = 4;
    struct alignas(std::hardware_destructive_interference_size) ThreadContext {
        thread           thr_;
        binary_semaphore push_sem_{1};
        binary_semaphore pop_sem_{0};

        size_t value_ = 0;

        void push()
        {
            push_sem_.acquire();

            ++value_;

            pop_sem_.release();
        }

        void stop()
        {
            push_sem_.acquire();
            value_ = stopping;
            pop_sem_.release();
        }

        bool pop(size_t& _expected_value)
        {
            pop_sem_.acquire();
            if (value_ == stopping)
                return false;

            solid_check(value_ == _expected_value);
            ++_expected_value;
            push_sem_.release();
            return true;
        }
    };
    auto lambda = [&]() {
        set_current_thread_affinity();
        unique_ptr<ThreadContext[]> ctxs{new ThreadContext[thread_count]};
        for (size_t i = 0; i < thread_count; ++i) {
            auto& ctx = ctxs[i];
            ctx.thr_  = thread(
                [](ThreadContext& _rctx) {
                    set_current_thread_affinity();
                    size_t expected_val = 1;
                    while (_rctx.pop(expected_val))
                        ;
                },
                ref(ctx));
        }
        uint64_t tmin{InvalidSize{}};
        uint64_t tmax{0};
        uint64_t tsum{0};
        uint64_t tcnt{0};
        for (size_t i = 0; i < 40; ++i) {
            const auto start = chrono::steady_clock::now();
            for (size_t j = 0; j < entry_count; ++j) {
                auto& rctx = ctxs[j % thread_count];
                rctx.push();
            }
            {
                const uint64_t duration = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - start).count();
                store_min(tmin, duration);
                store_max(tmax, duration);
                tsum += duration;
                ++tcnt;
            }
        }
        for (size_t i = 0; i < thread_count; ++i) {
            auto& ctx = ctxs[i];
            ctx.stop();
            ctx.thr_.join();
        }
        solid_log(logger, Verbose, "min " << tmin << " max " << tmax << " avg " << tsum / tcnt);
    };
#endif
    auto fut = async(launch::async, lambda);
    if (fut.wait_for(chrono::seconds(wait_seconds)) != future_status::ready) {
        solid_throw(" Test is taking too long - waited " << wait_seconds << " secs");
    }
    fut.get();
    solid_log(logger, Verbose, "after async wait");

    return 0;
}
