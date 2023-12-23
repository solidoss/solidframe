#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "solid/system/exception.hpp"
#include "solid/utility/common.hpp"
#include "solid/utility/sharedbuffer.hpp"
#include "solid/utility/threadpool.hpp"
using namespace std;
using namespace solid;
namespace {

#ifdef SOLID_ON_LINUX
vector<int> isolcpus = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15, 17, 18, 19};

void set_current_thread_affinity()
{
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

struct Message {
    atomic_uint32_t collapse_id_{0};
    atomic_uint32_t collapse_count_{0};
    string          text_;

    bool collapse(uint32_t _collapse_id)
    {
        ++collapse_count_;
        return collapse_id_.compare_exchange_strong(_collapse_id, _collapse_id + 1);
    }
};

template <class SP>
SP collapse(SP& _rsp, const ssize_t _expected_count = 1)
{
    typename SP::weak_type wp{_rsp};
    const auto             collapse_id = _rsp->collapse_id_.load();
    // here we have a race condition
    _rsp.reset();
    // this_thread::yield();
    if (wp.use_count() == _expected_count) {
        auto sp = wp.lock();
        if (sp && sp->collapse(collapse_id)) {
            return sp;
        }
    }
    return SP{};
}
using CallPoolT      = ThreadPool<Function<void()>, Function<void()>>;
using SharedMessageT = std::shared_ptr<Message>;
} // namespace

int test_collapse(int argc, char* argv[])
{
    char   choice       = 'B'; // B = basic, p = speed shared_ptr, b = speed SharedBuffer
    size_t repeat_count = 100;
    size_t thread_count = 10;

    if (argc > 1) {
        choice = *argv[1];
    }

    if (argc > 2) {
        repeat_count = stoul(argv[2]);
    }

    if (argc > 3) {
        thread_count = stoul(argv[3]);
    }

    if (choice == 'B') {
        for (size_t i = 0; i < repeat_count; ++i) {
            std::promise<void>           ready_promise;
            std::shared_future<void>     ready_future(ready_promise.get_future());
            std::promise<SharedMessageT> p;
            auto                         f  = p.get_future();
            auto                         sm = std::make_shared<Message>();
            vector<jthread>              thr_vec;
            {
                auto lambda = [&p, ready_future](SharedMessageT _sm) mutable {
                    set_current_thread_affinity();
                    // ready_future.wait();

                    if (auto tmp_sm = collapse(_sm)) {
                        p.set_value(std::move(tmp_sm));
                    }
                };

                auto sm_lock{sm};
                for (size_t i = 0; i < thread_count; ++i) {
                    thr_vec.emplace_back(lambda, sm);
                }

                ready_promise.set_value();

                if (auto tmp_sm = collapse(sm_lock)) {
                    p.set_value(std::move(tmp_sm));
                }
            }
            {
                auto lsm = f.get();
                cout << "done " << lsm.get() << ' ' << sm.get() << " collapse count = " << sm->collapse_count_.load() << endl;
                solid_check(lsm.get() == sm.get());
            }
        }
    } else if (choice == 'p') {
        CallPoolT  wp{thread_count, 10000, 100,
            [](const size_t) {
                set_current_thread_affinity();
            },
            [](const size_t) {}};
        auto       sm         = std::make_shared<Message>();
        const auto start_time = chrono::high_resolution_clock::now();
        for (size_t i = 0; i < repeat_count; ++i) {
            std::promise<SharedMessageT> p;
            auto                         f = p.get_future();
            auto                         sm_lock{sm};
            wp.pushAll(
                [&p, sm_lock]() mutable {
                    if (auto tmp_sm = collapse(sm_lock)) {
                        p.set_value(std::move(tmp_sm));
                    }
                });
            if (auto tmp_sm = collapse(sm_lock)) {
                p.set_value(std::move(tmp_sm));
            }
            {
                if (f.wait_for(chrono::seconds(1)) != future_status::ready) {
                    solid_throw("Waited for too long");
                }
                auto lsm = f.get();
            }
        }
        const auto stop_time = chrono::high_resolution_clock::now();
        cout << "Duration: " << chrono::duration_cast<chrono::microseconds>(stop_time - start_time).count() << "us" << endl;
    } else if (choice == 'b') {
        CallPoolT  wp{thread_count, 10000, 100,
            [](const size_t) {
                set_current_thread_affinity();
            },
            [](const size_t) {}};
        auto       sm         = make_shared_buffer(100);
        const auto start_time = chrono::high_resolution_clock::now();
        for (size_t i = 0; i < repeat_count; ++i) {
            std::promise<SharedBuffer> p;
            auto                       f = p.get_future();
            auto                       sm_lock{std::move(sm)};
            wp.pushAll(
                [&p, sm_lock]() mutable {
                    if (sm_lock.collapse()) {
                        p.set_value(std::move(sm_lock));
                    }
                });
            if (sm_lock.collapse()) {
                p.set_value(std::move(sm_lock));
            }
            {
                if (f.wait_for(chrono::seconds(1)) != future_status::ready) {
                    solid_throw("Waited for too long");
                }
                sm = f.get();
            }
        }
        const auto stop_time = chrono::high_resolution_clock::now();
        cout << "Duration: " << chrono::duration_cast<chrono::microseconds>(stop_time - start_time).count() << "us" << endl;
    } else {
        cout << "Invalid choice. Expecter B or p or b. Got [" << choice << "]." << endl;
    }
    return 0;
}
