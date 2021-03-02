#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcrelayengines.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include "mprpc_echo_relay_register.hpp"

#include <iostream>

using namespace solid;
using namespace std;

using AioSchedulerT = frame::Scheduler<frame::aio::Reactor>;

//-----------------------------------------------------------------------------
//      Parameters
//-----------------------------------------------------------------------------
struct Parameters {
    Parameters()
        : listener_port("3333")
        , listener_addr("0.0.0.0")
    {
    }

    string listener_port;
    string listener_addr;
};

//-----------------------------------------------------------------------------

bool parseArguments(Parameters& _par, int argc, char* argv[]);

//-----------------------------------------------------------------------------
//      main
//-----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    Parameters p;

    if (!parseArguments(p, argc, argv))
        return 0;

    {
        AioSchedulerT                         scheduler;
        frame::Manager                        manager;
        frame::mprpc::relay::SingleNameEngine relay_engine(manager); //before relay service because it must outlive it
        frame::mprpc::ServiceT                rpcservice(manager);
        ErrorConditionT                       err;

        scheduler.start(1);

        {
            auto con_register = [&relay_engine](
                                    frame::mprpc::ConnectionContext& _rctx,
                                    std::shared_ptr<Register>&       _rsent_msg_ptr,
                                    std::shared_ptr<Register>&       _rrecv_msg_ptr,
                                    ErrorConditionT const&           _rerror) {
                solid_check(!_rerror);
                if (_rrecv_msg_ptr) {
                    solid_check(!_rsent_msg_ptr);
                    solid_log(generic_logger, Info, "recv register request: " << _rrecv_msg_ptr->name);

                    relay_engine.registerConnection(_rctx, std::move(_rrecv_msg_ptr->name));

                    ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

                    solid_check(!err, "Failed sending register response: " << err.message());

                } else {
                    solid_check(!_rrecv_msg_ptr);
                    solid_log(generic_logger, Info, "sent register response");
                }
            };

            auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
                reflection::v1::metadata::factory,
                [&](auto& _rmap) {
                    _rmap.template registerMessage<Register>(1, "Register", con_register);
                });

            frame::mprpc::Configuration cfg(scheduler, relay_engine, proto);

            cfg.server.listener_address_str = p.listener_addr;
            cfg.server.listener_address_str += ':';
            cfg.server.listener_address_str += p.listener_port;
            cfg.relay_enabled = true;

            cfg.server.connection_start_state = frame::mprpc::ConnectionState::Active;

            rpcservice.start(std::move(cfg));

            {
                std::ostringstream oss;
                oss << rpcservice.configuration().server.listenerPort();
                cout << "server listens on port: " << oss.str() << endl;
            }
        }

        cout << "Press ENTER to stop: ";
        cin.ignore();
    }
    return 0;
}

//-----------------------------------------------------------------------------

bool parseArguments(Parameters& _par, int argc, char* argv[])
{
    if (argc == 2) {
        size_t pos;

        _par.listener_addr = argv[1];

        pos = _par.listener_addr.rfind(':');

        if (pos != string::npos) {
            _par.listener_addr[pos] = '\0';

            _par.listener_port.assign(_par.listener_addr.c_str() + pos + 1);

            _par.listener_addr.resize(pos);
        }
    }
    return true;
}
