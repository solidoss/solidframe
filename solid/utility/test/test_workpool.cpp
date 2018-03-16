#include "solid/system/exception.hpp"
#include "solid/utility/workpool.hpp"
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
using FunctionJobT = std::function<void()>;
} // namespace

int test_workpool(int /*argc*/, char* /*argv*/ [])
{
    auto l = [](FunctionJobT& _jf) {
        _jf();
    };

    FunctionWorkPoolT<FunctionJobT> wp{l, 5};
    std::atomic<size_t>             val{0};
    const size_t                    cnt{5000000};

    wp.start(5);

    cout << "wp started" << endl;

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
    cout << "val = " << val << endl;
    const size_t v = ((cnt - 1) * cnt) / 2;

    SOLID_CHECK(v == val);
    return 0;
}
