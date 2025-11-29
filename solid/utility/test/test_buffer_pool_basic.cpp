
#include "solid/utility/sharedbuffer.hpp"
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

struct Task {
    size_t            id_;
    ConstSharedBuffer buf_;
};

void test_local_pool()
{
    deque<MutableSharedBuffer> dq;

    for (size_t i = 0; i < 100; ++i) {
        dq.emplace_back(BufferPool<>::create(4000));
    }

    std::unordered_set<void const*> uset;

    for (auto const& ptr : dq) {
        solid_check(ptr);
        uset.insert(ptr.mdata());
    }
    solid_check(uset.size() == dq.size());

    dq.clear();

    std::unordered_set<void const*> uset2;

    for (size_t i = 0; i < 100; ++i) {
        dq.emplace_back(BufferPool<>::create(4000));
        solid_check(uset.count(dq.back().mdata()) == 1);
        solid_check(uset2.count(dq.back().mdata()) == 0);
        uset2.insert(dq.back().mdata());
    }

    solid_check(uset2 == uset);
}

thread_local deque<ConstSharedBuffer> tldq;

void test_thread_pool()
{
    constexpr size_t object_count = 100;
    constexpr size_t thread_count = 1;

    using VariantT    = std::variant<Task>;
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
                solid_check(_task.buf_);
                {
                    lock_guard<mutex> lock{m};
                    uset.insert(_task.buf_.data());
                }
                tldq.push_back(std::move(_task.buf_));
                solid_check(not _task.buf_);
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
            tp.pushOne(Task{.id_ = i, .buf_ = BufferPool<>::create(4000)});
        } else {
            tp.pushOne(Task{.id_ = i, .buf_ = BufferPool<>::create(8000)});
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
        deque<ConstSharedBuffer>        dq;
        std::unordered_set<void const*> uset2;

        for (size_t i = 0; i < object_count; ++i) {
            if (i & 1) {
                dq.emplace_back(BufferPool<>::create(4000));
                solid_check(dq.back());
                uset2.insert(dq.back().data());
            } else {
                dq.emplace_back(BufferPool<>::create(8000));
                solid_check(dq.back());
                uset2.insert(dq.back().data());
            }
        }

        cout << "uset.size = " << uset.size() << " uset2.size = " << uset2.size() << endl;

        solid_check(uset2 == uset);
    }
}

} // namespace

int test_buffer_pool_basic(int /*argc*/, char* /*argv*/[])
{
    {
        jthread thr{test_local_pool};
    }
    {
        jthread thr{test_thread_pool};
    }
    return 0;
}