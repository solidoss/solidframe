#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioresolver.hpp"
#include "solid/frame/aio/aiosocket.hpp"
#include "solid/frame/aio/aiostream.hpp"
#include "solid/frame/aio/aiotimer.hpp"

#include "solid/system/log.hpp"
#include "solid/system/socketaddress.hpp"
#include "solid/system/socketdevice.hpp"
#include "solid/utility/event.hpp"

#include "solid/system/cassert.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <signal.h>

#include "boost/program_options.hpp"

using namespace std;
using namespace solid;

using AioSchedulerT = frame::Scheduler<frame::aio::Reactor>;
using AtomicSizeT   = atomic<size_t>;

struct Statistics {
    AtomicSizeT concnt;
    AtomicSizeT connectcnt;
    AtomicSizeT connectedcnt;
    AtomicSizeT donecnt;
    Statistics()
        : concnt(0)
        , connectcnt(0)
        , connectedcnt(0)
        , donecnt(0)
    {
    }
} stats;

namespace {
struct Params {
    uint32_t connection_count;
    uint32_t repeat_count;
    string   connect_addr_str;
    string   connect_port_str;

    vector<string> dbg_modules;
    string         dbg_addr;
    string         dbg_port;
    bool           dbg_console;
    bool           dbg_buffered;
};

mutex              mtx;
condition_variable cnd;
AtomicSizeT        concnt(0);
uint64_t           recv_count(0);
Params             params;

const size_t sizes[]{
    100,
    200,
    400,
    800,
    1600,
    2400,
    3200,
    4800,
    6400,
    //    8800,
    //    12800
    100,
    200,
    400,
    800,
    1600,
    2400,
    3200,
    4800,
    100,
    200,
    400,
    800,
    1600,
    2400,
    3200,
    100,
    200,
    400,
    800,
    1600,
    2400};
const size_t sizes_size = sizeof(sizes) / sizeof(size_t);

vector<string> send_data_vec;

frame::aio::Resolver& async_resolver(frame::aio::Resolver* _pres = nullptr)
{
    static frame::aio::Resolver& r = *_pres;
    return r;
}

} //namespace

class Connection : public Dynamic<Connection, frame::aio::Object> {
public:
    Connection(const size_t _idx)
        : sock(this->proxy())
        , recvcnt(0)
        , idx(_idx)
        , crt_send_idx(0)
        , expect_recvcnt(0)
    {
        ++stats.concnt;
    }
    ~Connection()
    {
        --stats.concnt;
    }

private:
    void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent) override;
    void onStop(frame::Manager& _rm) override
    {
        lock_guard<mutex> lock(mtx);
        --concnt;
        recv_count += recvcnt;
        if (concnt == 0) {
            cnd.notify_one();
        }
    }

    void doSend(frame::aio::ReactorContext& _rctx);

    static void onRecv(frame::aio::ReactorContext& _rctx, size_t _sz);
    static void onSend(frame::aio::ReactorContext& _rctx);

    static void onConnect(frame::aio::ReactorContext& _rctx);

private:
    typedef frame::aio::Stream<frame::aio::Socket> StreamSocketT;
    enum { BufferCapacity = 1024 * 2 };

    char          buf[BufferCapacity];
    StreamSocketT sock;
    uint64_t      recvcnt;
    const size_t  idx;
    size_t        crt_send_idx;
    size_t        expect_recvcnt;
};

bool parseArguments(Params& _par, int argc, char* argv[]);

void prepareSendData();

int main(int argc, char* argv[])
{

    if (parseArguments(params, argc, argv))
        return 0;

    prepareSendData();

#ifndef SOLID_ON_WINDOWS
    //signal(SIGINT, term_handler); /* Die on SIGTERM */
    signal(SIGPIPE, SIG_IGN);
#endif

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

    FunctionWorkPool     fwp;
    frame::aio::Resolver resolver(fwp);

    fwp.start(WorkPoolConfiguration());

    async_resolver(&resolver);
    {

        AioSchedulerT sch;

        frame::Manager  m;
        frame::ServiceT svc(m);

        if (sch.start(thread::hardware_concurrency())) {
            cout << "Error starting scheduler" << endl;
        } else {
            for (size_t i = 0; i < params.connection_count; ++i) {
                DynamicPointer<frame::aio::Object> objptr(new Connection(i));
                solid::ErrorConditionT             err;
                solid::frame::ObjectIdT            objuid;

                ++concnt;
                objuid = sch.startObject(objptr, svc, make_event(GenericEvents::Start), err);
                if (objuid.isInvalid()) {
                    --concnt;
                }
                solid_log(generic_logger, Info, "Started Connection Object: " << objuid.index << ',' << objuid.unique);
            }
        }

        unique_lock<mutex> lock(mtx);

        while (concnt) {
            cnd.wait(lock);
        }
        cout << "Received " << recv_count / 1024 << "KB on " << params.connection_count << " connections" << endl;
        m.stop();
    }

    return 0;
}

//-----------------------------------------------------------------------------
bool parseArguments(Params& _par, int argc, char* argv[])
{
    using namespace boost::program_options;
    try {
        options_description desc("SolidFrame concept application");
        // clang-format off
        desc.add_options()
            ("connect,c", value<string>(&_par.connect_addr_str)->default_value(""), "Connect address")
            ("connection-count,N", value<uint32_t>(&_par.connection_count)->default_value(1), "Connection count")
            ("repeat-count,R", value<uint32_t>(&_par.repeat_count)->default_value(100), "Repeat count")
            ("debug-modules,M", value<vector<string>>(&_par.dbg_modules), "Debug logging modules")
            ("debug-address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")
            ("debug-port,P", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")
            ("debug-console,C", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")
            ("debug-unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered")
            ("help,h", "List program options");
        // clang-format on
        variables_map vm;
        store(parse_command_line(argc, argv, desc), vm);
        notify(vm);
        if (vm.count("help")) {
            cout << desc << "\n";
            return true;
        }

        size_t pos;

        pos = _par.connect_addr_str.rfind(':');
        if (pos != string::npos) {
            _par.connect_addr_str[pos] = '\0';

            _par.connect_port_str.assign(_par.connect_addr_str.c_str() + pos + 1);

            _par.connect_addr_str.resize(pos);
        } else {
            _par.connect_port_str = "2222";
        }

        return false;
    } catch (exception& e) {
        cout << e.what() << "\n";
        return true;
    }
}

//-----------------------------------------------------------------------------
void prepareSendData()
{

    string pattern;
    pattern.reserve(256);

    for (int i = 0; i < 256; ++i) {
        if (isgraph(i)) {
            pattern += static_cast<char>(i);
        }
    }

    send_data_vec.resize(sizes_size);
    for (size_t i = 0; i < sizes_size; ++i) {
        auto& s = send_data_vec[i];
        s.reserve(sizes[i]);
        for (size_t j = 0; j < sizes[i]; ++j) {
            s += pattern[(i + j) % pattern.size()];
        }
    }
}

//-----------------------------------------------------------------------------
struct ResolvFunc {
    frame::Manager&  rm;
    frame::ObjectIdT objuid;

    ResolvFunc(frame::Manager& _rm, frame::ObjectIdT const& _robjuid)
        : rm(_rm)
        , objuid(_robjuid)
    {
    }

    void operator()(ResolveData& _rrd, ErrorCodeT const& _rerr)
    {
        Event ev(make_event(GenericEvents::Message));

        ev.any() = std::move(_rrd);

        solid_log(generic_logger, Info, this << " send resolv_message");
        rm.notify(objuid, std::move(ev));
    }
};

void Connection::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    solid_log(generic_logger, Info, "event = " << _revent);
    if (_revent == generic_event_start) {
        if (params.connect_addr_str.size()) {
            //we must resolve the address then connect
            solid_log(generic_logger, Info, "async_resolve = " << params.connect_addr_str << " " << params.connect_port_str);
            async_resolver().requestResolve(
                ResolvFunc(_rctx.service().manager(), _rctx.service().manager().id(*this)), params.connect_addr_str.c_str(),
                params.connect_port_str.c_str(), 0, SocketInfo::Inet4, SocketInfo::Stream);
        }
    } else if (_revent == generic_event_kill) {
        postStop(_rctx);
    } else if (generic_event_message == _revent) {
        ResolveData* presolvemsg = _revent.any().cast<ResolveData>();
        if (presolvemsg) {
            if (presolvemsg->empty()) {
                solid_log(generic_logger, Error, this << " postStop");
                //++stats.donecnt;
                postStop(_rctx);
            } else {
                ++stats.connectcnt;
                if (sock.connect(_rctx, presolvemsg->begin(), &Connection::onConnect)) {
                    onConnect(_rctx);
                }
            }
        }
    }
}

void Connection::doSend(frame::aio::ReactorContext& _rctx)
{
    size_t      sendidx = (crt_send_idx + idx) % send_data_vec.size();
    const auto& str     = send_data_vec[sendidx];
    expect_recvcnt      = str.size();

    solid_log(generic_logger, Info, this << " sending " << str.size());

    sock.postSendAll(_rctx, str.data(), str.size(), &Connection::onSend);
}

/*static*/ void Connection::onConnect(frame::aio::ReactorContext& _rctx)
{
    --stats.connectcnt;
    Connection& rthis = static_cast<Connection&>(_rctx.object());
    if (!_rctx.error()) {
        solid_log(generic_logger, Info, &rthis << " SUCCESS");
        ++stats.connectedcnt;
        rthis.sock.device().enableNoDelay();
        rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv);
        rthis.doSend(_rctx);
    } else {
        solid_log(generic_logger, Error, &rthis << " postStop " << rthis.recvcnt << " " << _rctx.systemError().message());
        //++stats.donecnt;
        rthis.postStop(_rctx);
    }
}

/*static*/ void Connection::onRecv(frame::aio::ReactorContext& _rctx, size_t _sz)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());

    if (!_rctx.error()) {
        rthis.recvcnt += _sz;

        SOLID_ASSERT(_sz <= rthis.expect_recvcnt);

        solid_log(generic_logger, Info, &rthis << " received " << _sz);

        rthis.expect_recvcnt -= _sz;

        if (rthis.expect_recvcnt == 0) {
            ++rthis.crt_send_idx;
            if (rthis.crt_send_idx != params.repeat_count) {
                rthis.doSend(_rctx);
            } else {
                rthis.postStop(_rctx);
                ++stats.donecnt;
            }
        }
        rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv);
    } else {
        solid_log(generic_logger, Error, &rthis << " postStop " << rthis.recvcnt << " " << _rctx.systemError().message());
        //++stats.donecnt;
        rthis.postStop(_rctx);
    }
}

/*static*/ void Connection::onSend(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());

    if (!_rctx.error()) {
    } else {
        solid_log(generic_logger, Error, &rthis << " postStop " << rthis.recvcnt << " " << _rctx.systemError().message());
        //++stats.donecnt;
        rthis.postStop(_rctx);
    }
}
