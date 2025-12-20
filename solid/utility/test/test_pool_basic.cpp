#include "solid/utility/intrusiveptr.hpp"
#include "solid/utility/pool.hpp"
#include "solid/utility/poolable.hpp"
#include "solid/utility/threadpool.hpp"

#include "solid/system/cassert.hpp"
#include "solid/system/exception.hpp"

#include <deque>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_set>
#include <variant>
using namespace std;
using namespace solid;

namespace {

struct First final : IntrusiveThreadSafeBase, Poolable<First> {
    static atomic_uint32_t create_count;
    uint64_t               v_ = 0;

    First(uint64_t _v)
        : v_(_v)
    {
        ++create_count;
    }

    ~First()
    {
        --create_count;
    }

    void init(uint64_t _v)
    {
        v_ = _v;
    }
};

struct Second final : IntrusiveThreadSafeBase, Poolable<Second> {
    static atomic_uint32_t create_count;
    string                 v_;

    Second(string_view _v)
        : v_(_v)
    {
        ++create_count;
    }

    ~Second()
    {
        --create_count;
    }

    void init(string_view _v)
    {
        v_ = _v;
    }
};

atomic_uint32_t First::create_count{0};
atomic_uint32_t Second::create_count{0};

template <typename Msg>
struct Task {
    size_t                 id_;
    ConstIntrusivePtr<Msg> ptr_;
};

void test_local_pool()
{
    Pool<First>::get({.push_capacity_ = 100, .poll_capacity_ = 0});
    deque<MutableIntrusivePtr<First>> dq;

    for (size_t i = 0; i < 100; ++i) {
        dq.emplace_back(Pool<First>::create(i));
    }

    std::unordered_set<void const*> uset;

    for (auto const& ptr : dq) {
        solid_check(ptr);
        uset.insert(ptr.get());
    }
    solid_check(uset.size() == dq.size());

    dq.clear();

    for (size_t i = 0; i < 100; ++i) {
        dq.emplace_back(Pool<First>::create(i));
    }

    std::unordered_set<void const*> uset2;

    for (auto const& ptr : dq) {
        solid_check(ptr);
        uset2.insert(ptr.get());
    }
    solid_check(uset2 == uset);
}

thread_local deque<std::variant<ConstIntrusivePtr<First>, ConstIntrusivePtr<Second>>> tldq;

void test_thread_pool()
{
    constexpr size_t object_count = 100;
    constexpr size_t thread_count = 1;

    using VariantT    = std::variant<Task<First>, Task<Second>>;
    using ThreadPoolT = ThreadPool<VariantT, size_t, solid::EmptyThreadPoolStatistic>;

    mutex                           m;
    std::unordered_set<void const*> uset;
    promise<void>                   prom;
    atomic<size_t>                  all_wait{thread_count};

    ThreadPoolT tp{
        {thread_count, 100 * 1024, 1024, 1000ULL},
        [](size_t) {
        },
        [](size_t) {
        },
        [&](VariantT& _var) {
            std::visit([&](auto& _task) {
                solid_check(_task.ptr_);
                {
                    lock_guard<mutex> lock{m};
                    uset.insert(_task.ptr_.get());
                }
                tldq.push_back(std::move(_task.ptr_));
                solid_check(not _task.ptr_);
                if (_task.id_ == (object_count - 1)) {
                    prom.set_value();
                }
            },
                _var);
        },
        [&](size_t) {
            cout << "tldq.size = " << tldq.size() << endl;
            tldq.clear();
            --all_wait;
            all_wait.notify_one();
        }};

    for (size_t i = 0; i < object_count; ++i) {
        if (i & 1) {
            tp.pushOne(Task<First>{.id_ = i, .ptr_ = Pool<First>::create(i)});
        } else {
            tp.pushOne(Task<Second>{.id_ = i, .ptr_ = Pool<Second>::create(to_string(i))});
        }
    }
    auto fut = prom.get_future();
    fut.wait();
    fut.get();
    tp.pushAll(0);
    auto old_wait = all_wait.load();
    while (old_wait) {
        all_wait.wait(old_wait);
        old_wait = all_wait.load();
    }
    {
        deque<MutableIntrusivePtr<First>>  first_dq;
        deque<MutableIntrusivePtr<Second>> second_dq;
        std::unordered_set<void const*>    uset2;

        for (size_t i = 0; i < object_count; ++i) {
            if (i & 1) {
                first_dq.emplace_back(Pool<First>::create(i));
                solid_check(first_dq.back());
                uset2.insert(first_dq.back().get());
            } else {
                second_dq.emplace_back(Pool<Second>::create(to_string(i)));
                solid_check(second_dq.back());
                uset2.insert(second_dq.back().get());
            }
        }

        cout << "uset.size = " << uset.size() << " uset2.size = " << uset2.size() << endl;

        solid_check(uset2 == uset);
    }
}

} // namespace

int test_pool_basic(int /*argc*/, char* /*argv*/[])
{
    {
        jthread thr{test_local_pool};
    }
    {
        jthread thr{test_thread_pool};
    }

    solid_check(First::create_count == 0);
    solid_check(Second::create_count == 0);
    return 0;
}