#include "solid/frame/mprpc/mprpcsocketstub_openssl.hpp"

#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioactor.hpp"
#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioresolver.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v2.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/system/exception.hpp"

#include "solid/system/log.hpp"

#include "multiprotocol_basic/alpha/server/alphaserver.hpp"
#include "multiprotocol_basic/beta/server/betaserver.hpp"
#include "multiprotocol_basic/gamma/server/gammaserver.hpp"

#include "multiprotocol_basic/alpha/client/alphaclient.hpp"
#include "multiprotocol_basic/beta/client/betaclient.hpp"
#include "multiprotocol_basic/gamma/client/gammaclient.hpp"

#include <iostream>

#include "multiprotocol_basic/clientcommon.hpp"

using namespace std;
using namespace solid;

typedef frame::aio::openssl::Context SecureContextT;

namespace {

std::atomic<size_t> wait_count(0);
mutex               mtx;
condition_variable  cnd;

std::atomic<uint64_t> transfered_size(0);
std::atomic<size_t>   transfered_count(0);
std::atomic<size_t>   connection_count(0);

void server_connection_stop(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
}

void server_connection_start(frame::mprpc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId());
}

} //namespace

int test_multiprotocol_basic(int argc, char* argv[])
{
    solid::log_start(std::cerr, {".*:EW"});

    size_t max_per_pool_connection_count = 1;

    if (argc > 1) {
        max_per_pool_connection_count = atoi(argv[1]);
        if (max_per_pool_connection_count == 0) {
            max_per_pool_connection_count = 1;
        }
        if (max_per_pool_connection_count > 100) {
            max_per_pool_connection_count = 100;
        }
    }

    {
        AioSchedulerT sch_client;
        AioSchedulerT sch_server;

        frame::Manager         m;
        frame::mprpc::ServiceT mprpcserver(m);
        ErrorConditionT        err;
        CallPool<void()>       cwp{WorkPoolConfiguration(), 1};
        frame::aio::Resolver   resolver(cwp);

        sch_client.start(1);
        sch_server.start(1);

        std::string server_port;

        { //mprpc server initialization
            auto                        proto = ProtocolT::create();
            frame::mprpc::Configuration cfg(sch_server, proto);

            proto->null(TypeIdT(0, 0));
            gamma_server::register_messages(*proto);
            beta_server::register_messages(*proto);
            alpha_server::register_messages(*proto);

            cfg.connection_stop_fnc         = &server_connection_stop;
            cfg.server.connection_start_fnc = &server_connection_start;

            cfg.server.connection_start_state = frame::mprpc::ConnectionState::Active;
            cfg.server.listener_address_str   = "0.0.0.0:0";

            mprpcserver.start(std::move(cfg));

            {
                std::ostringstream oss;
                oss << mprpcserver.configuration().server.listenerPort();
                server_port = oss.str();
                solid_dbg(generic_logger, Info, "server listens on port: " << server_port);
            }
        }

        Context ctx(sch_client, m, resolver, max_per_pool_connection_count, server_port, wait_count, mtx, cnd);

        err = alpha_client::start(ctx);

        if (err) {
            solid_dbg(generic_logger, Error, "starting alpha mprpcservice: " << err.message());
            //exiting
            return 1;
        }

        err = beta_client::start(ctx);

        if (err) {
            solid_dbg(generic_logger, Error, "starting alpha mprpcservice: " << err.message());
            //exiting
            return 1;
        }

        err = gamma_client::start(ctx);

        if (err) {
            solid_dbg(generic_logger, Error, "starting gamma mprpcservice: " << err.message());
            //exiting
            return 1;
        }

        unique_lock<mutex> lock(mtx);

        if (!cnd.wait_for(lock, std::chrono::seconds(120), []() { return wait_count == 0; })) {
            solid_throw("Process is taking too long.");
        }
        //client service must not outlive manager!!
        alpha_client::stop();
        beta_client::stop();
        gamma_client::stop();
    }

    //exiting

    std::cout << "Transfered size = " << (transfered_size * 2) / 1024 << "KB" << endl;
    std::cout << "Transfered count = " << transfered_count << endl;
    std::cout << "Connection count = " << connection_count << endl;
    return 0;
}
