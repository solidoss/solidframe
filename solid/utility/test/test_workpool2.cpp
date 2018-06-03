#include "solid/system/exception.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/workpool2.hpp"
#include <atomic>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

using namespace solid;
using namespace std;

using thread_t      = std::thread;
using mutex_t       = std::mutex;
using unique_lock_t = std::unique_lock<mutex_t>;

namespace {

mutex_t mtx;
//using FunctionJobT = std::function<void()>;
using FunctionJobT = solid::Function<32, void()>;
} // namespace

int test_workpool(int /*argc*/, char* /*argv*/ [])
{
    solid::log_start(std::cerr, {".*:VIEW"});

    auto l = [](FunctionJobT& _jf) {
        _jf();
    };

    FunctionWorkPoolT<FunctionJobT> wp{l, thread::hardware_concurrency()};
    std::atomic<size_t>             val{0};
    const size_t                    cnt{5000000};

    wp.start(thread::hardware_concurrency());

    solid_log(generic_logger, Verbose, "wp started");

    for (size_t i = 0; i < cnt; ++i) {
        auto l = [i, &val]() {
            if (0) {
                unique_lock_t lock(mtx);
                cout << this_thread::get_id() << ' ' << i << endl;
            }
            //this_thread::sleep_for(std::chrono::seconds(2));
            val += i;
        };
        wp.push(l);
    };

    wp.stop();

    solid_log(generic_logger, Verbose, "val = " << val);

    const size_t v = ((cnt - 1) * cnt) / 2;

    SOLID_CHECK(v == val);
    return 0;
}
