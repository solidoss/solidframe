#include "solid/frame/mprpc/mprpcsocketstub_openssl.hpp"

#include "solid/frame/mprpc/mprpccompression_snappy.hpp"
#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v2.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioactor.hpp"
#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioresolver.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include "solid/utility/string.hpp"

#include "solid/system/directory.hpp"
#include "solid/system/exception.hpp"

#include "solid/system/log.hpp"

#include <fstream>
#include <future>
#include <iomanip>
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
atomic<size_t>          expect_count(0);
promise<void>           prom;

namespace back {
struct Request;
} //namespace back

namespace front {

struct Response;

struct Request : frame::mprpc::Message {
    string   name_;
    ofstream ofs_;
    bool     send_request_;

    Request()
    {
    }

    Request(Response& _res);

    Request(const string& _name)
        : name_(_name)
        , send_request_(true)
    {
    }

    ~Request() override
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, /*_name*/)
    {
        _s.add(_rthis.name_, _rctx, "name");
    }
};

struct Response : frame::mprpc::Message {
    uint32_t                       error_;
    ostringstream                  oss_;
    mutable istringstream          iss_;
    std::shared_ptr<back::Request> req_ptr_;
    frame::mprpc::RecipientId      recipient_id_;

    Response()
        : error_(0)
    {
    }

    Response(Request& _req)
        : frame::mprpc::Message(_req)
        , error_(0)
    {
    }

    ~Response() override
    {
    }

    template <class S>
    void solidSerializeV2(S& _s, frame::mprpc::ConnectionContext& _rctx, const char* _name) const
    {
        //on serializer side
        _s.add(error_, _rctx, "error");
        auto progress_lambda = [](std::istream& _ris, uint64_t _len, const bool _done, frame::mprpc::ConnectionContext& _rctx, const char* _name) {
            if (_done) {
                solid_log(logger, Verbose, "Progress(" << _name << "): " << _len << " done = " << _done);
            }
        };
        _s.add(iss_, progress_lambda, _rctx, "file");
    }

    template <class S>
    void solidSerializeV2(S& _s, frame::mprpc::ConnectionContext& _rctx, const char* _name)
    {
        _s.add(error_, _rctx, "error");
        auto progress_lambda = [](std::ostream& _ros, uint64_t _len, const bool _done, frame::mprpc::ConnectionContext& _rctx, const char* _name) {
            if (_done) {
                solid_log(logger, Verbose, "Progress(" << _name << "): " << _len << " done = " << _done);
            }
        };
        _s.add(oss_, progress_lambda, _rctx, _name);
    }
};

Request::Request(Response& _res)
    : frame::mprpc::Message(_res)
{
}

void on_server_receive_first_request(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<front::Request>& _rsent_msg_ptr,
    std::shared_ptr<front::Request>& _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror);

void on_server_response(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Response>&       _rsent_msg_ptr,
    std::shared_ptr<Response>&       _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    solid_log(logger, Verbose, "front: on message: " << _rsent_msg_ptr.get());
}

void on_client_request(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Request>&        _rsent_msg_ptr,
    std::shared_ptr<Request>&        _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    solid_log(logger, Verbose, "front: on message");
}

void on_client_response(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Response>&       _rsent_msg_ptr,
    std::shared_ptr<Response>&       _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    solid_log(logger, Verbose, "front: on message");
}

void on_client_receive_response(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Request>&        _rsent_msg_ptr,
    std::shared_ptr<Response>&       _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror);

} //namespace front

namespace back {

struct Response;

struct Request : frame::mprpc::Message {
    string                           name_;
    std::shared_ptr<front::Response> res_ptr_;
    frame::mprpc::RecipientId        recipient_id_;
    bool                             await_response_;

    Request()
    {
    }

    Request(Response& _res);

    ~Request() override
    {
    }

    SOLID_PROTOCOL_V2(_s, _rthis, _rctx, /*_name*/)
    {
        _s.add(_rthis.name_, _rctx, "name");
    }
};

struct Response : frame::mprpc::Message {
    uint32_t         error_;
    ostringstream    oss_;
    mutable ifstream ifs_;

    Response()
        : error_(0)
    {
    }

    Response(Request& _req)
        : frame::mprpc::Message(_req)
        , error_(0)
    {
    }

    ~Response() override
    {
    }

    template <class S>
    void solidSerializeV2(S& _s, frame::mprpc::ConnectionContext& _rctx, const char* _name) const
    {
        _s.add(error_, _rctx, "error");
        auto progress_lambda = [](std::istream& _ris, uint64_t _len, const bool _done, frame::mprpc::ConnectionContext& _rctx, const char* _name) {
            if (_done) {
                solid_log(logger, Verbose, "Progress(" << _name << "): " << _len << " done = " << _done);
            }
        };
        _s.add(ifs_, 100 * 1024, progress_lambda, _rctx, "file");
    }

    template <class S>
    void solidSerializeV2(S& _s, frame::mprpc::ConnectionContext& _rctx, const char* _name)
    {
        _s.add(error_, _rctx, "error");
        auto progress_lambda = [](std::ostream& _ros, uint64_t _len, const bool _done, frame::mprpc::ConnectionContext& _rctx, const char* _name) {
            if (_done) {
                solid_log(logger, Verbose, "Progress(" << _name << "): " << _len << " done = " << _done);
            }
        };
        _s.add(oss_, progress_lambda, _rctx, _name);
    }
};

Request::Request(Response& _res)
    : frame::mprpc::Message(_res)
{
}

void on_server_receive_first_request(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Request>&        _rsent_msg_ptr,
    std::shared_ptr<Request>&        _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror);

void on_server_response(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Response>&       _rsent_msg_ptr,
    std::shared_ptr<Response>&       _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    solid_log(logger, Verbose, "back: on message");
}

void on_client_request(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Request>&        _rsent_msg_ptr,
    std::shared_ptr<Request>&        _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    solid_log(logger, Verbose, "back: on message");
}

void on_client_response(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Response>&       _rsent_msg_ptr,
    std::shared_ptr<Response>&       _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    solid_log(logger, Verbose, "back: on message");
}

} //namespace back

void create_files(vector<string>& _file_vec, const char* _path_prefix, uint64_t _count, uint64_t _start_size, uint64_t _increment_size);
void check_files(const vector<string>& _file_vec, const char* _path_prefix_client, const char* _path_prefix_server);

} //namespace

int test_clientfrontback_download(int argc, char* argv[])
{
    solid::log_start(std::cerr, {".*:EW", "test:IEW"});

    size_t   max_per_pool_connection_count = 1;
    bool     secure                        = false;
    bool     compress                      = false;
    uint64_t count                         = 4;
    uint64_t start_size                    = make_number("30M");
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
    system("rm -rf server_storage");

    Directory::create("client_storage");
    Directory::create("server_storage");

    vector<string> file_vec;
    create_files(file_vec, "server_storage", count, start_size, increment_size);
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
        CallPool<void()>  cwp{WorkPoolConfiguration(), 1};
        frame::aio::Resolver resolver(cwp);

        sch_client.start(1);
        sch_front.start(1);
        sch_back.start(1);

        std::string back_port;
        std::string front_port;

        { //mprpc back_server initialization
            auto                        proto = ProtocolT::create();
            frame::mprpc::Configuration cfg(sch_back, proto);

            proto->null(0);
            proto->registerMessage<back::Request>(back::on_server_receive_first_request, 1);
            proto->registerMessage<back::Response>(back::on_server_response, 2);

            //cfg.recv_buffer_capacity = 1024;
            //cfg.send_buffer_capacity = 1024;

            cfg.server.listener_address_str   = "0.0.0.0:0";
            cfg.server.connection_start_state = frame::mprpc::ConnectionState::Active;

            if (secure) {
                solid_dbg(logger, Info, "Configure SSL server -------------------------------------");
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

            mprpc_back_server.start(std::move(cfg));

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
            proto->registerMessage<back::Request>(back::on_client_request, 1);
            proto->registerMessage<back::Response>(back::on_client_response, 2);

            cfg.pool_max_active_connection_count = max_per_pool_connection_count;

            cfg.client.name_resolve_fnc       = frame::mprpc::InternetResolverF(resolver, back_port.c_str() /*, SocketInfo::Inet4*/);
            cfg.client.connection_start_state = frame::mprpc::ConnectionState::Active;

            if (secure) {
                solid_dbg(logger, Info, "Configure SSL client ------------------------------------");
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

            mprpc_back_client.start(std::move(cfg));

            pmprpc_back_client = &mprpc_back_client;
        }

        { //mprpc front_server initialization
            auto                        proto = ProtocolT::create();
            frame::mprpc::Configuration cfg(sch_back, proto);

            proto->null(0);
            proto->registerMessage<front::Request>(front::on_server_receive_first_request, 1);
            proto->registerMessage<front::Response>(front::on_server_response, 2);

            //cfg.recv_buffer_capacity = 1024;
            //cfg.send_buffer_capacity = 1024;

            cfg.server.listener_address_str   = "0.0.0.0:0";
            cfg.server.connection_start_state = frame::mprpc::ConnectionState::Active;

            if (secure) {
                solid_dbg(logger, Info, "Configure SSL server -------------------------------------");
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

            mprpc_front_server.start(std::move(cfg));

            {
                std::ostringstream oss;
                oss << mprpc_front_server.configuration().server.listenerPort();
                front_port = oss.str();
                solid_dbg(logger, Verbose, "front listens on port: " << front_port);
            }
            pmprpc_front_server = &mprpc_front_server;
        }

        { //mprpc front_client initialization
            auto                        proto = ProtocolT::create();
            frame::mprpc::Configuration cfg(sch_front, proto);

            proto->null(0);
            proto->registerMessage<front::Request>(front::on_client_request, 1);
            proto->registerMessage<front::Response>(front::on_client_response, 2);

            cfg.pool_max_active_connection_count = max_per_pool_connection_count;

            cfg.client.name_resolve_fnc       = frame::mprpc::InternetResolverF(resolver, front_port.c_str() /*, SocketInfo::Inet4*/);
            cfg.client.connection_start_state = frame::mprpc::ConnectionState::Active;

            if (secure) {
                solid_dbg(logger, Info, "Configure SSL client ------------------------------------");
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

            mprpc_front_client.start(std::move(cfg));
        }

        expect_count = file_vec.size();
        for (const auto& f : file_vec) {
            auto msg_ptr = make_shared<front::Request>(f);
            msg_ptr->ofs_.open(string("client_storage/") + f);

            mprpc_front_client.sendRequest("localhost", msg_ptr, front::on_client_receive_response);
        }

        solid_check(prom.get_future().wait_for(chrono::seconds(150)) == future_status::ready, "Taking too long - waited 150 secs");
        solid_log(logger, Info, "Done upload");
        check_files(file_vec, "client_storage", "server_storage");
        solid_log(logger, Info, "Done file checking - exiting");
    }
    return 0;
}

namespace {

size_t real_size(size_t _sz)
{
    //offset + (align - (offset mod align)) mod align
    return _sz + ((sizeof(uint64_t) - (_sz % sizeof(uint64_t))) % sizeof(uint64_t));
}

void create_files(vector<string>& _file_vec, const char* _path_prefix, uint64_t _count, uint64_t _start_size, uint64_t _increment_size)
{
    string pattern;
    for (int j = 0; j < 1; ++j) {
        for (int i = 0; i < 127; ++i) {
            int c = (i + j) % 127;
            if (isprint(c) != 0 && isblank(c) == 0) {
                pattern += static_cast<char>(c);
            }
        }
    }

    size_t sz = real_size(pattern.size());

    if (sz > pattern.size()) {
        pattern.resize(sz - sizeof(uint64_t));
    } else if (sz < pattern.size()) {
        pattern.resize(sz);
    }

    string   fname;
    uint64_t crtsz = _start_size;

    for (size_t i = 0; i < _count; ++i) {
        {
            ostringstream oss;
            oss << "test_file_" << i << "_" << crtsz << ".txt";
            _file_vec.emplace_back(oss.str());
        }

        fname = string(_path_prefix) + '/' + _file_vec.back();
        ofstream ofs(fname);
        solid_check(ofs, "failed open file: " << fname);

        int64_t sz   = crtsz;
        int64_t line = 0;
        do {
            ofs << hex << setw(8) << setfill('0') << line << ' ';

            ofs.write(pattern.data(), pattern.size());
            ofs << "\r\n";
            sz -= pattern.size();
            sz -= 11;
            ++line;
        } while (sz > 0);
        crtsz += _increment_size;
    }
}

void compare_streams(istream& _is1, istream& _is2)
{
    constexpr size_t bufsz = 4 * 1024;
    char             buf1[bufsz];
    char             buf2[bufsz];
    uint64_t         off = 0;

    do {
        _is1.read(buf1, bufsz);
        _is2.read(buf2, bufsz);

        size_t r1 = _is1.gcount();
        size_t r2 = _is2.gcount();

        solid_check(r1 == r2, "failed read at offset: " << off);

        solid_check(memcmp(buf1, buf2, r1) == 0, "failed chec at offset: " << off);
        off += r1;

    } while (!_is1.eof() || !_is2.eof());

    solid_check(_is1.eof() && _is2.eof(), "not both streams eof");
}

void check_files(const vector<string>& _file_vec, const char* _path_prefix_client, const char* _path_prefix_server)
{
    for (auto& f : _file_vec) {
        string   clientpath = string(_path_prefix_client) + '/' + f;
        string   serverpath = string(_path_prefix_server) + '/' + f;
        ifstream ifsc(clientpath);
        ifstream ifss(serverpath);
        solid_check(ifsc, "Failed open: " << clientpath);
        solid_check(ifss, "Failed open: " << serverpath);
        compare_streams(ifsc, ifss);
    }
}

namespace back {

void on_client_receive_response(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<back::Request>&  _rsent_msg_ptr,
    std::shared_ptr<back::Response>& _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror);

} //namespace back

namespace front {

//-----------------------------------------------------------------------------
// client
//-----------------------------------------------------------------------------

void on_client_receive_response(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Request>&        _rsent_msg_ptr,
    std::shared_ptr<Response>&       _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    solid_check(_rrecv_msg_ptr);

    solid_check(_rrecv_msg_ptr->error_ == 0);

    string s = _rrecv_msg_ptr->oss_.str();
    _rsent_msg_ptr->ofs_.write(s.data(), s.size());

    solid_log(logger, Verbose, "received response data of size: " << s.size());

    frame::mprpc::MessageFlagsT flags;

    if (!_rrecv_msg_ptr->isResponseLast()) {
        if (_rsent_msg_ptr->send_request_) {
            _rsent_msg_ptr->send_request_ = false;
            auto res_ptr                  = make_shared<Request>(*_rrecv_msg_ptr);
            auto err                      = _rctx.service().sendMessage(_rctx.recipientId(), res_ptr, {frame::mprpc::MessageFlagsE::Response});
            solid_log(logger, Verbose, "send response to: " << _rctx.recipientId() << " err: " << err.message());
        } else {
            _rsent_msg_ptr->send_request_ = true;
        }
    } else {
        _rsent_msg_ptr->ofs_.flush();
        if (expect_count.fetch_sub(1) == 1) {
            prom.set_value();
        }
    }
}

//-----------------------------------------------------------------------------
// front server side
//-----------------------------------------------------------------------------

void on_server_receive_request(
    frame::mprpc::ConnectionContext&  _rctx,
    std::shared_ptr<front::Response>& _rsent_msg_ptr,
    std::shared_ptr<front::Request>&  _rrecv_msg_ptr,
    ErrorConditionT const&            _rerror)
{
    solid_check(_rrecv_msg_ptr);
    solid_check(_rrecv_msg_ptr->name_.empty());

    solid_log(logger, Verbose, "front: received request");

    auto req_ptr = std::move(_rsent_msg_ptr->req_ptr_);

    pmprpc_back_client->sendMessage(_rsent_msg_ptr->recipient_id_, req_ptr, {frame::mprpc::MessageFlagsE::Response});
}

void on_server_receive_first_request(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<front::Request>& _rsent_msg_ptr,
    std::shared_ptr<front::Request>& _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    solid_check(_rrecv_msg_ptr);

    solid_log(logger, Verbose, "front: received first request for file: " << _rrecv_msg_ptr->name_);

    auto                        req_ptr = make_shared<back::Request>();
    frame::mprpc::MessageFlagsT flags;

    req_ptr->name_           = std::move(_rrecv_msg_ptr->name_);
    req_ptr->recipient_id_   = _rctx.recipientId();
    req_ptr->res_ptr_        = make_shared<front::Response>(*_rrecv_msg_ptr);
    req_ptr->await_response_ = true;

    pmprpc_back_client->sendRequest("localhost", req_ptr, back::on_client_receive_response, flags);
}

} //namespace front

namespace back {

void on_client_receive_response(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<back::Request>&  _rsent_msg_ptr,
    std::shared_ptr<back::Response>& _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    solid_log(logger, Verbose, "back: received response for: " << _rsent_msg_ptr->name_);

    std::shared_ptr<front::Response> res_ptr = make_shared<front::Response>();

    res_ptr->error_ = _rrecv_msg_ptr->error_;
    res_ptr->iss_.str(_rrecv_msg_ptr->oss_.str());
    res_ptr->header(_rsent_msg_ptr->res_ptr_->header());

    if (_rrecv_msg_ptr->isResponseLast()) {
        frame::mprpc::MessageFlagsT flags{frame::mprpc::MessageFlagsE::AwaitResponse, frame::mprpc::MessageFlagsE::ResponseLast};
        solid_log(logger, Verbose, "back: sent last response to front: " << res_ptr.get());
        pmprpc_front_server->sendMessage(_rsent_msg_ptr->recipient_id_, res_ptr, flags);
    } else {
        if (_rsent_msg_ptr->await_response_) {
            frame::mprpc::MessageFlagsT flags{frame::mprpc::MessageFlagsE::AwaitResponse, frame::mprpc::MessageFlagsE::ResponsePart};
            _rsent_msg_ptr->await_response_ = false;
            res_ptr->recipient_id_          = _rctx.recipientId();
            res_ptr->req_ptr_               = make_shared<back::Request>(*_rrecv_msg_ptr);
            solid_log(logger, Verbose, "back: sent response part to front with wait: " << res_ptr.get());
            pmprpc_front_server->sendMessage(_rsent_msg_ptr->recipient_id_, res_ptr, front::on_server_receive_request, flags);
        } else {
            frame::mprpc::MessageFlagsT flags{frame::mprpc::MessageFlagsE::ResponsePart};
            _rsent_msg_ptr->await_response_ = true;
            solid_log(logger, Verbose, "back: sent response part to front without wait: " << res_ptr.get());
            pmprpc_front_server->sendMessage(_rsent_msg_ptr->recipient_id_, res_ptr, flags);
        }
    }
}

//-----------------------------------------------------------------------------
// server
//-----------------------------------------------------------------------------
void on_server_receive_request(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Response>&       _rsent_msg_ptr,
    std::shared_ptr<Request>&        _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    solid_check(_rrecv_msg_ptr);

    if (!_rsent_msg_ptr->ifs_.eof()) {
        solid_log(logger, Verbose, "Sending " << _rrecv_msg_ptr->name_ << " to " << _rctx.recipientId());
        frame::mprpc::MessageFlagsT flags{frame::mprpc::MessageFlagsE::ResponsePart, frame::mprpc::MessageFlagsE::AwaitResponse};
        _rctx.service().sendMessage(_rctx.recipientId(), _rsent_msg_ptr, on_server_receive_request, flags);
        flags.reset(frame::mprpc::MessageFlagsE::AwaitResponse);
        _rctx.service().sendMessage(_rctx.recipientId(), _rsent_msg_ptr, flags);
    } else {
        solid_log(logger, Verbose, "Sending to " << _rctx.recipientId() << " last");
        frame::mprpc::MessageFlagsT flags{frame::mprpc::MessageFlagsE::ResponseLast};
        _rctx.service().sendMessage(_rctx.recipientId(), _rsent_msg_ptr, flags);
    }
}

void on_server_receive_first_request(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<Request>&        _rsent_msg_ptr,
    std::shared_ptr<Request>&        _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    string path = string("server_storage") + '/' + _rrecv_msg_ptr->name_;

    auto res_ptr = make_shared<Response>(*_rrecv_msg_ptr);

    res_ptr->ifs_.open(path);

    solid_check(res_ptr->ifs_, "failed open file: " << path);

    if (!res_ptr->ifs_.eof()) {
        solid_log(logger, Verbose, "Sending " << _rrecv_msg_ptr->name_ << " to " << _rctx.recipientId());
        frame::mprpc::MessageFlagsT flags{frame::mprpc::MessageFlagsE::ResponsePart, frame::mprpc::MessageFlagsE::AwaitResponse};
        auto                        error = _rctx.service().sendMessage(_rctx.recipientId(), res_ptr, on_server_receive_request, flags);
        solid_check(!error, "failed send message: " << error.message());
        flags.reset(frame::mprpc::MessageFlagsE::AwaitResponse);
        error = _rctx.service().sendMessage(_rctx.recipientId(), res_ptr, flags);
        solid_check(!error, "failed send message: " << error.message());
    } else {
        solid_log(logger, Verbose, "Sending " << _rsent_msg_ptr->name_ << " to " << _rctx.recipientId() << " last");
        frame::mprpc::MessageFlagsT flags{frame::mprpc::MessageFlagsE::ResponseLast};
        _rctx.service().sendMessage(_rctx.recipientId(), res_ptr, flags);
    }
}

} //namespace back

} //namespace
