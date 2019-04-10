/*
 * This test is companion to test_event_stress.
 * It tries to simulate the message passing from test_event_stress using Workpool instead of
 * actors and schedulers.
 */

#include "solid/utility/workpool.hpp"
#include "solid/utility/string.hpp"
#include "solid/utility/function.hpp"

#include <future>

using namespace std;
using namespace solid;


namespace{

using AtomicSizeT   = atomic<size_t>;
    
template <typename Ctx>
class FunctionWorkPool : public WorkPool<solid::Function<void(Ctx&)>> {
    using WorkPoolT = WorkPool<solid::Function<void(Ctx&)>>;

    Ctx& rctx_;

public:
    FunctionWorkPool(
        const size_t                 _start_wkr_cnt,
        const WorkPoolConfiguration& _cfg,
        Ctx&                         _rctx)
        : WorkPoolT(
              _start_wkr_cnt,
              _cfg,
              [this](std::function<void(Ctx&)>& _rfnc) {
                  _rfnc(this->context());
              })
        , rctx_(_rctx)
    {
    }

    FunctionWorkPool(
        const WorkPoolConfiguration& _cfg,
        Ctx&                         _rctx)
        : WorkPoolT(
              0,
              _cfg,
              [this](std::function<void(Ctx&)>& _rfnc) {
                  _rfnc(this->context());
              })
        , rctx_(_rctx)
    {
    }

    Ctx& context() const
    {
        return rctx_;
    }
};


}//namespace

int test_event_stress(int argc, char* argv[])
{
    size_t account_count            = 10000;
    size_t account_connection_count = 10;
    size_t account_device_count     = 20;
    size_t repeat_count             = 40;
    int    wait_seconds             = 10000;

    if (argc > 1) {
        repeat_count = make_number(argv[1]);
    }

    if (argc > 2) {
        account_count = make_number(argv[2]);
    }

    if (argc > 3) {
        account_connection_count = make_number(argv[3]);
    }

    if (argc > 4) {
        account_device_count = make_number(argv[4]);
    }
    
    auto lambda = [&]() {
        AtomicSizeT     connection_count(0);
        promise<void>   prom;
        
        ConnectionWorkpoolT     connection_wp;
        AccountWorkpoolT        account_wp;
        DeviceWorkpoolT         device_wp;
        
        for(size_t i = 0; i < account_count; ++i){
            auto lambda = [account_connection_count](AccountContext &_rctx){
                for(size_t i = 0; i < account_connection_count; ++i){
                    _rctx.pushConnection(i);
                }
            };
            account_wp.push(lambda);
        }
    };

    if (async(launch::async, lambda).wait_for(chrono::seconds(wait_seconds)) != future_status::ready) {
        solid_throw(" Test is taking too long - waited " << wait_seconds << " secs");
    }
    
    return 0;
}
