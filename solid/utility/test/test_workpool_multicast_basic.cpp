#include "solid/system/crashhandler.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/workpool.hpp"
#include <atomic>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

using namespace solid;
using namespace std;
namespace {
const LoggerT logger("test");

thread_local size_t thread_local_value = 0;

}

int test_workpool_multicast_basic(int argc, char* argv[])
{
    install_crash_handler();
    solid::log_start(std::cerr, {".*:EWXS", "test:VIEWS"});
    using WorkPoolT  = WorkPoolMulticast<size_t, uint32_t>;
    using AtomicPWPT = std::atomic<WorkPoolT*>;

    solid_log(logger, Statistic, "thread concurrency: " << thread::hardware_concurrency());
#ifdef SOLID_SANITIZE_THREAD
    const int wait_seconds = 1500;
#else
    const int wait_seconds = 150;
#endif
    int                 loop_cnt = 5;
    const size_t        cnt{5000000};
    const size_t        v = (((cnt - 1) * cnt)) / 2;
    std::atomic<size_t> all_val{0};
    std::atomic<size_t> val{0};
    AtomicPWPT          pwp{nullptr};

    if (argc > 1) {
        loop_cnt = atoi(argv[1]);
    }
    auto lambda = [&]() {
        for (int i = 0; i < loop_cnt; ++i) {
            {
                WorkPoolT wp{
                    WorkPoolConfiguration{2},
                    [&val, &all_val](const size_t _v) {
                        solid_check(thread_local_value == all_val.load());
                        val += _v;
                    },
                    [](const uint32_t _v){//synch handle
                        thread_local_value = _v;
                    },
                    [&all_val](const uint32_t _v){//synch update
                        all_val = _v;
                    }
                };
                
                pwp = &wp;
                std::promise<void> barrier;
                std::future<void> barrier_future = barrier.get_future();
                
                std::thread all_thr{
                    [&wp, cnt](std::promise<void> barrier){
                        barrier.set_value();
                        for(uint32_t i = 0; i < cnt; ++i){
                            if((i % 10) == 0){
                                wp.pushAllSync(i/10);
                            }
                        }
                    }, std::move(barrier)
                };
                
                barrier_future.wait();
                
                for (size_t i = 0; i < cnt; ++i) {
                    wp.push(i);
                };
                
                pwp = nullptr;
                
                all_thr.join();
            }
            solid_log(logger, Verbose, "after loop");
            solid_check(v == val, "val = " << val << " v = " << v);
            val = 0;
        }
    };
    
    
    auto fut = async(launch::async, lambda);
    if (fut.wait_for(chrono::seconds(wait_seconds)) != future_status::ready) {
        if (pwp != nullptr) {
            pwp.load()->dumpStatistics();
        }
        solid_throw(" Test is taking too long - waited " << wait_seconds << " secs");
    }
    fut.get();
    
    solid_log(logger, Verbose, "after async wait");

    return 0;
}
