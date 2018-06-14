#include "solid/system/exception.hpp"
#include "solid/utility/workpool.hpp"
#include <algorithm>
#include <atomic>
#include <deque>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

using namespace solid;
using namespace std;

namespace {
std::atomic<size_t> val{0};
}

struct Context {
    deque<size_t>& rgdq_;
    mutex&         rgmtx_;
    deque<size_t>  ldq_;

    Context(deque<size_t>& _rgdq, mutex& _rgmtx)
        : rgdq_(_rgdq)
        , rgmtx_(_rgmtx)
    {
    }

    ~Context()
    {
        lock_guard<mutex> lock(rgmtx_);
        rgdq_.insert(rgdq_.end(), ldq_.begin(), ldq_.end());
    }
};

int test_workpool(int argc, char* argv[])
{
    solid::log_start(std::cerr, {".*:VIEWS"});

    cout << "usage: " << argv[0] << " JOB_COUNT WAIT_SECONDS QUEUE_SIZE PRODUCER_COUNT CONSUMER_COUNT PUSH_SLEEP_MSECS JOB_SLEEP_MSECS" << endl;

    size_t        job_count        = 5000000;
    int           wait_seconds     = 10;
    int           queue_size       = -1;
    int           producer_count   = 0;
    int           consumer_count   = thread::hardware_concurrency();
    int           push_sleep_msecs = 0;
    int           job_sleep_msecs  = 0;
    deque<size_t> gdq;
    std::mutex    gmtx;
    promise<void> prom;

    if (argc > 1) {
        job_count = atoi(argv[1]);
    }

    if (argc > 2) {
        wait_seconds = atoi(argv[2]);
    }

    if (argc > 3) {
        queue_size = atoi(argv[3]);
    }

    if (argc > 4) {
        producer_count = atoi(argv[4]);
    }

    if (argc > 5) {
        consumer_count = atoi(argv[5]);
    }

    if (argc > 6) {
        push_sleep_msecs = atoi(argv[6]);
    }

    if (argc > 7) {
        job_sleep_msecs = atoi(argv[7]);
    }

    thread wait_thread(
        [](promise<void>& _rprom, const int _wait_time_seconds) {
            SOLID_CHECK(_rprom.get_future().wait_for(chrono::seconds(_wait_time_seconds)) == future_status::ready, " Test is taking too long - waited " << _wait_time_seconds << " secs");
        },
        std::ref(prom), wait_seconds);

    {
        WorkPool<size_t> wp{
            1,
            WorkPoolConfiguration(consumer_count, queue_size <= 0 ? std::numeric_limits<size_t>::max() : queue_size),
            [job_sleep_msecs](size_t _v, Context& _rctx) {
                //SOLID_CHECK(_rs == "this is a string", "failed string check");
                val += _v;
                if (job_sleep_msecs) {
                    this_thread::sleep_for(chrono::milliseconds(job_sleep_msecs));
                }
#ifdef EXTRA_CHECKING
                _rctx.ldq_.emplace_back(_v);
#endif
            },
            std::ref(gdq), std::ref(gmtx)};

        auto producer_lambda = [job_count, push_sleep_msecs, &wp]() {
            for (size_t i = 0; i < job_count; ++i) {
                if (push_sleep_msecs) {
                    this_thread::sleep_for(chrono::milliseconds(push_sleep_msecs));
                }
                wp.push(i);
            };
        };

        if (producer_count) {
            vector<thread> thr_vec;
            for (int i = 0; i < producer_count; ++i) {
                thr_vec.emplace_back(producer_lambda);
            }
            for (auto& t : thr_vec) {
                t.join();
            }
        } else {
            producer_lambda();
        }
    }

    prom.set_value();
    wait_thread.join();

    const size_t v = (((job_count - 1) * job_count) / 2) * (producer_count == 0 ? 1 : producer_count);

    solid_log(generic_logger, Verbose, "val = " << val << " expected val = " << v);

#ifdef EXTRA_CHECKING
    sort(gdq.begin(), gdq.end());

    const size_t  dv = -1;
    const size_t* pv = &dv;

    for (const auto& cv : gdq) {
        if (cv == *pv) {
            cout << cv << ' ';
        }
        pv = &cv;
    }
    cout << endl;
#endif
    SOLID_CHECK(v == val);
    return 0;
}
