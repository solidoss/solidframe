#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipcservice.hpp"
#include "solid/utility/event.hpp"

#include "solid/system/log.hpp"

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
        : server_addr("localhost:3333")
    {
    }

    string server_addr;
};

//-----------------------------------------------------------------------------
namespace ipc_echo_client {

template <class M>
void complete_message(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<M>&              _rsent_msg_ptr,
    std::shared_ptr<M>&              _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    if (_rerror) {
        cout << "Error sending message to " << _rctx.recipientName() << ". Error: " << _rerror.message() << endl;
        return;
    }

    solid_check(_rrecv_msg_ptr && _rsent_msg_ptr);

    cout << "Received from " << _rctx.recipientName() << ": " << _rrecv_msg_ptr->str << endl;
}

struct MessageSetup {
    void operator()(ipc_echo::ProtocolT& _rprotocol, TypeToType<ipc_echo::Message> _t2t, const ipc_echo::ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerMessage<ipc_echo::Message>(complete_message<ipc_echo::Message>, _rtid);
    }
};

} // namespace ipc_echo_client

//-----------------------------------------------------------------------------

bool parseArguments(Parameters& _par, int argc, char* argv[]);

//-----------------------------------------------------------------------------
//      main
//-----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    Parameters p;

    solid::log_start(std::cerr, {"solid::frame::mpipc:VIEW", "\\*:VIEW"});

    if (!parseArguments(p, argc, argv))
        return 0;

    {

        AioSchedulerT          scheduler;
        frame::Manager         manager;
        frame::mpipc::ServiceT ipcservice(manager);
        FunctionWorkPool       fwp{WorkPoolConfiguration()};
        frame::aio::Resolver   resolver(fwp);
        ErrorConditionT        err;

        err = scheduler.start(1);

        if (err) {
            cout << "Error starting aio scheduler: " << err.message() << endl;
            return 1;
        }

        {
            auto                        proto = ipc_echo::ProtocolT::create();
            frame::mpipc::Configuration cfg(scheduler, proto);

            ipc_echo::protocol_setup(ipc_echo_client::MessageSetup(), *proto);

            cfg.client.name_resolve_fnc = frame::mpipc::InternetResolverF(resolver, "3333");

            cfg.client.connection_start_state = frame::mpipc::ConnectionState::Active;

            err = ipcservice.reconfigure(std::move(cfg));

            if (err) {
                cout << "Error starting ipcservice: " << err.message() << endl;
                return 1;
            }
        }

        frame::mpipc::RecipientId recipient_id;
        ipcservice.createConnectionPool(
            p.server_addr.c_str(),
            recipient_id,
            [](frame::mpipc::ConnectionContext& _rctx, Event&& _revt, const ErrorConditionT& _rerr) {
                solid_log(generic_logger, Verbose, "Connection pool event: " << _revt);
            });

        while (true) {
            string line;
            getline(cin, line);

            if (line == "q" || line == "Q" || line == "quit") {
                break;
            }
            ipcservice.sendMessage(recipient_id, make_shared<ipc_echo::Message>(std::move(line)), {frame::mpipc::MessageFlagsE::WaitResponse});
        }
    }
    return 0;
}

//-----------------------------------------------------------------------------

bool parseArguments(Parameters& _par, int argc, char* argv[])
{
    if (argc == 2) {
        _par.server_addr = argv[1];
    }
    return true;
}
