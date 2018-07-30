#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipcservice.hpp"

#include "mpipc_echo_messages.hpp"

#include <iostream>

using namespace solid;
using namespace std;

using AioSchedulerT = frame::Scheduler<frame::aio::Reactor>;

//-----------------------------------------------------------------------------
//      Parameters
//-----------------------------------------------------------------------------
struct Parameters {
    Parameters()
        : listener_port("0")
        , listener_addr("0.0.0.0")
    {
    }

    string listener_port;
    string listener_addr;
};

//-----------------------------------------------------------------------------

namespace ipc_echo_server {

template <class M>
void complete_message(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<M>&              _rsent_msg_ptr,
    std::shared_ptr<M>&              _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    solid_check(!_rerror);

    if (_rrecv_msg_ptr) {
        solid_check(!_rsent_msg_ptr);
        solid_check(!_rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr)));
    }

    if (_rsent_msg_ptr) {
        solid_check(!_rrecv_msg_ptr);
    }
}

struct MessageSetup {
    void operator()(ipc_echo::ProtocolT& _rprotocol, TypeToType<ipc_echo::Message> _t2t, const ipc_echo::ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerMessage<ipc_echo::Message>(complete_message<ipc_echo::Message>, _rtid);
    }
};

} // namespace ipc_echo_server

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

        AioSchedulerT scheduler;

        frame::Manager         manager;
        frame::mpipc::ServiceT ipcservice(manager);
        ErrorConditionT        err;

        err = scheduler.start(1);

        if (err) {
            cout << "Error starting aio scheduler: " << err.message() << endl;
            return 1;
        }

        {
            auto                        proto = ipc_echo::ProtocolT::create();
            frame::mpipc::Configuration cfg(scheduler, proto);

            ipc_echo::protocol_setup(ipc_echo_server::MessageSetup(), *proto);

            cfg.server.listener_address_str = p.listener_addr;
            cfg.server.listener_address_str += ':';
            cfg.server.listener_address_str += p.listener_port;

            cfg.server.connection_start_state = frame::mpipc::ConnectionState::Active;

            err = ipcservice.reconfigure(std::move(cfg));

            if (err) {
                cout << "Error starting ipcservice: " << err.message() << endl;
                manager.stop();
                return 1;
            }
            {
                std::ostringstream oss;
                oss << ipcservice.configuration().server.listenerPort();
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
