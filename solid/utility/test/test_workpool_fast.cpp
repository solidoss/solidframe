#include "solid/system/exception.hpp"
#include "solid/utility/workpool.hpp"
#include <atomic>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

using namespace solid;
using namespace std;

namespace {
std::atomic<size_t> val{0};
}

int test_workpool_fast(int /*argc*/, char* /*argv*/ [])
{
    solid::log_start(std::cerr, {".*:VIEW"});

    const size_t cnt{70};
    {
        WorkPool<size_t> wp{
            1 /*thread::hardware_concurrency()*/,
            WorkPoolConfiguration(1),
            [](size_t _v, const std::string& _rs) {
                //SOLID_CHECK(_rs == "this is a string", "failed string check");
                val += _v;
            },
            "this is a string"};

        for (size_t i = 0; i < cnt; ++i) {
            wp.push(i);
        };
    }

    solid_log(generic_logger, Verbose, "val = " << val);

    const size_t v = ((cnt - 1) * cnt) / 2;

    SOLID_CHECK(v == val);
    return 0;
}
