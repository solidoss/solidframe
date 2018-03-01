#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipcservice.hpp"

#include <boost/filesystem/exception.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/utility.hpp>

#include "mpipc_file_messages.hpp"

#include <deque>
#include <iostream>
#include <regex>
#include <string>

using namespace solid;
using namespace std;

namespace fs = boost::filesystem;

namespace {

using AioSchedulerT = frame::Scheduler<frame::aio::Reactor>;

//-----------------------------------------------------------------------------
//      Parameters
//-----------------------------------------------------------------------------
struct Parameters {
    Parameters()
        : listener_port("0")
        , listener_addr("0.0.0.0")
    {
    }

    string listener_port;
    string listener_addr;
};
//-----------------------------------------------------------------------------

} //namespace

namespace ipc_file_server {

template <class M>
void complete_message(
    frame::mpipc::ConnectionContext& _rctx,
    std::shared_ptr<M>&              _rsent_msg_ptr,
    std::shared_ptr<M>&              _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror);

template <>
void complete_message<ipc_file::ListRequest>(
    frame::mpipc::ConnectionContext&        _rctx,
    std::shared_ptr<ipc_file::ListRequest>& _rsent_msg_ptr,
    std::shared_ptr<ipc_file::ListRequest>& _rrecv_msg_ptr,
    ErrorConditionT const&                  _rerror)
{
    SOLID_CHECK(not _rerror);
    SOLID_CHECK(_rrecv_msg_ptr);
    SOLID_CHECK(not _rsent_msg_ptr);

    auto msgptr = std::make_shared<ipc_file::ListResponse>(*_rrecv_msg_ptr);

    fs::path fs_path(_rrecv_msg_ptr->path.c_str() /*, fs::native*/);

    if (fs::exists(fs_path) and fs::is_directory(fs_path)) {
        fs::directory_iterator it, end;

        try {
            it = fs::directory_iterator(fs_path);
        } catch (const std::exception& ex) {
            it = end;
        }

        while (it != end) {
            msgptr->node_dq.emplace_back(std::string(it->path().c_str()), static_cast<uint8_t>(is_directory(*it)));
            ++it;
        }
    }
    SOLID_CHECK(!_rctx.service().sendResponse(_rctx.recipientId(), std::move(msgptr)));
}

template <>
void complete_message<ipc_file::ListResponse>(
    frame::mpipc::ConnectionContext&         _rctx,
    std::shared_ptr<ipc_file::ListResponse>& _rsent_msg_ptr,
    std::shared_ptr<ipc_file::ListResponse>& _rrecv_msg_ptr,
    ErrorConditionT const&                   _rerror)
{
    SOLID_CHECK(not _rerror);
    SOLID_CHECK(not _rrecv_msg_ptr);
    SOLID_CHECK(_rsent_msg_ptr);
}

template <>
void complete_message<ipc_file::FileRequest>(
    frame::mpipc::ConnectionContext&        _rctx,
    std::shared_ptr<ipc_file::FileRequest>& _rsent_msg_ptr,
    std::shared_ptr<ipc_file::FileRequest>& _rrecv_msg_ptr,
    ErrorConditionT const&                  _rerror)
{
    SOLID_CHECK(not _rerror);
    SOLID_CHECK(_rrecv_msg_ptr);
    SOLID_CHECK(not _rsent_msg_ptr);

    auto msgptr = std::make_shared<ipc_file::FileResponse>(*_rrecv_msg_ptr);

    if (0) {
        boost::system::error_code error;

        msgptr->remote_file_size = fs::file_size(fs::path(_rrecv_msg_ptr->remote_path), error);
    }

    SOLID_CHECK(!_rctx.service().sendResponse(_rctx.recipientId(), std::move(msgptr)));
}

template <>
void complete_message<ipc_file::FileResponse>(
    frame::mpipc::ConnectionContext&         _rctx,
    std::shared_ptr<ipc_file::FileResponse>& _rsent_msg_ptr,
    std::shared_ptr<ipc_file::FileResponse>& _rrecv_msg_ptr,
    ErrorConditionT const&                   _rerror)
{
    SOLID_CHECK(not _rerror);
    SOLID_CHECK(not _rrecv_msg_ptr);
    SOLID_CHECK(_rsent_msg_ptr);
}

struct MessageSetup {
    template <class T>
    void operator()(ipc_file::ProtocolT& _rprotocol, TypeToType<T> _t2t, const ipc_file::ProtocolT::TypeIdT& _rtid)
    {
        _rprotocol.registerMessage<T>(complete_message<T>, _rtid);
    }
};

} // namespace ipc_file_server

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
        ErrorConditionT        err;

        err = scheduler.start(1);

        if (err) {
            cout << "Error starting aio scheduler: " << err.message() << endl;
            return 1;
        }

        {
            auto                        proto = ipc_file::ProtocolT::create();
            frame::mpipc::Configuration cfg(scheduler, proto);

            ipc_file::protocol_setup(ipc_file_server::MessageSetup(),*proto);

            cfg.server.listener_address_str = p.listener_addr;
            cfg.server.listener_address_str += ':';
            cfg.server.listener_address_str += p.listener_port;

            cfg.server.connection_start_state = frame::mpipc::ConnectionState::Active;

            err = ipcservice.reconfigure(std::move(cfg));

            if (err) {
                cout << "Error starting ipcservice: " << err.message() << endl;
                manager.stop();
                return 1;
            }
            {
                std::ostringstream oss;
                oss << ipcservice.configuration().server.listenerPort();
                cout << "server listens on port: " << oss.str() << endl;
            }
        }

        cout << "Press ENTER to stop: ";
        cin.ignore();
    }
    return 0;
}

//-----------------------------------------------------------------------------

bool parseArguments(Parameters& _par, int argc, char* argv[])
{
    if (argc == 2) {
        size_t pos;

        _par.listener_addr = argv[1];

        pos = _par.listener_addr.rfind(':');

        if (pos != string::npos) {
            _par.listener_addr[pos] = '\0';

            _par.listener_port.assign(_par.listener_addr.c_str() + pos + 1);

            _par.listener_addr.resize(pos);
        }
    }
    return true;
}
