#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include "solid/utility/threadpool.hpp"

#include "mprpc_echo_messages.hpp"

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
        : port("3333")
    {
    }

    string port;
};

//-----------------------------------------------------------------------------
namespace rpc_echo_client {

template <class M>
void complete_message(
    frame::mprpc::ConnectionContext&  _rctx,
    frame::mprpc::MessagePointerT<M>& _rsent_msg_ptr,
    frame::mprpc::MessagePointerT<M>& _rrecv_msg_ptr,
    ErrorConditionT const&            _rerror)
{
    if (_rerror) {
        cout << "Error sending message to " << _rctx.recipientName() << ". Error: " << _rerror.message() << endl;
        return;
    }

    solid_check(_rrecv_msg_ptr && _rsent_msg_ptr);

    cout << "Received from " << _rctx.recipientName() << ": " << _rrecv_msg_ptr->str << endl;
}

} // namespace rpc_echo_client

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
        CallPoolT              cwp{{1, 100, 0}, [](const size_t) {}, [](const size_t) {}};
        frame::aio::Resolver   resolver([&cwp](std::function<void()>&& _fnc) { cwp.pushOne(std::move(_fnc)); });
        ErrorConditionT        err;

        scheduler.start(1);

        {
            auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
                reflection::v1::metadata::factory,
                [&](auto& _rmap) {
                    auto lambda = [&]<typename T>(const uint8_t _id, const std::string_view _name, type_identity<T> const& _rtype) {
                        _rmap.template registerMessage<T>(_id, _name, rpc_echo_client::complete_message<T>);
                    };
                    rpc_echo::configure_protocol(lambda);
                });
            frame::mprpc::Configuration cfg(scheduler, proto);

            cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(resolver, p.port.c_str());

            cfg.client.connection_start_state = frame::mprpc::ConnectionState::Active;

            rpcservice.start(std::move(cfg));
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
                    rpcservice.sendMessage({recipient}, frame::mprpc::make_message<rpc_echo::Message>(line.substr(offset + 1)), {frame::mprpc::MessageFlagsE::AwaitResponse});
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
