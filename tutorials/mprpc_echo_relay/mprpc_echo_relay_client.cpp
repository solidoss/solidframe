#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include "mprpc_echo_relay_register.hpp"

#include "solid/utility/workpool.hpp"
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

struct Message : solid::frame::mprpc::Message {
    std::string name;
    std::string data;

    Message() {}

    Message(const std::string& _name, std::string&& _ustr)
        : name(_name)
        , data(std::move(_ustr))
    {
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.name, _rctx, 1, "name");
        _rr.add(_rthis.data, _rctx, 2, "data");
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
        AioSchedulerT                     scheduler;
        frame::Manager                    manager;
        frame::mprpc::ServiceT            rpcservice(manager);
        ErrorConditionT                   err;
        lockfree::CallPoolT<void(), void> cwp{WorkPoolConfiguration(1)};
        frame::aio::Resolver              resolver([&cwp](std::function<void()>&& _fnc) { cwp.push(std::move(_fnc)); });

        (WorkPoolConfiguration());

        scheduler.start(1);

        {
            auto con_register = [](
                                    frame::mprpc::ConnectionContext& _rctx,
                                    std::shared_ptr<Register>&       _rsent_msg_ptr,
                                    std::shared_ptr<Register>&       _rrecv_msg_ptr,
                                    ErrorConditionT const&           _rerror) {
                solid_check(!_rerror);

                if (_rrecv_msg_ptr && _rrecv_msg_ptr->name.empty()) {
                    auto lambda = [](frame::mprpc::ConnectionContext&, ErrorConditionT const& _rerror) {
                        solid_log(generic_logger, Info, "peerb --- enter active error: " << _rerror.message());
                    };
                    cout << "Connection registered" << endl;
                    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId(), lambda);
                }
            };
            auto on_message = [&p](
                                  frame::mprpc::ConnectionContext& _rctx,
                                  std::shared_ptr<Message>&        _rsent_msg_ptr,
                                  std::shared_ptr<Message>&        _rrecv_msg_ptr,
                                  ErrorConditionT const&           _rerror) {
                if (_rrecv_msg_ptr) {
                    cout << _rrecv_msg_ptr->name << ": " << _rrecv_msg_ptr->data << endl;
                    if (!_rsent_msg_ptr) {
                        // we're on peer - echo back the response
                        _rrecv_msg_ptr->name = p.name;
                        ErrorConditionT err  = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

                        (void)err;
                    }
                }
            };
            auto on_connection_start = [&p](frame::mprpc::ConnectionContext& _rctx) {
                solid_log(generic_logger, Info, _rctx.recipientId());

                auto            msgptr = std::make_shared<Register>(p.name);
                ErrorConditionT err    = _rctx.service().sendMessage(_rctx.recipientId(), std::move(msgptr), {frame::mprpc::MessageFlagsE::AwaitResponse});
                solid_check(!err, "failed send Register");
            };

            auto on_connection_stop = [](frame::mprpc::ConnectionContext& /*_rctx*/) {
                cout << "Connection stopped" << endl;
            };

            auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
                reflection::v1::metadata::factory,
                [&](auto& _rmap) {
                    _rmap.template registerMessage<Register>(1, "Register", con_register);
                    _rmap.template registerMessage<Message>(2, "Message", on_message);
                });
            frame::mprpc::Configuration cfg(scheduler, proto);

            cfg.client.connection_start_fnc = std::move(on_connection_start);
            cfg.connection_stop_fnc         = std::move(on_connection_stop);

            cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(resolver, p.server_port.c_str());

            rpcservice.start(std::move(cfg));
        }

        rpcservice.createConnectionPool(p.server_addr.c_str());

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
                    recipient = p.server_addr + '/' + line.substr(0, offset);
                    rpcservice.sendMessage(recipient.c_str(), make_shared<Message>(p.name, line.substr(offset + 1)), {frame::mprpc::MessageFlagsE::AwaitResponse});
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
