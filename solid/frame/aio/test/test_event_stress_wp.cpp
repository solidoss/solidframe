/*
 * This test is companion to test_event_stress.
 * It tries to simulate the message passing from test_event_stress using Workpool instead of
 * actors and schedulers.
 */

#include "solid/system/crashhandler.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/string.hpp"
#include "solid/utility/workpool.hpp"

#include <future>
#include <iostream>
#include <thread>

using namespace std;
using namespace solid;

namespace {

using AtomicSizeT  = atomic<size_t>;
using AtomicSSizeT = atomic<ssize_t>;

struct AccountContext;
struct ConnectionContext;
struct DeviceContext;

using AccountCallPoolT    = CallPool<void(AccountContext&), workpoll_default_node_capacity_bit_count, impl::StressTestWorkPoolBase<30>>;
using ConnectionCallPoolT = CallPool<void(ConnectionContext&), workpoll_default_node_capacity_bit_count, impl::StressTestWorkPoolBase<30>>;
using DeviceCallPoolT     = CallPool<void(DeviceContext&), workpoll_default_node_capacity_bit_count, impl::StressTestWorkPoolBase<30>>;

struct GlobalContext {
    atomic<bool>       stopping_;
    mutex              mtx_;
    condition_variable cnd_;
};

struct Context {
    AtomicSizeT    use_count_;
    GlobalContext& rgctx_;

    Context(GlobalContext& _rgctx)
        : use_count_(0)
        , rgctx_(_rgctx)
    {
    }

    void enter()
    {
        ++use_count_;
    }

    void exit()
    {
        if (use_count_.fetch_sub(1) == 1 && rgctx_.stopping_) {
            lock_guard<mutex> lock(rgctx_.mtx_);
            rgctx_.cnd_.notify_one();
        }
    }

    void wait()
    {
        solid_check(rgctx_.stopping_);
        unique_lock<mutex> lock(rgctx_.mtx_);
        rgctx_.cnd_.wait(lock, [this]() { return use_count_ == 0; });
    }
};

struct ConnectionContext : Context {
    AtomicSSizeT      conn_cnt_;
    AccountCallPoolT& racc_cp_;
    promise<void>&    rprom_;
    ConnectionContext(GlobalContext& _rgctx, AccountCallPoolT& _racc_cp, promise<void>& _rprom)
        : Context(_rgctx)
        , conn_cnt_(0)
        , racc_cp_(_racc_cp)
        , rprom_(_rprom)
    {
    }

    void pushConnection(size_t _acc, size_t _acc_con, size_t _repeat_count);
};

struct AccountContext : Context {
    ConnectionCallPoolT& rconn_cp_;
    DeviceCallPoolT&     rdev_cp_;

    AccountContext(GlobalContext& _rgctx, ConnectionCallPoolT& _rconn_cp, DeviceCallPoolT& _rdev_cp)
        : Context(_rgctx)
        , rconn_cp_(_rconn_cp)
        , rdev_cp_(_rdev_cp)
    {
    }

    void pushConnection(size_t _acc, size_t _acc_con, size_t _repeat_count);
    void pushConnectionToDevice(size_t _acc, size_t _acc_con, size_t _repeat_count);
};

struct DeviceContext : Context {
    ConnectionCallPoolT& rconn_cp_;
    DeviceContext(GlobalContext& _rgctx, ConnectionCallPoolT& _rconn_cp)
        : Context(_rgctx)
        , rconn_cp_(_rconn_cp)
    {
    }

    void pushConnection(size_t _acc, size_t _acc_con, size_t _repeat_count);
};

void AccountContext::pushConnection(size_t _acc, size_t _acc_con, size_t _repeat_count)
{
    enter();
    rconn_cp_.push(
        [_acc, _acc_con, _repeat_count](ConnectionContext& _rctx) mutable {
            solid_assert(_repeat_count > 0 && _repeat_count < 1000000);
            --_repeat_count;
            if (_repeat_count) {
                _rctx.pushConnection(_acc, _acc_con, _repeat_count);
            }
        });
    exit();
}

void AccountContext::pushConnectionToDevice(size_t _acc, size_t _acc_con, size_t _repeat_count)
{
    enter();
    rdev_cp_.push(
        [_acc, _acc_con, _repeat_count](DeviceContext& _rctx) mutable {
            _rctx.pushConnection(_acc, _acc_con, _repeat_count);
        });
    exit();
}

void ConnectionContext::pushConnection(size_t _acc, size_t _acc_con, size_t _repeat_count)
{
    enter();
    racc_cp_.push(
        [_acc, _acc_con, _repeat_count](AccountContext& _rctx) mutable {
            _rctx.pushConnectionToDevice(_acc, _acc_con, _repeat_count);
        });
    exit();
}

void DeviceContext::pushConnection(size_t _acc, size_t _acc_con, size_t _repeat_count)
{
    enter();
    rconn_cp_.push(
        [_acc, _acc_con, _repeat_count](ConnectionContext& _rctx) mutable {
            volatile size_t repeat_count     = _repeat_count;
            ConnectionContext* volatile pctx = &_rctx;
            solid_assert(repeat_count > 0 && repeat_count < 1000000);
            --repeat_count;
            if (repeat_count) {
                if (_rctx.conn_cnt_ <= 0) {
                    solid_dbg(workpool_logger, Warning, "OVERPUSH " << _acc << ' ' << _acc_con << ' ' << repeat_count);
                }
                solid_assert(_rctx.conn_cnt_ > 0);
                pctx->pushConnection(_acc, _acc_con, repeat_count);
            } else if (_rctx.conn_cnt_.fetch_sub(1) == 1) {
                //last connection
                _rctx.rprom_.set_value();
                solid_dbg(workpool_logger, Warning, "DONE - notify " << _acc << ' ' << _acc_con << ' ' << _repeat_count);
            } else if (_rctx.conn_cnt_ < 0) {
                solid_assert_log(false, workpool_logger, "DONE - notify " << _acc << ' ' << _acc_con << ' ' << _repeat_count);
            }
        });
    exit();
}
/// ->AccountP->ConnectionP->AccountP->DeviceP->ConnectionP->AccountP->DeviceP->ConnectionP
} //namespace

int test_event_stress_wp(int argc, char* argv[])
{
    //install_crash_handler();

    solid::log_start(std::cerr, {".*:EWXS"});

    size_t account_count            = 10000;
    size_t account_connection_count = 10;
    size_t account_device_count     = 20;
    size_t repeat_count             = 40;
    int    wait_seconds             = 200;
    size_t thread_count             = 0;

    if (argc > 1) {
        thread_count = make_number(argv[1]);
    }

    if (argc > 2) {
        repeat_count = make_number(argv[2]);
    }

    if (argc > 3) {
        account_count = make_number(argv[3]);
    }

    if (argc > 4) {
        account_connection_count = make_number(argv[4]);
    }

    if (argc > 5) {
        account_device_count = make_number(argv[5]);
    }

    if (thread_count == 0) {
        thread_count = thread::hardware_concurrency();
    }
    (void)account_device_count;

    auto lambda = [&]() {
        {
            AtomicSizeT         connection_count(0);
            promise<void>       prom;
            ConnectionCallPoolT connection_cp{};
            DeviceCallPoolT     device_cp{};
            AccountCallPoolT    account_cp{};
            GlobalContext       gctx;
            ConnectionContext   conn_ctx(gctx, account_cp, prom);
            AccountContext      acc_ctx(gctx, connection_cp, device_cp);
            DeviceContext       dev_ctx(gctx, connection_cp);

            gctx.stopping_ = false;

            account_cp.start(WorkPoolConfiguration(thread_count), 1, std::ref(acc_ctx));
            connection_cp.start(WorkPoolConfiguration(thread_count), 1, std::ref(conn_ctx));
            device_cp.start(WorkPoolConfiguration(thread_count), 1, std::ref(dev_ctx));

            conn_ctx.conn_cnt_  = (account_connection_count * account_count);
            auto produce_lambda = [&]() {
                for (size_t i = 0; i < account_count; ++i) {
                    auto lambda = [i, account_connection_count, repeat_count](AccountContext& _rctx) {
                        for (size_t j = 0; j < account_connection_count; ++j) {
                            _rctx.pushConnection(i, j, repeat_count);
                        }
                    };
                    account_cp.push(lambda);
                }
                solid_dbg(workpool_logger, Statistic, "producer done");
            };
            if (0) {
                auto fut = async(launch::async, produce_lambda);
                fut.wait();
            } else {
                produce_lambda();
            }
            {
                auto fut = prom.get_future();
                if (fut.wait_for(chrono::seconds(wait_seconds)) != future_status::ready) {
                    solid_dbg(workpool_logger, Statistic, "Connection pool:");
                    connection_cp.dumpStatistics();
                    solid_dbg(workpool_logger, Statistic, "Device pool:");
                    device_cp.dumpStatistics();
                    solid_dbg(workpool_logger, Statistic, "Account pool:");
                    account_cp.dumpStatistics();
                    solid_dbg(workpool_logger, Warning, "sleep - wait for locked threads");
                    this_thread::sleep_for(chrono::seconds(100));
                    solid_dbg(workpool_logger, Warning, "wake - waited for locked threads");
                    //we must throw here otherwise it will crash because workpool(s) is/are used after destroy
                    solid_throw(" Test is taking too long - waited " << wait_seconds << " secs");
                }
            }
            solid_dbg(workpool_logger, Statistic, "connections done");
            //this_thread::sleep_for(chrono::milliseconds(100));
            gctx.stopping_ = true;
            conn_ctx.wait();
            solid_dbg(workpool_logger, Statistic, "conn_ctx done");
            acc_ctx.wait();
            solid_dbg(workpool_logger, Statistic, "acc_ctx done");
            dev_ctx.wait();
            solid_dbg(workpool_logger, Statistic, "dev_ctx done");

            //need explicit stop because pools use contexts which are destroyed before pools
            account_cp.stop();
            solid_dbg(workpool_logger, Statistic, "account pool stopped "<<&account_cp);
            device_cp.stop();
            solid_dbg(workpool_logger, Statistic, "device pool stopped "<<&device_cp);
            connection_cp.stop();
            solid_dbg(workpool_logger, Statistic, "connection pool stopped "<<&connection_cp);
        }
        int* p = new int[1000];
        delete[] p;
    };
    auto fut = async(launch::async, lambda);
    if (fut.wait_for(chrono::seconds(wait_seconds + 110)) != future_status::ready) {
        solid_throw(" Test is taking too long - waited " << wait_seconds + 110 << " secs");
    }

    return 0;
}
