#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include "mprpc_file_messages.hpp"

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
namespace ipc_file_client {

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
    template <class T>
    void operator()(ipc_file::ProtocolT& _rprotocol, TypeToType<T> _t2t, const ipc_file::ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerMessage<T>(complete_message<T>, _rtid);
    }
};

} // namespace ipc_file_client

namespace {
streampos file_size(const std::string& _path)
{
    std::ifstream ifs(_path);
    if (ifs) {
        ifs.seekg(0, ifs.end);
        streampos endpos = ifs.tellg();
        return endpos;
    } else {
        return -1;
    }
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
        CallPool<void()>       cwp{WorkPoolConfiguration(), 1};
        frame::aio::Resolver   resolver(cwp);
        ErrorConditionT        err;

        scheduler.start(1);

        {
            auto                        proto = ipc_file::ProtocolT::create();
            frame::mprpc::Configuration cfg(scheduler, proto);

            ipc_file::protocol_setup(ipc_file_client::MessageSetup(), *proto);

            cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(resolver, p.port.c_str());

            cfg.client.connection_start_state = frame::mprpc::ConnectionState::Active;

            ipcservice.start(std::move(cfg));
        }

        cout << "Expect lines like:" << endl;
        cout << "quit" << endl;
        cout << "q" << endl;
        cout << "localhost l /home" << endl;
        cout << "localhost L /home/remote_user" << endl;
        cout << "localhost C /home/remote_user/remote_file ./local_file" << endl;
        cout << "localhost c /home/remote_user/remote_file /home/local_user/local_file" << endl;

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

                    std::istringstream iss(line.substr(offset + 1));

                    char choice;

                    iss >> choice;

                    switch (choice) {
                    case 'l':
                    case 'L': {

                        std::string path;
                        iss >> path;

                        ipcservice.sendRequest(
                            recipient.c_str(), make_shared<ipc_file::ListRequest>(std::move(path)),
                            [](
                                frame::mprpc::ConnectionContext&         _rctx,
                                std::shared_ptr<ipc_file::ListRequest>&  _rsent_msg_ptr,
                                std::shared_ptr<ipc_file::ListResponse>& _rrecv_msg_ptr,
                                ErrorConditionT const&                   _rerror) {
                                if (_rerror) {
                                    cout << "Error sending message to " << _rctx.recipientName() << ". Error: " << _rerror.message() << endl;
                                    return;
                                }

                                solid_check(!_rerror && _rsent_msg_ptr && _rrecv_msg_ptr);

                                cout << "File List from " << _rctx.recipientName() << ":" << _rsent_msg_ptr->path << " with " << _rrecv_msg_ptr->node_dq.size() << " items: {" << endl;

                                for (auto it : _rrecv_msg_ptr->node_dq) {
                                    cout << (it.second ? 'D' : 'F') << ": " << it.first << endl;
                                }
                                cout << "}" << endl;
                            },
                            0);

                        break;
                    }

                    case 'c':
                    case 'C': {
                        std::string remote_path, local_path;

                        iss >> remote_path >> local_path;

                        ipcservice.sendRequest(
                            recipient.c_str(), make_shared<ipc_file::FileRequest>(std::move(remote_path), std::move(local_path)),
                            [](
                                frame::mprpc::ConnectionContext&         _rctx,
                                std::shared_ptr<ipc_file::FileRequest>&  _rsent_msg_ptr,
                                std::shared_ptr<ipc_file::FileResponse>& _rrecv_msg_ptr,
                                ErrorConditionT const&                   _rerror) {
                                if (_rerror) {
                                    cout << "Error sending message to " << _rctx.recipientName() << ". Error: " << _rerror.message() << endl;
                                    return;
                                }

                                solid_check(!_rerror && _rsent_msg_ptr && _rrecv_msg_ptr);

                                cout << "Done copy from " << _rctx.recipientName() << ":" << _rsent_msg_ptr->remote_path << " to " << _rsent_msg_ptr->local_path << ": ";
                                int64_t local_file_size = file_size(_rsent_msg_ptr->local_path);
                                if (_rrecv_msg_ptr->remote_file_size != InvalidSize() && _rrecv_msg_ptr->remote_file_size == local_file_size) {
                                    cout << "Success(" << _rrecv_msg_ptr->remote_file_size << ")" << endl;
                                } else if (_rrecv_msg_ptr->remote_file_size == InvalidSize()) {
                                    cout << "Fail(no remote)" << endl;
                                } else {
                                    cout << "Fail(" << local_file_size << " instead of " << _rrecv_msg_ptr->remote_file_size << ")" << endl;
                                }
                            },
                            0);
                        break;
                    }
                    default:
                        cout << "Unknown choice" << endl;
                        break;
                    }

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
