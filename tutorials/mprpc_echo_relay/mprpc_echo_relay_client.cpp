#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include "mprpc_echo_relay_register.hpp"

#include "solid/utility/threadpool.hpp"
#include <iostream>

using namespace solid;
using namespace std;

using AioSchedulerT = frame::Scheduler<frame::aio::Reactor<frame::mprpc::EventT>>;
using CallPoolT     = ThreadPool<Function<void()>, Function<void()>>;

//-----------------------------------------------------------------------------
//      Parameters
//-----------------------------------------------------------------------------
struct Parameters {
    Parameters()
        : server_port("3333")
        , server_addr("127.0.0.1")
    {
    }
    uint32_t group_id;
    string   server_port;
    string   server_addr;
};

struct Message : solid::frame::mprpc::Message {
    uint32_t    group_id_ = -1;
    std::string data;

    Message() {}

    Message(uint32_t _group_id, std::string&& _ustr)
        : group_id_(_group_id)
        , data(std::move(_ustr))
    {
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.group_id_, _rctx, 1, "group_id");
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
        AioSchedulerT          scheduler;
        frame::Manager         manager;
        frame::mprpc::ServiceT rpcservice(manager);
        ErrorConditionT        err;
        CallPoolT              cwp{{1, 100, 0}, [](const size_t) {}, [](const size_t) {}};
        frame::aio::Resolver   resolver([&cwp](std::function<void()>&& _fnc) { cwp.pushOne(std::move(_fnc)); });

        scheduler.start(1);

        {
            auto con_register = [](
                                    frame::mprpc::ConnectionContext&         _rctx,
                                    frame::mprpc::MessagePointerT<Register>& _rsent_msg_ptr,
                                    frame::mprpc::MessagePointerT<Register>& _rrecv_msg_ptr,
                                    ErrorConditionT const&                   _rerror) {
                solid_check(!_rerror);

                if (_rrecv_msg_ptr) {
                    auto lambda = [](frame::mprpc::ConnectionContext&, ErrorConditionT const& _rerror) {
                        solid_log(generic_logger, Info, "peerb --- enter active error: " << _rerror.message());
                    };
                    cout << "Connection registered" << endl;
                    _rctx.service().connectionNotifyEnterActiveState(_rctx.recipientId(), lambda);
                }
            };
            auto on_message = [&p](
                                  frame::mprpc::ConnectionContext&        _rctx,
                                  frame::mprpc::MessagePointerT<Message>& _rsent_msg_ptr,
                                  frame::mprpc::MessagePointerT<Message>& _rrecv_msg_ptr,
                                  ErrorConditionT const&                  _rerror) {
                if (_rrecv_msg_ptr) {
                    cout << _rrecv_msg_ptr->group_id_ << ": " << _rrecv_msg_ptr->data << endl;
                    if (!_rsent_msg_ptr) {
                        // we're on peer - echo back the response
                        _rrecv_msg_ptr->group_id_ = p.group_id;
                        ErrorConditionT err       = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

                        (void)err;
                    }
                }
            };
            auto on_connection_start = [&p](frame::mprpc::ConnectionContext& _rctx) {
                solid_log(generic_logger, Info, _rctx.recipientId());

                auto            msgptr = frame::mprpc::make_message<Register>(p.group_id);
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

        rpcservice.createConnectionPool({p.server_addr});

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
                    rpcservice.sendMessage({recipient}, frame::mprpc::make_message<Message>(p.group_id, line.substr(offset + 1)), {frame::mprpc::MessageFlagsE::AwaitResponse});
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
        _par.group_id = stoul(argv[1]);
        return true;
    }
    if (argc == 3) {
        size_t pos;

        _par.group_id = stoul(argv[1]);

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
         << argv[0] << " my_group_id [server_addr:server_port]" << endl;
    return false;
}
