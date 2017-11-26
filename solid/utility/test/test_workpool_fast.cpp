#include "solid/system/exception.hpp"
#include "solid/utility/workpool.hpp"
#include <atomic>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

using namespace solid;
using namespace std;

std::atomic<size_t> val{0};

int test_workpool_fast(int argc, char* argv[])
{
    FunctionWorkPoolT<size_t> wp{
        [](size_t _v) {
            val += _v;
        },
        5};
    const size_t cnt{5000000};

    wp.start(5);

    cout << "wp started" << endl;

    for (size_t i = 0; i < cnt; ++i) {
        wp.push(i);
    };

    wp.stop();
    cout << "val = " << val << endl;
    const size_t v = ((cnt - 1) * cnt) / 2;

    SOLID_CHECK(v == val);
    return 0;
}
