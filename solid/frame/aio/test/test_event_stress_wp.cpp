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

using namespace std;
using namespace solid;

namespace {

using AtomicSizeT = atomic<size_t>;

struct AccountContext;
struct ConnectionContext;
struct DeviceContext;

using AccountCallPoolT    = CallPool<void(AccountContext&&)>;
using ConnectionCallPoolT = CallPool<void(ConnectionContext&)>;
using DeviceCallPoolT     = CallPool<void(DeviceContext&&)>;

struct ConnectionContext {
    AtomicSizeT       conn_cnt_;
    AccountCallPoolT& racc_cp_;
    promise<void>&    rprom_;
    ConnectionContext(AccountCallPoolT& _racc_cp, promise<void>& _rprom)
        : conn_cnt_(0)
        , racc_cp_(_racc_cp)
        , rprom_(_rprom)
    {
    }

    void pushConnection(size_t _acc, size_t _acc_con, size_t _repeat_count);
};

struct AccountContext {
    ConnectionCallPoolT& rconn_cp_;
    DeviceCallPoolT&     rdev_cp_;

    AccountContext(ConnectionCallPoolT& _rconn_cp, DeviceCallPoolT& _rdev_cp)
        : rconn_cp_(_rconn_cp)
        , rdev_cp_(_rdev_cp)
    {
    }

    void pushConnection(size_t _acc, size_t _acc_con, size_t _repeat_count);
    void pushConnectionToDevice(size_t _acc, size_t _acc_con, size_t _repeat_count);
};

struct DeviceContext {
    ConnectionCallPoolT& rconn_cp_;
    DeviceContext(ConnectionCallPoolT& _rconn_cp)
        : rconn_cp_(_rconn_cp)
    {
    }

    void pushConnection(size_t _acc, size_t _acc_con, size_t _repeat_count);
};

void AccountContext::pushConnection(size_t _acc, size_t _acc_con, size_t _repeat_count)
{
    rconn_cp_.push(
        [_acc, _acc_con, _repeat_count](ConnectionContext& _rctx) mutable {
            --_repeat_count;
            if (_repeat_count) {
                _rctx.pushConnection(_acc, _acc_con, _repeat_count);
            }
        });
}

void AccountContext::pushConnectionToDevice(size_t _acc, size_t _acc_con, size_t _repeat_count)
{
    rdev_cp_.push(
        [_acc, _acc_con, _repeat_count](DeviceContext&& _rctx) mutable {
            _rctx.pushConnection(_acc, _acc_con, _repeat_count);
        });
}

void ConnectionContext::pushConnection(size_t _acc, size_t _acc_con, size_t _repeat_count)
{
    racc_cp_.push(
        [_acc, _acc_con, _repeat_count](AccountContext&& _rctx) mutable {
            _rctx.pushConnectionToDevice(_acc, _acc_con, _repeat_count);
        });
}

void DeviceContext::pushConnection(size_t _acc, size_t _acc_con, size_t _repeat_count)
{
    rconn_cp_.push(
        [_acc, _acc_con, _repeat_count](ConnectionContext& _rctx) mutable {
            --_repeat_count;
            if (_repeat_count) {
                _rctx.pushConnection(_acc, _acc_con, _repeat_count);
            } else if (_rctx.conn_cnt_.fetch_sub(1) == 1) {
                //last connection
                _rctx.rprom_.set_value();
            }
        });
}

} //namespace

int test_event_stress_wp(int argc, char* argv[])
{
    install_crash_handler();

    solid::log_start(std::cerr, {".*:EWS"});

    size_t account_count            = 10000;
    size_t account_connection_count = 10;
    size_t account_device_count     = 20;
    size_t repeat_count             = 40;
    int    wait_seconds             = 50;

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

    (void)account_device_count;

    auto lambda = [&]() {
        AtomicSizeT   connection_count(0);
        promise<void> prom;

        ConnectionCallPoolT connection_cp{};
        DeviceCallPoolT     device_cp{};
        AccountCallPoolT    account_cp{};

        ConnectionContext conn_ctx(account_cp, prom);

        account_cp.start(WorkPoolConfiguration(), 1, AccountContext(connection_cp, device_cp));
        connection_cp.start(WorkPoolConfiguration(), 1, std::ref(conn_ctx));
        device_cp.start(WorkPoolConfiguration(), 1, DeviceContext(connection_cp));

        conn_ctx.conn_cnt_ = account_connection_count * account_count;

        for (size_t i = 0; i < account_count; ++i) {
            auto lambda = [i, account_connection_count, repeat_count](AccountContext&& _rctx) {
                for (size_t j = 0; j < account_connection_count; ++j) {
                    _rctx.pushConnection(i, j, repeat_count);
                }
            };
            account_cp.push(lambda);
        }

        if (prom.get_future().wait_for(chrono::seconds(wait_seconds)) != future_status::ready) {
            solid_dbg(workpool_logger, Statistic, "Connection pool:");
            connection_cp.dumpStatistics();
            solid_dbg(workpool_logger, Statistic, "Device pool:");
            device_cp.dumpStatistics();
            solid_dbg(workpool_logger, Statistic, "Account pool:");
            account_cp.dumpStatistics();
        }
    };

    if (async(launch::async, lambda).wait_for(chrono::seconds(wait_seconds + 10)) != future_status::ready) {
        solid_throw(" Test is taking too long - waited " << wait_seconds + 10 << " secs");
    }

    return 0;
}
