//NOTE: on Windows, when compiling against BoringSSL
//we need to include "openssl/ssl.hpp" before any
//inclusion of Windows.h
#include "solid/frame/mprpc/mprpcsocketstub_openssl.hpp"

#include "solid/frame/mprpc/mprpccompression_snappy.hpp"
#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "mprpc_request_messages.hpp"
#include <fstream>

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

std::string loadFile(const char* _path)
{
    ifstream      ifs(_path);
    ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

//-----------------------------------------------------------------------------
namespace ipc_request_client {

using namespace ipc_request;

template <class M>
void complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<M>&              _rsent_msg_ptr,
    std::shared_ptr<M>&              _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    solid_check(false); //this method should not be called
}

struct MessageSetup {

    void operator()(ipc_request::ProtocolT& _rprotocol, TypeToType<RequestKeyAnd> _t2t, const ipc_request::ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerType<RequestKeyAnd>(_rtid);
    }

    void operator()(ipc_request::ProtocolT& _rprotocol, TypeToType<RequestKeyOr> _t2t, const ipc_request::ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerType<RequestKeyOr>(_rtid);
    }

    void operator()(ipc_request::ProtocolT& _rprotocol, TypeToType<RequestKeyAndList> _t2t, const ipc_request::ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerType<RequestKeyAndList>(_rtid);
    }

    void operator()(ipc_request::ProtocolT& _rprotocol, TypeToType<RequestKeyOrList> _t2t, const ipc_request::ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerType<RequestKeyOrList>(_rtid);
    }

    void operator()(ipc_request::ProtocolT& _rprotocol, TypeToType<RequestKeyUserIdRegex> _t2t, const ipc_request::ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerType<RequestKeyUserIdRegex>(_rtid);
    }

    void operator()(ipc_request::ProtocolT& _rprotocol, TypeToType<RequestKeyEmailRegex> _t2t, const ipc_request::ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerType<RequestKeyEmailRegex>(_rtid);
    }

    void operator()(ipc_request::ProtocolT& _rprotocol, TypeToType<RequestKeyYearLess> _t2t, const ipc_request::ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerType<RequestKeyYearLess>(_rtid);
    }

    template <class T>
    void operator()(ipc_request::ProtocolT& _rprotocol, TypeToType<T> _t2t, const ipc_request::ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerMessage<T>(complete_message<T>, _rtid);
    }
};

} // namespace ipc_request_client

namespace {
ostream& operator<<(ostream& _ros, const ipc_request::Date& _rd)
{
    _ros << _rd.year << '.' << (int)_rd.month << '.' << (int)_rd.day;
    return _ros;
}
ostream& operator<<(ostream& _ros, const ipc_request::UserData& _rud)
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
        frame::mprpc::ServiceT ipcservice(manager);
        FunctionWorkPool       fwp{WorkPoolConfiguration()};
        frame::aio::Resolver   resolver(fwp);
        ErrorConditionT        err;

        scheduler.start(1);

        {
            auto                        proto = ipc_request::ProtocolT::create();
            frame::mprpc::Configuration cfg(scheduler, proto);

            ipc_request::protocol_setup(ipc_request_client::MessageSetup(), *proto);

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

            ipcservice.start(std::move(cfg));
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
                                      frame::mprpc::ConnectionContext&        _rctx,
                                      std::shared_ptr<ipc_request::Request>&  _rsent_msg_ptr,
                                      std::shared_ptr<ipc_request::Response>& _rrecv_msg_ptr,
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

                    auto req_ptr = make_shared<ipc_request::Request>(
                        make_shared<ipc_request::RequestKeyAndList>(
                            make_shared<ipc_request::RequestKeyOr>(
                                make_shared<ipc_request::RequestKeyUserIdRegex>(line.substr(offset + 1)),
                                make_shared<ipc_request::RequestKeyEmailRegex>(line.substr(offset + 1))),
                            make_shared<ipc_request::RequestKeyOr>(
                                make_shared<ipc_request::RequestKeyYearLess>(2000),
                                make_shared<ipc_request::RequestKeyYearLess>(2003))));

                    cout << "Request key: ";
                    if (req_ptr->key)
                        req_ptr->key->print(cout);
                    cout << endl;

                    ipcservice.sendRequest(
                        recipient.c_str(), //make_shared<ipc_request::Request>(line.substr(offset + 1)),
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
