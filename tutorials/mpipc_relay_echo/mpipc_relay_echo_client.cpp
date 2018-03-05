#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipcservice.hpp"

#include "mpipc_relay_echo_register.hpp"

#include <iostream>

using namespace solid;
using namespace std;

using AioSchedulerT = frame::Scheduler<frame::aio::Reactor>;

//-----------------------------------------------------------------------------
//      Parameters
//-----------------------------------------------------------------------------
struct Parameters {
    Parameters()
        : server_port("3333")
        , server_addr("127.0.0.1")
    {
    }
    string name;
    string server_port;
    string server_addr;
};

struct Message : solid::frame::mpipc::Message {
    std::string name;
    std::string data;

    Message() {}

    Message(const std::string& _name, std::string&& _ustr)
        : name(_name)
        , data(std::move(_ustr))
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.name, _rctx, "name");
        _s.add(_rthis.data, _rctx, "data");
    }
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
        AioSchedulerT          scheduler;
        frame::Manager         manager;
        frame::mpipc::ServiceT ipcservice(manager);
        frame::aio::Resolver   resolver;
        ErrorConditionT        err;

        err = scheduler.start(1);

        if (err) {
            cout << "Error starting aio scheduler: " << err.message() << endl;
            return 1;
        }

        err = resolver.start(1);

        if (err) {
            cout << "Error starting aio resolver: " << err.message() << endl;
            return 1;
        }

        {
            auto con_register = [](
                                    frame::mpipc::ConnectionContext& _rctx,
                                    std::shared_ptr<Register>&       _rsent_msg_ptr,
                                    std::shared_ptr<Register>&       _rrecv_msg_ptr,
                                    ErrorConditionT const&           _rerror) {
                SOLID_CHECK(!_rerror);

                if (_rrecv_msg_ptr and _rrecv_msg_ptr->name.empty()) {
                    auto lambda = [](frame::mpipc::ConnectionContext&, ErrorConditionT const& _rerror) {
                        idbg("peerb --- enter active error: " << _rerror.message());
                        return frame::mpipc::MessagePointerT();
                    };
                    cout << "Connection registered" << endl;
                    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId(), lambda);
                }
            };
            auto on_message = [&p](
                                  frame::mpipc::ConnectionContext& _rctx,
                                  std::shared_ptr<Message>&        _rsent_msg_ptr,
                                  std::shared_ptr<Message>&        _rrecv_msg_ptr,
                                  ErrorConditionT const&           _rerror) {

                if (_rrecv_msg_ptr) {
                    cout << _rrecv_msg_ptr->name << ": " << _rrecv_msg_ptr->data << endl;
                    if (!_rsent_msg_ptr) {
                        //we're on peer - echo back the response
                        _rrecv_msg_ptr->name = p.name;
                        ErrorConditionT err  = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

                        (void)err;
                    }
                }
            };
            auto on_connection_start = [&p](frame::mpipc::ConnectionContext& _rctx) {
                idbg(_rctx.recipientId());

                auto            msgptr = std::make_shared<Register>(p.name);
                ErrorConditionT err    = _rctx.service().sendMessage(_rctx.recipientId(), std::move(msgptr), {frame::mpipc::MessageFlagsE::WaitResponse});
                SOLID_CHECK(not err, "failed send Register");
            };

            auto on_connection_stop = [&p](frame::mpipc::ConnectionContext& _rctx) {
                cout << "Connection stopped" << endl;
            };

            auto                        proto = ProtocolT::create();
            frame::mpipc::Configuration cfg(scheduler, proto);

            proto->null(null_type_id);
            proto->registerMessage<Register>(con_register, register_type_id);
            proto->registerMessage<Message>(on_message, TypeIdT{1, 1});

            cfg.client.connection_start_fnc = std::move(on_connection_start);
            cfg.connection_stop_fnc         = std::move(on_connection_stop);

            cfg.client.name_resolve_fnc = frame::mpipc::InternetResolverF(resolver, p.server_port.c_str());

            err = ipcservice.reconfigure(std::move(cfg));

            if (err) {
                cout << "Error starting ipcservice: " << err.message() << endl;
                return 1;
            }
        }

        ipcservice.createConnectionPool(p.server_addr.c_str());

        while (true) {
            string line;
            getline(cin, line);

            if (line == "q" or line == "Q" or line == "quit") {
                break;
            }
            {
                string recipient;
                size_t offset = line.find(' ');
                if (offset != string::npos) {
                    recipient = p.server_addr + '/' + line.substr(0, offset);
                    ipcservice.sendMessage(recipient.c_str(), make_shared<Message>(p.name, line.substr(offset + 1)), {frame::mpipc::MessageFlagsE::WaitResponse});
                } else {
                    cout << "No recipient name specified. E.g:" << endl
                         << "alpha Some text to send" << endl;
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
        _par.name = argv[1];
        return true;
    }
    if (argc == 3) {
        size_t pos;

        _par.name = argv[1];

        _par.server_addr = argv[2];

        pos = _par.server_addr.rfind(':');

        if (pos != string::npos) {
            _par.server_addr[pos] = '\0';

            _par.server_port.assign(_par.server_addr.c_str() + pos + 1);

            _par.server_addr.resize(pos);
        }
        return true;
    }
    cout << "Usage: " << endl
         << argv[0] << " my_name [server_addr:server_port]" << endl;
    return false;
}
