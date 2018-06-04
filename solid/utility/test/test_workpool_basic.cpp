#include "solid/system/exception.hpp"
#include "solid/utility/workpool.hpp"
#include <atomic>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

using namespace solid;
using namespace std;

int test_workpool_basic(int /*argc*/, char* /*argv*/ [])
{
    solid::log_start(std::cerr, {".*:VIEW"});

    WorkPool<size_t>    wp;
    const size_t        cnt{5000000};
    std::atomic<size_t> val{0};

    wp.start(
        0,
        WorkPoolConfiguration(),
        [&val](size_t _v) {
            val += _v;
        });

    for (size_t i = 0; i < cnt; ++i) {
        wp.push(i);
    };

    wp.stop();

    solid_log(generic_logger, Verbose, "val = " << val);

    const size_t v = ((cnt - 1) * cnt) / 2;

    SOLID_CHECK(v == val);
    return 0;
}
