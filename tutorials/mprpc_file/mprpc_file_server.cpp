#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include <filesystem>

#include "mprpc_file_messages.hpp"

#include <deque>
#include <iostream>
#include <regex>
#include <string>

using namespace solid;
using namespace std;

namespace fs = std::filesystem;

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

} // namespace

namespace rpc_file_server {

template <class M>
void complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<M>&              _rsent_msg_ptr,
    std::shared_ptr<M>&              _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror);

template <>
void complete_message<rpc_file::ListRequest>(
    frame::mprpc::ConnectionContext&        _rctx,
    std::shared_ptr<rpc_file::ListRequest>& _rsent_msg_ptr,
    std::shared_ptr<rpc_file::ListRequest>& _rrecv_msg_ptr,
    ErrorConditionT const&                  _rerror)
{
    solid_check(!_rerror);
    solid_check(_rrecv_msg_ptr);
    solid_check(!_rsent_msg_ptr);

    auto msgptr = std::make_shared<rpc_file::ListResponse>(*_rrecv_msg_ptr);

    fs::path fs_path(_rrecv_msg_ptr->path.c_str() /*, fs::native*/);

    if (fs::exists(fs_path) && fs::is_directory(fs_path)) {
        fs::directory_iterator it, end;

        try {
            it = fs::directory_iterator(fs_path);
        } catch (const std::exception& ex) {
            it = end;
        }

        while (it != end) {
            msgptr->node_dq.emplace_back(std::string(it->path().generic_string()), static_cast<uint8_t>(is_directory(*it)));
            ++it;
        }
    }
    solid_check(!_rctx.service().sendResponse(_rctx.recipientId(), std::move(msgptr)));
}

template <>
void complete_message<rpc_file::ListResponse>(
    frame::mprpc::ConnectionContext&         _rctx,
    std::shared_ptr<rpc_file::ListResponse>& _rsent_msg_ptr,
    std::shared_ptr<rpc_file::ListResponse>& _rrecv_msg_ptr,
    ErrorConditionT const&                   _rerror)
{
    solid_check(!_rerror);
    solid_check(!_rrecv_msg_ptr);
    solid_check(_rsent_msg_ptr);
}

template <>
void complete_message<rpc_file::FileRequest>(
    frame::mprpc::ConnectionContext&        _rctx,
    std::shared_ptr<rpc_file::FileRequest>& _rsent_msg_ptr,
    std::shared_ptr<rpc_file::FileRequest>& _rrecv_msg_ptr,
    ErrorConditionT const&                  _rerror)
{
    solid_check(!_rerror);
    solid_check(_rrecv_msg_ptr);
    solid_check(!_rsent_msg_ptr);

    auto msgptr = std::make_shared<rpc_file::FileResponse>(*_rrecv_msg_ptr);

    if ((false)) {
        error_code error;

        msgptr->remote_file_size = fs::file_size(fs::path(_rrecv_msg_ptr->remote_path), error);
    }

    solid_check(!_rctx.service().sendResponse(_rctx.recipientId(), std::move(msgptr)));
}

template <>
void complete_message<rpc_file::FileResponse>(
    frame::mprpc::ConnectionContext&         _rctx,
    std::shared_ptr<rpc_file::FileResponse>& _rsent_msg_ptr,
    std::shared_ptr<rpc_file::FileResponse>& _rrecv_msg_ptr,
    ErrorConditionT const&                   _rerror)
{
    solid_check(!_rerror);
    solid_check(!_rrecv_msg_ptr);
    solid_check(_rsent_msg_ptr);
}

} // namespace rpc_file_server

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

        scheduler.start(1);

        {
            auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
                reflection::v1::metadata::factory,
                [&](auto& _rmap) {
                    auto lambda = [&](const uint8_t _id, const std::string_view _name, auto const& _rtype) {
                        using TypeT = typename std::decay_t<decltype(_rtype)>::TypeT;
                        _rmap.template registerMessage<TypeT>(_id, _name, rpc_file_server::complete_message<TypeT>);
                    };
                    rpc_file::configure_protocol(lambda);
                });
            frame::mprpc::Configuration cfg(scheduler, proto);

            cfg.server.listener_address_str = p.listener_addr;
            cfg.server.listener_address_str += ':';
            cfg.server.listener_address_str += p.listener_port;

            cfg.server.connection_start_state = frame::mprpc::ConnectionState::Active;

            {
                frame::mprpc::ServiceStartStatus start_status;
                rpcservice.start(start_status, std::move(cfg));

                solid_dbg(generic_logger, Info, "server listens on: " << start_status.listen_addr_vec_.back());
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
