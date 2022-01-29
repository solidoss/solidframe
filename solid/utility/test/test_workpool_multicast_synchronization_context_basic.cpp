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

thread_local uint32_t thread_local_value = 0;

constexpr uint32_t synch_context_count = 10;

struct Record {
    uint32_t value_           = -1;
    uint32_t context_id_      = -1;
    uint32_t multicast_value_ = -1;
    size_t   validation_      = 0;

    Record(const uint32_t _value = -1, const uint32_t _context_id = -1, const size_t _validation = 0)
        : value_(_value)
        , context_id_(_context_id)
        , validation_(_validation)
    {
    }
};

using WorkPoolT = locking::WorkPoolT<Record, uint32_t>;

struct SynchContext {
    WorkPoolT::SynchronizationContextT ctx_;
    size_t                             validation_ = 0;
};

size_t synch_contexts_validations[synch_context_count] = {0};

} // namespace

int test_workpool_multicast_synchronization_context_basic(int argc, char* argv[])
{

    solid::log_start(std::cerr, {".*:EWXS", "test:VIEWS"});

#ifdef SOLID_SANITIZE_THREAD
    const int wait_seconds = 1500;
#else
    const int wait_seconds = 10000;
#endif
    const size_t count{5000000};

    auto lambda = [&]() {
        deque<Record> record_dq;
        record_dq.resize(count);

        WorkPoolT wp{
            WorkPoolConfiguration{2},
            [&record_dq](const Record& _r) {
                solid_check(record_dq[_r.value_].multicast_value_ == static_cast<uint32_t>(-1));
                record_dq[_r.value_].multicast_value_ = thread_local_value;
                record_dq[_r.value_].value_           = _r.value_;
                record_dq[_r.value_].context_id_      = _r.context_id_;
                if (_r.context_id_ != InvalidIndex{}) {
                    solid_check(synch_contexts_validations[_r.context_id_]++ == _r.validation_);
                }
                //solid_log(logger, Verbose, "job " << _r.value_);
            },
            [](const uint32_t _v) { //mcast execute
                thread_local_value = _v;
                //solid_log(logger, Verbose, "mcast " << _v);
            }};
        {
            SynchContext synch_contexts[synch_context_count];

            //we leave the last contex empty for async tasks
            for (size_t i = 0; i < (synch_context_count - 1); ++i) {
                synch_contexts[i].ctx_ = wp.createSynchronizationContext();
            }

            for (uint32_t i = 0; i < count; ++i) {
                if ((i % 100) == 0) {
                    wp.pushAll(i);
                }

                auto& rsynch_context = synch_contexts[i % synch_context_count];

                if (!rsynch_context.ctx_.empty()) {
                    rsynch_context.ctx_.push(Record{i, i % synch_context_count, rsynch_context.validation_++});
                } else {
                    wp.push(Record{i, InvalidIndex{}}); //async
                }
            }
        }
    };

    auto fut = async(launch::async, lambda);
    if (fut.wait_for(chrono::seconds(wait_seconds)) != future_status::ready) {

        solid_throw(" Test is taking too long - waited " << wait_seconds << " secs");
    }
    fut.get();

    solid_log(logger, Verbose, "after async wait");

    return 0;
}
