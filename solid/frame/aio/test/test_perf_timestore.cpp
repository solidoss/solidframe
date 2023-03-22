#include "solid/frame/timestore.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace std;
using namespace solid;

int test_perf_timestore(int argc, char* argv[])
{
    size_t repeat_count = 100;
    size_t timer_count  = 10000;

    frame::TimeStore<size_t> time_store;
    chrono::nanoseconds      max_duration(0);
    chrono::nanoseconds      total_duration(0);
    size_t                   count = 0;

    for (size_t r = 0; r < repeat_count; ++r) {
        for (size_t i = 0; i < timer_count; ++i) {
            time_store.push(NanoTime::createSteady(), i);
            this_thread::sleep_for(chrono::nanoseconds(5));
        }
        size_t first_call_count  = 0;
        size_t second_call_count = 0;
        max_duration             = chrono::nanoseconds(0);
        while (time_store.next() != NanoTime::maximum) {
            const auto start = chrono::steady_clock::now();
            time_store.pop(
                time_store.next(),
                [&](const size_t _tidx, const size_t _chidx) { ++first_call_count; },
                [&](const size_t _chidx, const size_t _newidx, const size_t _oldidx) { ++second_call_count; });

            const auto duration = chrono::duration_cast<chrono::nanoseconds>(chrono::steady_clock::now() - start);
            if (duration > max_duration) {
                max_duration = duration;
            }
            ++count;
            total_duration += duration;
        }
        cout << "max duration: " << max_duration << " size = " << time_store.size() << endl;
    }

    cout << "max duration: " << max_duration << endl;
    cout << "avg duration: " << total_duration / count << endl;
    return 0;
}