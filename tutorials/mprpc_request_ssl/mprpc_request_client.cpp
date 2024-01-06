// NOTE: on Windows, when compiling against BoringSSL
// we need to include "openssl/ssl.hpp" before any
// inclusion of Windows.h
#include "solid/frame/mprpc/mprpcsocketstub_openssl.hpp"

#include "solid/frame/mprpc/mprpccompression_snappy.hpp"
#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/utility/threadpool.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "mprpc_request_messages.hpp"
#include <fstream>

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

std::string loadFile(const char* _path)
{
    ifstream      ifs(_path);
    ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

//-----------------------------------------------------------------------------
namespace rpc_request_client {

using namespace rpc_request;

template <class M>
void complete_message(
    frame::mprpc::ConnectionContext&  _rctx,
    frame::mprpc::MessagePointerT<M>& _rsent_msg_ptr,
    frame::mprpc::MessagePointerT<M>& _rrecv_msg_ptr,
    ErrorConditionT const&            _rerror)
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

                        if constexpr (std::is_base_of_v<rpc_request::RequestKey, TypeT>) {
                            _rmap.template registerType<TypeT, rpc_request::RequestKey>(_id, _name);
                        } else {
                            _rmap.template registerMessage<TypeT>(_id, _name, rpc_request_client::complete_message<TypeT>);
                        }
                    };
                    rpc_request::configure_protocol(lambda);
                });
            frame::mprpc::Configuration cfg(scheduler, proto);

            cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(resolver, p.port.c_str());

            cfg.client.connection_start_state = frame::mprpc::ConnectionState::Active;

            frame::mprpc::openssl::setup_client(
                cfg,
                [](frame::aio::openssl::Context& _rctx) -> ErrorCodeT {
                    _rctx.addVerifyAuthority(loadFile("echo-ca-cert.pem"));
                    _rctx.loadCertificate(loadFile("echo-client-cert.pem"));
                    _rctx.loadPrivateKey(loadFile("echo-client-key.pem"));
                    return ErrorCodeT();
                },
                frame::mprpc::openssl::NameCheckSecureStart{"echo-server"});

            frame::mprpc::snappy::setup(cfg);

            rpcservice.start(std::move(cfg));
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
                    recipient = line.substr(0, offset);

                    auto lambda = [](
                                      frame::mprpc::ConnectionContext&                      _rctx,
                                      frame::mprpc::MessagePointerT<rpc_request::Request>&  _rsent_msg_ptr,
                                      frame::mprpc::MessagePointerT<rpc_request::Response>& _rrecv_msg_ptr,
                                      ErrorConditionT const&                                _rerror) {
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

                    auto req_ptr = frame::mprpc::make_message<rpc_request::Request>(
                        make_shared<rpc_request::RequestKeyAndList>(
                            make_shared<rpc_request::RequestKeyOr>(
                                make_shared<rpc_request::RequestKeyUserIdRegex>(line.substr(offset + 1)),
                                make_shared<rpc_request::RequestKeyEmailRegex>(line.substr(offset + 1))),
                            make_shared<rpc_request::RequestKeyOr>(
                                make_shared<rpc_request::RequestKeyYearLess>(2000),
                                make_shared<rpc_request::RequestKeyYearLess>(2003))));

                    cout << "Request key: ";
                    if (req_ptr->key)
                        req_ptr->key->print(cout);
                    cout << endl;

                    rpcservice.sendRequest(
                        recipient.c_str(), // frame::mprpc::make_message<rpc_request::Request>(line.substr(offset + 1)),
                        req_ptr, lambda, 0);
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
