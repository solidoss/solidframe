#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include "solid/utility/threadpool.hpp"

#include "mprpc_request_messages.hpp"

#include <iostream>

using namespace solid;
using namespace std;

using AioSchedulerT = frame::Scheduler<frame::aio::Reactor>;
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
namespace rpc_request_client {

template <class M>
void complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<M>&              _rsent_msg_ptr,
    std::shared_ptr<M>&              _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    solid_check(false); // this method should not be called
}

} // namespace rpc_request_client

namespace {
ostream& operator<<(ostream& _ros, const rpc_request::Date& _rd)
{
    _ros << _rd.year << '.' << (int)_rd.month << '.' << (int)_rd.day;
    return _ros;
}
ostream& operator<<(ostream& _ros, const rpc_request::UserData& _rud)
{
    _ros << _rud.full_name << ", " << _rud.email << ", " << _rud.country << ", " << _rud.city << ", " << _rud.birth_date;
    return _ros;
}
} // namespace
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
        CallPoolT              cwp{1, 100, 0, [](const size_t) {}, [](const size_t) {}};
        frame::aio::Resolver   resolver([&cwp](std::function<void()>&& _fnc) { cwp.pushOne(std::move(_fnc)); });
        ErrorConditionT        err;

        scheduler.start(1);

        {
            auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
                reflection::v1::metadata::factory,
                [&](auto& _rmap) {
                    auto lambda = [&](const uint8_t _id, const std::string_view _name, auto const& _rtype) {
                        using TypeT = typename std::decay_t<decltype(_rtype)>::TypeT;
                        _rmap.template registerMessage<TypeT>(_id, _name, rpc_request_client::complete_message<TypeT>);
                    };
                    rpc_request::configure_protocol(lambda);
                });
            frame::mprpc::Configuration cfg(scheduler, proto);

            cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(resolver, p.port.c_str());

            cfg.client.connection_start_state = frame::mprpc::ConnectionState::Active;

            rpcservice.start(std::move(cfg));

            if (err) {
                cout << "Error starting rpcservice: " << err.message() << endl;
                return 1;
            }
        }

        cout << "Expect lines like:" << endl;
        cout << "quit" << endl;
        cout << "q" << endl;
        cout << "localhost user\\d*" << endl;
        cout << "127.0.0.1 [a-z]+_man" << endl;

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
                    recipient   = line.substr(0, offset);
                    auto lambda = [](
                                      frame::mprpc::ConnectionContext&        _rctx,
                                      std::shared_ptr<rpc_request::Request>&  _rsent_msg_ptr,
                                      std::shared_ptr<rpc_request::Response>& _rrecv_msg_ptr,
                                      ErrorConditionT const&                  _rerror) {
                        if (_rerror) {
                            cout << "Error sending message to " << _rctx.recipientName() << ". Error: " << _rerror.message() << endl;
                            return;
                        }

                        solid_check(!_rerror && _rsent_msg_ptr && _rrecv_msg_ptr);

                        cout << "Received " << _rrecv_msg_ptr->user_data_map.size() << " users:" << endl;

                        for (const auto& user_data : _rrecv_msg_ptr->user_data_map) {
                            cout << '{' << user_data.first << "}: " << user_data.second << endl;
                        }
                    };

                    auto req_ptr = make_shared<rpc_request::Request>(line.substr(offset + 1));

                    rpcservice.sendRequest(
                        recipient.c_str(), req_ptr, lambda, 0);
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
