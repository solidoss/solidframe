#include "solid/system/exception.hpp"
#include "solid/utility/workpool.hpp"
#include <algorithm>
#include <atomic>
#include <deque>
#include <functional>
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

int test_workpool_fast(int argc, char* argv[])
{
    solid::log_start(std::cerr, {".*:VIEW"});

    deque<size_t> gdq;
    std::mutex    gmtx;

    size_t cnt{5000000};

    if (argc > 1) {
        cnt = atoi(argv[1]);
    }

    {
        WorkPool<size_t> wp{
            2 /*thread::hardware_concurrency()*/,
            WorkPoolConfiguration(),
            [](size_t _v, Context& _rctx) {
                //SOLID_CHECK(_rs == "this is a string", "failed string check");
                val += _v;
#ifdef EXTRA_CHECKING
                _rctx.ldq_.emplace_back(_v);
#endif
            },
            std::ref(gdq), std::ref(gmtx)};
        //this_thread::sleep_for(chrono::seconds(1));
        for (size_t i = 0; i < cnt; ++i) {
            wp.push(i);
        };
    }

    const size_t v = ((cnt - 1) * cnt) / 2;

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
