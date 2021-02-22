#include "solid/frame/mprpc/mprpcsocketstub_openssl.hpp"

#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioactor.hpp"
#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioresolver.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include "solid/frame/aio/openssl/aiosecurecontext.hpp"
#include "solid/frame/aio/openssl/aiosecuresocket.hpp"

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v3.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include "solid/system/socketaddress.hpp"
#include <condition_variable>
#include <mutex>

#include "cxxopts.hpp"

#include <fstream>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <streambuf>
#include <string>

using namespace std;
using namespace solid;

using AioSchedulerT  = frame::Scheduler<frame::aio::Reactor>;
using SecureContextT = frame::aio::openssl::Context;

//------------------------------------------------------------------
//------------------------------------------------------------------

struct Params {
    typedef std::vector<std::string>       StringVectorT;
    typedef std::vector<SocketAddressInet> PeerAddressVectorT;
    vector<string>                         dbg_modules;
    string                                 dbg_addr;
    string                                 dbg_port;
    bool                                   dbg_console;
    bool                                   dbg_buffered;

    std::string   baseport;
    bool          log;
    StringVectorT connectstringvec;
    bool          secure;

    bool prepare(frame::mprpc::Configuration& _rcfg, string& _err);
};

//------------------------------------------------------------------

struct ServerStub {
    ServerStub()
        : minmsec(0xffffffff)
        , maxmsec(0)
        , sz(0)
    {
    }
    uint64_t minmsec;
    uint64_t maxmsec;
    uint64_t sz;
};

struct MessageStub {
    MessageStub()
        : count(0)
    {
    }
    uint32_t count;
};

typedef std::vector<ServerStub>  ServerVectorT;
typedef std::vector<MessageStub> MessageVectorT;

struct FirstMessage;

namespace {
mutex              mtx;
condition_variable cnd;
Params             params;

void broadcast_message(frame::mprpc::Service& _rsvc, std::shared_ptr<frame::mprpc::Message>& _rmsgptr);
} //namespace

struct FirstMessage : frame::mprpc::Message {
    std::string str;

    FirstMessage(std::string const& _str)
        : str(_str)
    {
        solid_log(generic_logger, Info, "CREATE ---------------- " << (void*)this);
    }
    FirstMessage()
    {
        solid_log(generic_logger, Info, "CREATE ---------------- " << (void*)this);
    }
    ~FirstMessage()
    {
        solid_log(generic_logger, Info, "DELETE ---------------- " << (void*)this);
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.str, _rctx, 1, "str");
    }
};

void connection_stop(frame::mprpc::ConnectionContext& _rctx)
{
    solid_log(generic_logger, Info, _rctx.recipientId() << " error: " << _rctx.error().message());
}

void incoming_connection_start(frame::mprpc::ConnectionContext& _rctx)
{
    solid_log(generic_logger, Info, _rctx.recipientId());
}

void outgoing_connection_start(frame::mprpc::ConnectionContext& _rctx)
{
    solid_log(generic_logger, Info, _rctx.recipientId());
}

std::string loadFile(const char* _path);

//------------------------------------------------------------------

bool parseArguments(Params& _par, int argc, char* argv[]);
bool restart(
    frame::mprpc::ServiceT& _ipcsvc,
    frame::aio::Resolver&   _resolver,
    AioSchedulerT&          _sch);

int main(int argc, char* argv[])
{

    cout << "Built on SolidFrame version " << solid::version_full() << endl;

    if (parseArguments(params, argc, argv))
        return 0;

    if (params.dbg_addr.size() && params.dbg_port.size()) {
        solid::log_start(
            params.dbg_addr.c_str(),
            params.dbg_port.c_str(),
            params.dbg_modules,
            params.dbg_buffered);

    } else if (params.dbg_console) {
        solid::log_start(std::cerr, params.dbg_modules);
    } else {
        solid::log_start(
            *argv[0] == '.' ? argv[0] + 2 : argv[0],
            params.dbg_modules,
            params.dbg_buffered,
            3,
            1024 * 1024 * 64);
    }
    {
        AioSchedulerT sch;

        frame::Manager         m;
        frame::mprpc::ServiceT ipcsvc(m);
        ErrorConditionT        err;
        CallPool<void()>       cwp{WorkPoolConfiguration(), 1};
        frame::aio::Resolver   resolver(cwp);

        if (!restart(ipcsvc, resolver, sch)) {
            return 1;
        }

        {
            string s;
            do {
                getline(cin, s);
                if (s.size() == 1 && (s[0] == 'q' || s[0] == 'Q')) {
                    s.clear();
                } else if (s.size() == 1 && (s[0] == 'r' || s[0] == 'R')) {
                    if (!restart(ipcsvc, resolver, sch)) {
                        return 1;
                    }
                } else {
                    std::shared_ptr<frame::mprpc::Message> msgptr(new FirstMessage(s));
                    broadcast_message(ipcsvc, msgptr);
                }
            } while (s.size());
        }
        solid_log(generic_logger, Verbose, "done stop");
    }
    return 0;
}

bool restart(
    frame::mprpc::ServiceT& _ipcsvc,
    frame::aio::Resolver&   _resolver,
    AioSchedulerT&          _sch)
{
    ErrorConditionT err;
    auto            proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
        reflection::v1::metadata::factory,
        [&](auto &_rmap){
            _rmap.template registerMessage<FirstMessage>(1, "FirstMessage", 
                [](
                    frame::mprpc::ConnectionContext& _rctx,
                    std::shared_ptr<FirstMessage>&   _rsend_msg,
                    std::shared_ptr<FirstMessage>&   _rrecv_msg,
                    ErrorConditionT const&           _rerr)
                {
                    if (_rrecv_msg) {
                        solid_log(generic_logger, Info, _rctx.recipientId() << " Message received: is_on_sender: " << _rrecv_msg->isOnSender() << ", is_on_peer: " << _rrecv_msg->isOnPeer() << ", is_back_on_sender: " << _rrecv_msg->isBackOnSender());
                        if (_rrecv_msg->isOnPeer()) {
                            _rctx.service().sendResponse(_rctx.recipientId(), _rrecv_msg);
                        } else if (_rrecv_msg->isBackOnSender()) {
                            cout << "Received from " << _rctx.recipientName() << ": " << _rrecv_msg->str << endl;
                        }
                    }
                }
            );
        }
    );

    frame::Manager& rm = _ipcsvc.manager();

    rm.stop();
    _sch.stop();

    rm.start();

    _sch.start(thread::hardware_concurrency());

    frame::mprpc::Configuration cfg(_sch, proto);
    
    if (params.baseport.size()) {
        cfg.server.listener_address_str = "0.0.0.0:";
        cfg.server.listener_address_str += params.baseport;
        cfg.server.listener_service_str = params.baseport;
    }

    if (params.connectstringvec.size()) {
        cfg.client.name_resolve_fnc = frame::mprpc::InternetResolverF(_resolver, params.baseport.c_str());
    }
    cfg.connection_stop_fnc           = &connection_stop;
    cfg.server.connection_start_fnc   = &incoming_connection_start;
    cfg.client.connection_start_fnc   = &outgoing_connection_start;
    cfg.client.connection_start_state = frame::mprpc::ConnectionState::Active;
    cfg.server.connection_start_state = frame::mprpc::ConnectionState::Active;
#if 1
    if (params.secure) {
        //configure OpenSSL:
        solid_log(generic_logger, Info, "Configure SSL ---------------------------------------");
        frame::mprpc::openssl::setup_client(
            cfg,
            [](frame::aio::openssl::Context& _rctx) -> ErrorCodeT {
                //_rctx.loadVerifyFile("echo-ca-cert.pem"/*"/etc/pki/tls/certs/ca-bundle.crt"*/);
                //_rctx.loadCertificateFile("echo-client-cert.pem");
                //_rctx.loadPrivateKeyFile("echo-client-key.pem");
                _rctx.addVerifyAuthority(loadFile("echo-ca-cert.pem"));
                _rctx.loadCertificate(loadFile("echo-client-cert.pem"));
                _rctx.loadPrivateKey(loadFile("echo-client-key.pem"));
                return ErrorCodeT();
            },
            frame::mprpc::openssl::NameCheckSecureStart{"echo-server"});

        frame::mprpc::openssl::setup_server(
            cfg,
            [](frame::aio::openssl::Context& _rctx) -> ErrorCodeT {
                _rctx.loadVerifyFile("echo-ca-cert.pem" /*"/etc/pki/tls/certs/ca-bundle.crt"*/);
                _rctx.loadCertificateFile("echo-server-cert.pem");
                _rctx.loadPrivateKeyFile("echo-server-key.pem");
                return ErrorCodeT();
            },
            frame::mprpc::openssl::NameCheckSecureStart{"echo-client"} //does nothing - OpenSSL does not check for hostname on SSL_accept
        );
    }
#endif

    _ipcsvc.start(std::move(cfg));

    {
        std::ostringstream oss;
        oss << _ipcsvc.configuration().server.listenerPort();
        solid_log(generic_logger, Info, "server listens on port: " << oss.str());
    }
    return true;
}

//------------------------------------------------------------------
bool parseArguments(Params& _par, int argc, char* argv[])
{
    using namespace cxxopts;
    try {
        Options options{argv[0], "SolidFrame concept application"};
        // clang-format off
        options.add_options()
            ("l,listen-port", "IPC Listen port", value<std::string>(_par.baseport)->default_value("2000"))
            ("c,connect", "Peer to connect to: host:port", value<vector<string>>(_par.connectstringvec))
            ("s,secure", "Use SSL to secure communication", value<bool>(_par.secure)->implicit_value("true")->default_value("false"))
            ("M,debug-modules", "Debug logging modules", value<vector<string>>(_par.dbg_modules))
            ("A,debug-address", "Debug server address (e.g. on linux use: nc -l 2222)", value<string>(_par.dbg_addr))
            ("P,debug-port", "Debug server port (e.g. on linux use: nc -l 2222)", value<string>(_par.dbg_port))
            ("C,debug-console", "Debug console", value<bool>(_par.dbg_console)->implicit_value("true")->default_value("false"))
            ("S,debug-unbuffered", "Debug unbuffered", value<bool>(_par.dbg_buffered)->implicit_value("false")->default_value("true"))
            ("h,help", "List program options");
        // clang-format on
        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help({""}) << std::endl;
            return true;
        }
        return false;
    } catch (exception& e) {
        cout << e.what() << "\n";
        return true;
    }
}
//------------------------------------------------------
bool Params::prepare(frame::mprpc::Configuration& _rcfg, string& _err)
{
    return true;
}

//------------------------------------------------------
//      FirstMessage
//------------------------------------------------------

std::string loadFile(const char* _path)
{
    ifstream      ifs(_path);
    ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

namespace {

void broadcast_message(frame::mprpc::Service& _rsvc, std::shared_ptr<frame::mprpc::Message>& _rmsgptr)
{

    solid_log(generic_logger, Verbose, "done stop===============================");

    for (Params::StringVectorT::const_iterator it(params.connectstringvec.begin()); it != params.connectstringvec.end(); ++it) {
        _rsvc.sendMessage(it->c_str(), _rmsgptr, {frame::mprpc::MessageFlagsE::AwaitResponse});
    }
}

} //namespace
