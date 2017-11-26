#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aiosocket.hpp"
#include "solid/frame/aio/aiostream.hpp"
#include "solid/frame/aio/openssl/aiosecurecontext.hpp"
#include "solid/frame/aio/openssl/aiosecuresocket.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/system/debug.hpp"
#include "solid/system/socketaddress.hpp"
#include "solid/system/socketdevice.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/utility/event.hpp"

#include "boost/program_options.hpp"

#include <functional>
#include <iostream>
#include <signal.h>

using namespace std;
using namespace solid;
using namespace std::placeholders;

typedef frame::Scheduler<frame::aio::Reactor> AioSchedulerT;
typedef frame::aio::openssl::Context          SecureContextT;

//------------------------------------------------------------------
//------------------------------------------------------------------

struct Params {
    string connect_addr;
    string connect_port;
    string dbg_levels;
    string dbg_modules;
    string dbg_addr;
    string dbg_port;
    bool   dbg_console;
    bool   dbg_buffered;
    bool   log;
};

using ConnectStT = std::pair<std::string&, std::string&>;

struct ConnectStub {
    frame::aio::Resolver& resolver;
    std::string&          connect_addr;
    std::string&          connect_port;

    ConnectStub(
        frame::aio::Resolver& _resolver,
        std::string&          _connect_addr,
        std::string&          _connect_port)
        : resolver(_resolver)
        , connect_addr(_connect_addr)
        , connect_port(_connect_port)
    {
    }
};

struct ConnectFunction;

class Connection : public Dynamic<Connection, frame::aio::Object> {
public:
    Connection(SecureContextT& _secure_ctx)
        : sock(this->proxy(), _secure_ctx)
        , crt_send_idx(0)
    {
    }
    ~Connection() {}

    static void onConnect(frame::aio::ReactorContext& _rctx, ConnectFunction& _rconnect);

protected:
    /*virtual*/ void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent);
    static void onRecv(frame::aio::ReactorContext& _rctx, size_t _sz);
    static void onSent(frame::aio::ReactorContext& _rctx);
    static void onSecureConnect(frame::aio::ReactorContext& _rctx);
    static bool onSecureVerify(frame::aio::ReactorContext& _rctx, bool _preverified, frame::aio::openssl::VerifyContext& _rverify_ctx);

    unsigned crtFillIdx() const
    {
        return (crt_send_idx + 1) % 2;
    }
    unsigned crtSendIdx() const
    {
        return crt_send_idx;
    }
    unsigned nextSendIdx()
    {
        return crt_send_idx = (crt_send_idx + 1) % 2;
    }

protected:
    typedef frame::aio::Stream<frame::aio::openssl::Socket> StreamSocketT;
    //typedef frame::aio::Stream<frame::aio::Socket>        StreamSocketT;

    enum { BufferCapacity = 1024 * 2 };

    char          buf[BufferCapacity];
    StreamSocketT sock;
    string        send_strs[2];
    unsigned      crt_send_idx;
};

bool parseArguments(Params& _par, int argc, char* argv[]);

int main(int argc, char* argv[])
{
    Params p;
    if (parseArguments(p, argc, argv))
        return 0;

    SecureContextT secure_ctx(SecureContextT::create());
#if 0
    {
        ErrorCodeT err = secure_ctx.configure("/etc/pki/tls/openssl.cnf");
        if(err){
            cout<<"error configuring openssl: "<<err.message()<<endl;
            return 0;
        }
    }
#endif
    signal(SIGPIPE, SIG_IGN);

#ifdef SOLID_HAS_DEBUG
    {
        string dbgout;
        Debug::the().levelMask(p.dbg_levels.c_str());
        Debug::the().moduleMask(p.dbg_modules.c_str());
        if (p.dbg_addr.size() && p.dbg_port.size()) {
            Debug::the().initSocket(
                p.dbg_addr.c_str(),
                p.dbg_port.c_str(),
                p.dbg_buffered,
                &dbgout);
        } else if (p.dbg_console) {
            Debug::the().initStdErr(
                p.dbg_buffered,
                &dbgout);
        } else {
            Debug::the().initFile(
                *argv[0] == '.' ? argv[0] + 2 : argv[0],
                p.dbg_buffered,
                3,
                1024 * 1024 * 64,
                &dbgout);
        }
        cout << "Debug output: " << dbgout << endl;
        dbgout.clear();
        Debug::the().moduleNames(dbgout);
        cout << "Debug modules: " << dbgout << endl;
    }
#endif

    {
        ErrorCodeT err = secure_ctx.loadVerifyFile("echo-ca-cert.pem" /*"/etc/pki/tls/certs/ca-bundle.crt"*/);
        if (err) {
            cout << "error configuring openssl: " << err.message() << endl;
            return 0;
        }
        secure_ctx.loadCertificateFile("echo-client-cert.pem");
        secure_ctx.loadPrivateKeyFile("echo-client-key.pem");
    }

    {

        AioSchedulerT scheduler;

        frame::Manager   manager;
        frame::ServiceT  service(manager);
        frame::ObjectIdT objuid;

        frame::aio::Resolver resolver;
        ErrorConditionT      err;

        err = scheduler.start(1);

        if (err) {
            cout << "Error starting aio scheduler: " << err.message() << endl;
            return 1;
        }

        err = resolver.start(1);

        if (err) {
            cout << "Error starting aio resolver: " << err.message() << endl;
            return 1;
        }

        {

            DynamicPointer<frame::aio::Object> objptr(new Connection(secure_ctx));
            solid::ErrorConditionT             err;

            objuid = scheduler.startObject(objptr, service, make_event(GenericEvents::Start, ConnectStub(resolver, p.connect_addr, p.connect_port)), err);

            idbg("Started Client Connection object: " << objuid.index << ',' << objuid.unique);
        }

        while (true) {
            string line;

            getline(cin, line);

            if (line == "q" or line == "Q" or line == "quit") {
                break;
            }
            line += "\r\n";
            if (manager.notify(objuid, make_event(GenericEvents::Message, std::move(line)))) {
            } else
                break;
        }
    }
    return 0;
}

//-----------------------------------------------------------------------------
bool parseArguments(Params& _par, int argc, char* argv[])
{
    using namespace boost::program_options;
    try {
        options_description desc("SolidFrame concept application");
        desc.add_options()("help,h", "List program options")("connect-addr,a", value<string>(&_par.connect_addr), "Connect address")("connect-port,p", value<string>(&_par.connect_port)->default_value("5000"), "Connect port")("debug-levels,L", value<string>(&_par.dbg_levels)->default_value("view"), "Debug logging levels")("debug-modules,M", value<string>(&_par.dbg_modules), "Debug logging modules")("debug-address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")("debug-port,P", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")("debug-console,C", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")("debug-unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered");
        variables_map vm;
        store(parse_command_line(argc, argv, desc), vm);
        notify(vm);
        if (vm.count("help")) {
            cout << desc << "\n";
            return true;
        }
        return false;
    } catch (exception& e) {
        cout << e.what() << "\n";
        return true;
    }
}

//-----------------------------------------------------------------------------
//      Connection
//-----------------------------------------------------------------------------

struct ConnectFunction {
    Event                       event;
    ResolveData::const_iterator iterator;

    ConnectFunction() {}
    ConnectFunction(ConnectFunction&& _ucf)
        : event{std::move(_ucf.event)}
        , iterator{_ucf.iterator}
    {
    }
    ConnectFunction(ConnectFunction const& _rcf)
        : event{_rcf.event}
        , iterator{_rcf.iterator}
    {
    }

    void operator()(frame::aio::ReactorContext& _rctx)
    {
        Connection::onConnect(_rctx, *this);
    }
    void operator()(frame::aio::ReactorContext& _rctx, Event&& /*_revent*/)
    {
        Connection::onConnect(_rctx, *this);
    }
};

/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    edbg(this << " " << _revent);
    if (generic_event_message == _revent) {
        std::string* pline = _revent.any().cast<std::string>();

        SOLID_CHECK(pline != nullptr);

        send_strs[crtFillIdx()].append(*pline);

        if (not sock.hasPendingSend()) {
            crt_send_idx = crtFillIdx();
            sock.postSendAll(_rctx, send_strs[crtSendIdx()].data(), send_strs[crtSendIdx()].size(), onSent);
        }

    } else if (generic_event_start == _revent) {
        ConnectStub* pconnect_stub = _revent.any().cast<ConnectStub>();

        SOLID_CHECK(pconnect_stub != nullptr);
        idbg("Resolving: " << pconnect_stub->connect_addr << ':' << pconnect_stub->connect_port);

        frame::Manager&  manager = _rctx.service().manager();
        frame::ObjectIdT objuid  = _rctx.service().manager().id(*this);

        sock.secureSetCheckHostName(_rctx, "echo-server" /*pconnect_stub->connect_addr*/);

        pconnect_stub->resolver.requestResolve(
            [&manager, objuid](ResolveData& _rrd, ErrorCodeT const& /*_rerr*/) {
                idbg("send resolv_message");
                manager.notify(objuid, make_event(GenericEvents::Raise, std::move(_rrd)));
            },
            pconnect_stub->connect_addr.c_str(),
            pconnect_stub->connect_port.c_str(), 0, SocketInfo::Inet4, SocketInfo::Stream);
    } else if (generic_event_raise == _revent) {
        ResolveData* presolve_data = _revent.any().cast<ResolveData>();

        SOLID_CHECK(presolve_data != nullptr);

        if (not presolve_data->empty()) {
            ConnectFunction cf;
            cf.event    = std::move(_revent);
            cf.iterator = presolve_data->begin();
            SocketAddressInet inetaddr(cf.iterator);

            idbg("Connect to: " << inetaddr);

            if (sock.connect(_rctx, cf.iterator, cf)) {
                onConnect(_rctx, cf);
            }
        } else {
            edbg(this << " postStop");
            postStop(_rctx);
        }
    } else if (generic_event_kill == _revent) {
        edbg(this << " postStop");
        sock.shutdown(_rctx);
        postStop(_rctx);
    }
}

/*static*/ void Connection::onRecv(frame::aio::ReactorContext& _rctx, size_t _sz)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());
    idbg(&rthis << " " << _sz);
    cout.write(rthis.buf, _sz);
    //cout<<endl;
    if (!_rctx.error()) {
        rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv); //fully asynchronous call
    } else {
        edbg(&rthis << " postStop");
        rthis.postStop(_rctx);
    }
}

/*static*/ void Connection::onSent(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());
    if (!_rctx.error()) {
        idbg(&rthis << " postRecvSome");

        rthis.send_strs[rthis.crtSendIdx()].clear();

        rthis.nextSendIdx();

        if (rthis.send_strs[rthis.crtSendIdx()].size()) { //we have something to send
            rthis.sock.postSendAll(_rctx, rthis.send_strs[rthis.crtSendIdx()].data(), rthis.send_strs[rthis.crtSendIdx()].size(), onSent);
        }
    } else {
        edbg(&rthis << " postStop");
        rthis.postStop(_rctx);
    }
}

/*static*/ void Connection::onSecureConnect(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());
    if (!_rctx.error()) {
        idbg(&rthis << "");
        rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv); //fully asynchronous call

        rthis.crt_send_idx = 0;

        if (rthis.send_strs[0].size()) { //we have something to send
            rthis.sock.postSendAll(_rctx, rthis.send_strs[0].data(), rthis.send_strs[0].size(), onSent);
        }
    } else {
        edbg(&rthis << " postStop because: " << _rctx.systemError().message());
        rthis.postStop(_rctx);
    }
}

/*static*/ void Connection::onConnect(frame::aio::ReactorContext& _rctx, ConnectFunction& _rcf)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());

    idbg(&rthis << "");

    if (!_rctx.error()) {
        idbg(&rthis << " Connected");
        rthis.sock.secureSetVerifyDepth(_rctx, 10);
        rthis.sock.secureSetCheckHostName(_rctx, "echo-server");
        rthis.sock.secureSetVerifyCallback(_rctx, frame::aio::openssl::VerifyModePeer, onSecureVerify);
        if (rthis.sock.secureConnect(_rctx, onSecureConnect)) {
            onSecureConnect(_rctx);
        }
    } else {
        ResolveData* presolve_data = _rcf.event.any().cast<ResolveData>();

        SOLID_CHECK(presolve_data != nullptr);

        ++_rcf.iterator;

        if (_rcf.iterator != presolve_data->end()) {
            if (rthis.sock.connect(_rctx, _rcf.iterator, std::move(_rcf))) {
                rthis.post(_rctx, std::move(_rcf));
            }
        } else {
            edbg(&rthis << " postStop");
            rthis.postStop(_rctx);
        }
    }
}

/*static*/ bool Connection::onSecureVerify(frame::aio::ReactorContext& _rctx, bool _preverified, frame::aio::openssl::VerifyContext& _rverify_ctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());
    idbg(&rthis << " " << _preverified);
    return _preverified;
}
