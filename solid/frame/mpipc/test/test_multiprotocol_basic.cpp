#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioresolver.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include "solid/frame/aio/openssl/aiosecurecontext.hpp"
#include "solid/frame/aio/openssl/aiosecuresocket.hpp"

#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v2.hpp"
#include "solid/frame/mpipc/mpipcservice.hpp"

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

void server_connection_stop(frame::mpipc::ConnectionContext& _rctx)
{
    solid_dbg(generic_logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
}

void server_connection_start(frame::mpipc::ConnectionContext& _rctx)
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
        frame::mpipc::ServiceT mpipcserver(m);
        ErrorConditionT        err;
        FunctionWorkPool       fwp;
        frame::aio::Resolver   resolver(fwp);

        fwp.start(WorkPoolConfiguration());
        err = sch_client.start(1);

        if (err) {
            solid_dbg(generic_logger, Error, "starting aio client scheduler: " << err.message());
            return 1;
        }

        err = sch_server.start(1);

        if (err) {
            solid_dbg(generic_logger, Error, "starting aio server scheduler: " << err.message());
            return 1;
        }

        std::string server_port;

        { //mpipc server initialization
            auto                        proto = ProtocolT::create();
            frame::mpipc::Configuration cfg(sch_server, proto);

            proto->null(TypeIdT(0, 0));
            gamma_server::register_messages(*proto);
            beta_server::register_messages(*proto);
            alpha_server::register_messages(*proto);

            cfg.connection_stop_fnc         = &server_connection_stop;
            cfg.server.connection_start_fnc = &server_connection_start;

            cfg.server.connection_start_state = frame::mpipc::ConnectionState::Active;
            cfg.server.listener_address_str   = "0.0.0.0:0";

            err = mpipcserver.reconfigure(std::move(cfg));

            if (err) {
                solid_dbg(generic_logger, Error, "starting server mpipcservice: " << err.message());
                //exiting
                return 1;
            }

            {
                std::ostringstream oss;
                oss << mpipcserver.configuration().server.listenerPort();
                server_port = oss.str();
                solid_dbg(generic_logger, Info, "server listens on port: " << server_port);
            }
        }

        Context ctx(sch_client, m, resolver, max_per_pool_connection_count, server_port, wait_count, mtx, cnd);

        err = alpha_client::start(ctx);

        if (err) {
            solid_dbg(generic_logger, Error, "starting alpha mpipcservice: " << err.message());
            //exiting
            return 1;
        }

        err = beta_client::start(ctx);

        if (err) {
            solid_dbg(generic_logger, Error, "starting alpha mpipcservice: " << err.message());
            //exiting
            return 1;
        }

        err = gamma_client::start(ctx);

        if (err) {
            solid_dbg(generic_logger, Error, "starting gamma mpipcservice: " << err.message());
            //exiting
            return 1;
        }

        unique_lock<mutex> lock(mtx);

        if (!cnd.wait_for(lock, std::chrono::seconds(120), []() { return wait_count == 0; })) {
            SOLID_THROW("Process is taking too long.");
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
