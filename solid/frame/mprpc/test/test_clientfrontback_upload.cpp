#include "solid/frame/mprpc/mprpcsocketstub_openssl.hpp"

#include "solid/frame/mprpc/mprpccompression_snappy.hpp"
#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v2.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioresolver.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/utility/string.hpp"

#include "solid/system/directory.hpp"
#include "solid/system/exception.hpp"

#include "solid/system/log.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;
using namespace solid;

using AioSchedulerT  = frame::Scheduler<frame::aio::Reactor>;
using SecureContextT = frame::aio::openssl::Context;
using ProtocolT      = frame::mprpc::serialization_v2::Protocol<uint8_t>;

namespace {
LoggerT logger("test");

frame::mprpc::ServiceT* pmprpc_back_client  = nullptr;
frame::mprpc::ServiceT* pmprpc_front_server = nullptr;

namespace back {
struct UploadRequest;
} //namespace back

namespace front {

struct UploadRequest : frame::mprpc::Message {
    string        name_;
    ifstream      ifs_;
    ostringstream oss_;

    UploadRequest()
    {
    }

    ~UploadRequest() override
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, /*_name*/)
    {
        if (_s.is_serializer) {
        }
    }
};

struct UploadResponse : frame::mprpc::Message {
    uint32_t                             error_;
    solid::frame::mprpc::RecipientId     recipient_id_;
    std::shared_ptr<back::UploadRequest> req_ptr_;

    UploadResponse()
    {
    }

    UploadResponse(const UploadRequest& _req)
        : frame::mprpc::Message(_req)
    {
    }

    ~UploadResponse() override
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, /*_name*/)
    {
        _s.add(_rthis.error_, _rctx, "error");
    }
};

void on_server_request(
    frame::mprpc::ConnectionContext&       _rctx,
    std::shared_ptr<front::UploadRequest>& _rsent_msg_ptr,
    std::shared_ptr<front::UploadRequest>& _rrecv_msg_ptr,
    ErrorConditionT const&                 _rerror);

void on_server_response(
    frame::mprpc::ConnectionContext&        _rctx,
    std::shared_ptr<front::UploadResponse>& _rsent_msg_ptr,
    std::shared_ptr<front::UploadResponse>& _rrecv_msg_ptr,
    ErrorConditionT const&                  _rerror)
{
}

void on_client_requset(
    frame::mprpc::ConnectionContext&       _rctx,
    std::shared_ptr<front::UploadRequest>& _rsent_msg_ptr,
    std::shared_ptr<front::UploadRequest>& _rrecv_msg_ptr,
    ErrorConditionT const&                 _rerror)
{
}

void on_client_response(
    frame::mprpc::ConnectionContext&        _rctx,
    std::shared_ptr<front::UploadResponse>& _rsent_msg_ptr,
    std::shared_ptr<front::UploadResponse>& _rrecv_msg_ptr,
    ErrorConditionT const&                  _rerror)
{
}

void on_client_upload_response(
    frame::mprpc::ConnectionContext&        _rctx,
    std::shared_ptr<front::UploadRequest>&  _rsent_msg_ptr,
    std::shared_ptr<front::UploadResponse>& _rrecv_msg_ptr,
    ErrorConditionT const&                  _rerror);

} //namespace front

namespace back {

struct UploadRequest : frame::mprpc::Message {
    string                                 name_;
    ofstream                               ofs_;
    istringstream                          iss_;
    solid::frame::mprpc::RecipientId       recipient_id_;
    std::shared_ptr<front::UploadResponse> res_ptr_;

    UploadRequest()
    {
    }

    ~UploadRequest() override
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, /*_name*/)
    {
        if (_s.is_serializer) {
        }
    }
};

struct UploadResponse : frame::mprpc::Message {
    uint32_t error_;

    UploadResponse()
    {
    }

    ~UploadResponse() override
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, /*_name*/)
    {
        _s.add(_rthis.error_, _rctx, "error");
    }
};

void on_server_request(
    frame::mprpc::ConnectionContext&       _rctx,
    std::shared_ptr<front::UploadRequest>& _rsent_msg_ptr,
    std::shared_ptr<front::UploadRequest>& _rrecv_msg_ptr,
    ErrorConditionT const&                 _rerror);

void on_server_response(
    frame::mprpc::ConnectionContext&        _rctx,
    std::shared_ptr<front::UploadResponse>& _rsent_msg_ptr,
    std::shared_ptr<front::UploadResponse>& _rrecv_msg_ptr,
    ErrorConditionT const&                  _rerror)
{
}

void on_client_requset(
    frame::mprpc::ConnectionContext&       _rctx,
    std::shared_ptr<front::UploadRequest>& _rsent_msg_ptr,
    std::shared_ptr<front::UploadRequest>& _rrecv_msg_ptr,
    ErrorConditionT const&                 _rerror)
{
}

void on_client_response(
    frame::mprpc::ConnectionContext&        _rctx,
    std::shared_ptr<front::UploadResponse>& _rsent_msg_ptr,
    std::shared_ptr<front::UploadResponse>& _rrecv_msg_ptr,
    ErrorConditionT const&                  _rerror)
{
}

} //namespace back

void create_files(vector<string>& _file_vec, const char* _path_prefix, uint64_t _count, uint64_t _start_size, uint64_t _increment_size);

void on_uploading(
    frame::mprpc::ConnectionContext&        _rctx,
    std::shared_ptr<front::UploadRequest>&  _rsent_msg_ptr,
    std::shared_ptr<front::UploadResponse>& _rrecv_msg_ptr,
    ErrorConditionT const&                  _rerror);
} //namespace

int test_clientfrontback_upload(int argc, char* argv[])
{
    solid::log_start(std::cerr, {".*:EW", "test:VIEW"});

    size_t   max_per_pool_connection_count = 1;
    bool     secure                        = false;
    bool     compress                      = false;
    uint64_t count                         = 3;
    uint64_t start_size                    = make_number("10M");
    uint64_t increment_size                = make_number("10M");

    if (argc > 1) {
        max_per_pool_connection_count = atoi(argv[1]);
        if (max_per_pool_connection_count == 0) {
            max_per_pool_connection_count = 1;
        }
        if (max_per_pool_connection_count > 100) {
            max_per_pool_connection_count = 100;
        }
    }

    if (argc > 2) {
        if (*argv[2] == 's' || *argv[2] == 'S') {
            secure = true;
        }
        if (*argv[2] == 'c' || *argv[2] == 'C') {
            compress = true;
        }
        if (*argv[2] == 'b' || *argv[2] == 'B') {
            secure   = true;
            compress = true;
        }
    }

    if (argc > 3) {
        count = atoi(argv[1]);
    }
    if (argc > 4) {
        start_size = make_number(argv[2]);
    }
    if (argc > 5) {
        increment_size = make_number(argv[3]);
    }

    system("rm -rf client_storage");
    system("rm -rf back_storage");

    Directory::create("client_storage");

    vector<string> file_vec;
    create_files(file_vec, "client_storage", count, start_size, increment_size);
    solid_log(logger, Info, "Done creating files");

    {
        AioSchedulerT          sch_client;
        AioSchedulerT          sch_front;
        AioSchedulerT          sch_back;
        frame::Manager         m;
        frame::mprpc::ServiceT mprpc_front_client(m);
        frame::mprpc::ServiceT mprpc_front_server(m);
        frame::mprpc::ServiceT mprpc_back_client(m);
        frame::mprpc::ServiceT mprpc_back_server(m);
        ErrorConditionT        err;
        FunctionWorkPool       fwp{WorkPoolConfiguration()};
        frame::aio::Resolver   resolver(fwp);

        err = sch_client.start(1);

        solid_check(!err, "starting aio client scheduler: " << err.message());

        err = sch_front.start(1);

        solid_check(!err, "starting aio front scheduler: " << err.message());

        err = sch_back.start(1);

        solid_check(!err, "starting aio back scheduler: " << err.message());

        std::string back_port;
        std::string front_port;

        { //mprpc back_server initialization
            auto                        proto = ProtocolT::create();
            frame::mprpc::Configuration cfg(sch_back, proto);

            proto->null(0);
            proto->registerMessage<back::UploadRequest>(back::on_server_request, 1);
            proto->registerMessage<back::UploadResponse>(back::on_server_response, 2);

            //cfg.recv_buffer_capacity = 1024;
            //cfg.send_buffer_capacity = 1024;

            cfg.server.listener_address_str   = "0.0.0.0:0";
            cfg.server.connection_start_state = frame::mprpc::ConnectionState::Active;

            if (secure) {
                solid_dbg(generic_logger, Info, "Configure SSL server -------------------------------------");
                frame::mprpc::openssl::setup_server(
                    cfg,
                    [](frame::aio::openssl::Context& _rctx) -> ErrorCodeT {
                        _rctx.loadVerifyFile("echo-ca-cert.pem" /*"/etc/pki/tls/certs/ca-bundle.crt"*/);
                        _rctx.loadCertificateFile("echo-server-cert.pem");
                        _rctx.loadPrivateKeyFile("echo-server-key.pem");
                        return ErrorCodeT();
                    },
                    frame::mprpc::openssl::NameCheckSecureStart{"echo-client"});
            }

            if (compress) {
                frame::mprpc::snappy::setup(cfg);
            }

            err = mprpc_back_server.reconfigure(std::move(cfg));

            solid_check(!err, "starting back_server mprpcservice: " << err.message());

            {
                std::ostringstream oss;
                oss << mprpc_back_server.configuration().server.listenerPort();
                back_port = oss.str();
                solid_dbg(logger, Verbose, "back listens on port: " << back_port);
            }
        }

        { //mprpc back_client initialization
            auto                        proto = ProtocolT::create();
            frame::mprpc::Configuration cfg(sch_front, proto);

            proto->null(0);
            proto->registerMessage<back::UploadRequest>(back::on_client_requset, 1);
            proto->registerMessage<back::UploadResponse>(back::on_client_response, 2);

            cfg.pool_max_active_connection_count = max_per_pool_connection_count;

            cfg.client.name_resolve_fnc       = frame::mprpc::InternetResolverF(resolver, back_port.c_str() /*, SocketInfo::Inet4*/);
            cfg.client.connection_start_state = frame::mprpc::ConnectionState::Active;

            if (secure) {
                solid_dbg(generic_logger, Info, "Configure SSL client ------------------------------------");
                frame::mprpc::openssl::setup_client(
                    cfg,
                    [](frame::aio::openssl::Context& _rctx) -> ErrorCodeT {
                        _rctx.loadVerifyFile("echo-ca-cert.pem" /*"/etc/pki/tls/certs/ca-bundle.crt"*/);
                        _rctx.loadCertificateFile("echo-client-cert.pem");
                        _rctx.loadPrivateKeyFile("echo-client-key.pem");
                        return ErrorCodeT();
                    },
                    frame::mprpc::openssl::NameCheckSecureStart{"echo-server"});
            }

            if (compress) {
                frame::mprpc::snappy::setup(cfg);
            }

            err = mprpc_back_client.reconfigure(std::move(cfg));

            solid_check(!err, "starting back_client mprpcservice: " << err.message());
        }

        { //mprpc front_server initialization
            auto                        proto = ProtocolT::create();
            frame::mprpc::Configuration cfg(sch_back, proto);

            proto->null(0);
            proto->registerMessage<front::UploadRequest>(front::on_server_request, 1);
            proto->registerMessage<front::UploadResponse>(front::on_server_response, 2);

            //cfg.recv_buffer_capacity = 1024;
            //cfg.send_buffer_capacity = 1024;

            cfg.server.listener_address_str   = "0.0.0.0:0";
            cfg.server.connection_start_state = frame::mprpc::ConnectionState::Active;

            if (secure) {
                solid_dbg(generic_logger, Info, "Configure SSL server -------------------------------------");
                frame::mprpc::openssl::setup_server(
                    cfg,
                    [](frame::aio::openssl::Context& _rctx) -> ErrorCodeT {
                        _rctx.loadVerifyFile("echo-ca-cert.pem" /*"/etc/pki/tls/certs/ca-bundle.crt"*/);
                        _rctx.loadCertificateFile("echo-server-cert.pem");
                        _rctx.loadPrivateKeyFile("echo-server-key.pem");
                        return ErrorCodeT();
                    },
                    frame::mprpc::openssl::NameCheckSecureStart{"echo-client"});
            }

            if (compress) {
                frame::mprpc::snappy::setup(cfg);
            }

            err = mprpc_front_server.reconfigure(std::move(cfg));

            solid_check(!err, "starting back_server mprpcservice: " << err.message());

            {
                std::ostringstream oss;
                oss << mprpc_front_server.configuration().server.listenerPort();
                front_port = oss.str();
                solid_dbg(logger, Verbose, "front listens on port: " << front_port);
            }
        }

        { //mprpc front_client initialization
            auto                        proto = ProtocolT::create();
            frame::mprpc::Configuration cfg(sch_front, proto);

            proto->null(0);
            proto->registerMessage<front::UploadRequest>(front::on_client_requset, 1);
            proto->registerMessage<front::UploadResponse>(front::on_client_response, 2);

            cfg.pool_max_active_connection_count = max_per_pool_connection_count;

            cfg.client.name_resolve_fnc       = frame::mprpc::InternetResolverF(resolver, front_port.c_str() /*, SocketInfo::Inet4*/);
            cfg.client.connection_start_state = frame::mprpc::ConnectionState::Active;

            if (secure) {
                solid_dbg(generic_logger, Info, "Configure SSL client ------------------------------------");
                frame::mprpc::openssl::setup_client(
                    cfg,
                    [](frame::aio::openssl::Context& _rctx) -> ErrorCodeT {
                        _rctx.loadVerifyFile("echo-ca-cert.pem" /*"/etc/pki/tls/certs/ca-bundle.crt"*/);
                        _rctx.loadCertificateFile("echo-client-cert.pem");
                        _rctx.loadPrivateKeyFile("echo-client-key.pem");
                        return ErrorCodeT();
                    },
                    frame::mprpc::openssl::NameCheckSecureStart{"echo-server"});
            }

            if (compress) {
                frame::mprpc::snappy::setup(cfg);
            }

            err = mprpc_front_client.reconfigure(std::move(cfg));

            solid_check(!err, "starting front_client mprpcservice: " << err.message());
        }

        for (const auto& f : file_vec) {
            auto msg_ptr = make_shared<front::UploadRequest>(f);
            msg_ptr->ifs_.open(string("client_storage/") + f);

            mprpc_front_client.sendRequest("localhost", msg_ptr, front::on_client_upload_response);
        }
    }
    return 0;
}

namespace {

void create_files(vector<string>& _file_vec, const char* _path_prefix, uint64_t _count, uint64_t _start_size, uint64_t _increment_size)
{
}

namespace front {

void on_client_upload_continue(
    frame::mprpc::ConnectionContext&        _rctx,
    std::shared_ptr<front::UploadRequest>&  _rsent_msg_ptr,
    std::shared_ptr<front::UploadResponse>& _rrecv_msg_ptr,
    ErrorConditionT const&                  _rerror)
{
    solid_check(_rrecv_msg_ptr);

    solid_check(_rrecv_msg_ptr->error_ == 0);

    if (!_rsent_msg_ptr->ifs_.eof()) {
        _rctx.service().sendMessage(_rctx.recipientId(), _rsent_msg_ptr, on_client_upload_response, {frame::mprpc::MessageFlagsE::ResponsePart, frame::mprpc::MessageFlagsE::AwaitResponse});
    } else {
    }
}

void on_client_upload_response(
    frame::mprpc::ConnectionContext&        _rctx,
    std::shared_ptr<front::UploadRequest>&  _rsent_msg_ptr,
    std::shared_ptr<front::UploadResponse>& _rrecv_msg_ptr,
    ErrorConditionT const&                  _rerror)
{
    solid_check(_rrecv_msg_ptr);

    solid_check(_rrecv_msg_ptr->error_ == 0);

    if (!_rsent_msg_ptr->ifs_.eof()) {
        _rsent_msg_ptr->name_.clear();
        _rsent_msg_ptr->header(_rrecv_msg_ptr->header());
        _rctx.service().sendMessage(_rctx.recipientId(), _rsent_msg_ptr, on_client_upload_continue, {frame::mprpc::MessageFlagsE::ResponsePart});
    } else {
    }
}
//-----------------------------------------------------------------------------
// front server side
//-----------------------------------------------------------------------------
void on_back_client_response(
    frame::mprpc::ConnectionContext&       _rctx,
    std::shared_ptr<back::UploadRequest>&  _rsent_msg_ptr,
    std::shared_ptr<back::UploadResponse>& _rrecv_msg_ptr,
    ErrorConditionT const&                 _rerror);

void on_front_client_response(
    frame::mprpc::ConnectionContext&        _rctx,
    std::shared_ptr<front::UploadResponse>& _rsent_msg_ptr,
    std::shared_ptr<front::UploadRequest>&  _rrecv_msg_ptr,
    ErrorConditionT const&                  _rerror)
{
    std::shared_ptr<back::UploadRequest> req_ptr = std::move(_rsent_msg_ptr->req_ptr_);

    if (!req_ptr) {
        req_ptr                = make_shared<back::UploadRequest>();
        req_ptr->recipient_id_ = _rctx.recipientId();
        req_ptr->res_ptr_      = make_shared<front::UploadResponse>(_rrecv_msg_ptr);
    } else {
    }
    req_ptr->name_ = std::move(_rrecv_msg_ptr->name_);
    req_ptr->iss_.str(_rrecv_msg_ptr->oss_.str());

    pmprpc_back_client->sendMessage(_rsent_msg_ptr->recipient_id_, req_ptr, on_back_client_response, {frame::mprpc::MessageFlagsE::AwaitResponse});
}

void on_back_client_response(
    frame::mprpc::ConnectionContext&       _rctx,
    std::shared_ptr<back::UploadRequest>&  _rsent_msg_ptr,
    std::shared_ptr<back::UploadResponse>& _rrecv_msg_ptr,
    ErrorConditionT const&                 _rerror)
{
    std::shared_ptr<front::UploadResponse> res_ptr = std::move(_rsent_msg_ptr->res_ptr_);
    res_ptr->error_                                = _rrecv_msg_ptr->error_;
    res_ptr->recipient_id_                         = _rctx.recipientId();
    res_ptr->req_ptr_                              = std::move(_rsent_msg_ptr);

    pmprpc_front_server->sendMessage(res_ptr->req_ptr_->recipient_id_, res_ptr, on_front_client_response, {frame::mprpc::MessageFlagsE::AwaitResponse});
}

// front server upload entry point
void on_server_request(
    frame::mprpc::ConnectionContext&       _rctx,
    std::shared_ptr<front::UploadRequest>& _rsent_msg_ptr,
    std::shared_ptr<front::UploadRequest>& _rrecv_msg_ptr,
    ErrorConditionT const&                 _rerror)
{
    solid_check(_rrecv_msg_ptr);

    auto req_ptr   = make_shared<back::UploadRequest>();
    req_ptr->name_ = std::move(_rrecv_msg_ptr->name_);
    req_ptr->iss_.str(_rrecv_msg_ptr->oss_.str());
    req_ptr->recipient_id_ = _rctx.recipientId();
    req_ptr->res_ptr_      = make_shared<front::UploadResponse>(_rrecv_msg_ptr);

    pmprpc_back_client->sendRequest("localhost", req_ptr, on_back_client_response);
}

} //namespace front

namespace back {

void on_server_request(
    frame::mprpc::ConnectionContext&       _rctx,
    std::shared_ptr<front::UploadRequest>& _rsent_msg_ptr,
    std::shared_ptr<front::UploadRequest>& _rrecv_msg_ptr,
    ErrorConditionT const&                 _rerror)
{
}

} //namespace back

} //namespace
