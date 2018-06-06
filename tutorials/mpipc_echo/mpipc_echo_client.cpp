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
        : port("3333")
    {
    }

    string port;
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

    SOLID_CHECK(_rrecv_msg_ptr && _rsent_msg_ptr);

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

    if (!parseArguments(p, argc, argv))
        return 0;

    {

        AioSchedulerT scheduler;

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

            cfg.client.name_resolve_fnc = frame::mpipc::InternetResolverF(resolver, p.port.c_str());

            cfg.client.connection_start_state = frame::mpipc::ConnectionState::Active;

            err = ipcservice.reconfigure(std::move(cfg));

            if (err) {
                cout << "Error starting ipcservice: " << err.message() << endl;
                return 1;
            }
        }

        while (true) {
            string line;
            getline(cin, line);

            if (line == "q" || line == "Q" || line == "quit") {
                break;
            }
            {
                string recipient;
                size_t offset = line.find(' ');
                if (offset != string::npos) {
                    recipient = line.substr(0, offset);
                    ipcservice.sendMessage(recipient.c_str(), make_shared<ipc_echo::Message>(line.substr(offset + 1)), {frame::mpipc::MessageFlagsE::WaitResponse});
                } else {
                    cout << "No recipient specified. E.g:" << endl
                         << "localhost:4444 Some text to send" << endl;
                }
            }
        }
    }
    return 0;
}

//-----------------------------------------------------------------------------

bool parseArguments(Parameters& _par, int argc, char* argv[])
{
    if (argc == 2) {
        _par.port = argv[1];
    }
    return true;
}
