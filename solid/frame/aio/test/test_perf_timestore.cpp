#include "solid/frame/timestore.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace std;
using namespace solid;
using namespace std::chrono_literals;

namespace {

const LoggerT logger("test");

std::vector<chrono::microseconds> pattern{
    30s, 20s, 10s, 30us, 20us, 10us, 30min, 20min, 10min, 30ms, 20ms, 10ms,
    20s, 19s, 18s, 17s, 16s, 15s, 14s, 13s, 12s, 11s,
    50ms, 49ms, 48ms, 47ms, 46ms, 45ms, 44ms, 43ms, 42ms, 41ms,
    20min, 20s, 20ms, 19min, 19s, 19ms, 18min, 18s, 18ms, 17min, 17s, 17ms, 16min, 16s, 16ms,
    15min, 15s, 15ms, 14min, 14s, 14ms, 13min, 13s, 13ms, 12min, 12s, 12ms, 11min, 11s, 11s};

} // namespace

int test_perf_timestore(int argc, char* argv[])
{
    solid::log_start(std::cerr, {".*:EWXS", "test:EWS"});

    size_t repeat_count     = 100;
    size_t timer_count      = 10000;
    size_t add_update_count = 100;

    int version = 2;

    if (argc > 1) {
        version = atoi(argv[1]);
    }

    chrono::nanoseconds max_duration(0);
    chrono::nanoseconds total_duration(0);
    size_t              count = 0;

    if (version == 1) {

        std::deque<std::tuple<NanoTime, size_t>> timers;
        frame::TimeStore                         time_store;
        NanoTime                                 now = NanoTime::nowSteady();

        for (size_t i = 0; i < timer_count; ++i) {
            timers.emplace_back(NanoTime::max(), InvalidIndex{});
        }
        size_t index = 0;
        for (size_t r = 0; r < repeat_count; ++r) {
            for (size_t i = 0; i < add_update_count; ++i) {
                const auto idx = index % timer_count;
                auto&      rt  = timers[idx];

                if (get<0>(rt) == NanoTime::max()) {
                    get<0>(rt) = now + pattern[index % pattern.size()];
                    get<1>(rt) = time_store.push(now, get<0>(rt), idx);
                    solid_log(logger, Verbose, "push " << now << " " << get<0>(rt) << " " << i << " now+" << pattern[index % pattern.size()]);
                } else {
                    const auto oldexp = get<0>(rt);
                    get<0>(rt)        = get<0>(rt) + pattern[index % pattern.size()];
                    time_store.update(get<1>(rt), now, get<0>(rt));
                    solid_log(logger, Verbose, "update " << now << " from " << oldexp << " to " << get<0>(rt) << " " << idx << " now+" << pattern[index % pattern.size()]);
                }
                ++index;
            }

            now = time_store.expiry() + 10ms;

            const auto start = chrono::steady_clock::now();

            time_store.pop(now, [&](const size_t _i, const NanoTime& _expiry, const size_t _proxy_index) {
                auto& rt = timers[_i];
                solid_check(get<1>(rt) == _proxy_index && get<0>(rt) == _expiry && now >= get<0>(rt));
                get<0>(rt) = NanoTime::max();
            });

            const auto duration = chrono::duration_cast<chrono::nanoseconds>(chrono::steady_clock::now() - start);
            if (duration > max_duration) {
                max_duration = duration;
            }
            ++count;
            total_duration += duration;
        }

        while (!time_store.empty()) {
            now = time_store.expiry();
            time_store.pop(now, [&](const size_t _i, const NanoTime& _expiry, const size_t _proxy_index) {
                auto& rt = timers[_i];
                solid_check(get<1>(rt) == _proxy_index && get<0>(rt) == _expiry && now >= get<0>(rt));
                get<0>(rt) = NanoTime::max();
            });
        }
    }

    cout << "max duration: " << max_duration << endl;
    cout << "avg duration: " << total_duration / count << endl;
    return 0;
}