#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aiosocket.hpp"
#include "solid/frame/aio/aiostream.hpp"
#include "solid/frame/aio/aioresolver.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/system/debug.hpp"
#include "solid/system/socketaddress.hpp"
#include "solid/system/socketdevice.hpp"


#include "solid/utility/event.hpp"

#include "boost/program_options.hpp"

#include <functional>
#include <iostream>
#include <signal.h>
#include <sstream>

using namespace std;
using namespace solid;

using AioSchedulerT = frame::Scheduler<frame::aio::Reactor>;
using AtomicSizeT   = atomic<size_t>;
//-----------------------------------------------------------------------------
bool                   running = true;
mutex                  mtx;
condition_variable     cnd;
size_t                 concnt = 0;
size_t                 recv_count = 0;
const size_t           repeat_count = 10000;
std::string            srv_port_str;
//-----------------------------------------------------------------------------
class Listener final : public Dynamic<Listener, frame::aio::Object> {
public:
    static size_t backlog_size()
    {
        return SocketInfo::max_listen_backlog_size();
    }

    Listener(
        frame::Service& _rsvc,
        AioSchedulerT&  _rsched,
        SocketDevice&&  _rsd)
        : rsvc(_rsvc)
        , rsch(_rsched)
        , sock(this->proxy(), std::move(_rsd))
    {
    }
    ~Listener()
    {
    }

private:
    void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent) override;
    void onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd);

    typedef frame::aio::Listener    ListenerSocketT;

    frame::Service& rsvc;
    AioSchedulerT&  rsch;
    ListenerSocketT sock;
};
//-----------------------------------------------------------------------------
class Connection : public Dynamic<Connection, frame::aio::Object> {
public:
    Connection(SocketDevice&& _rsd)
        : sock(this->proxy(), std::move(_rsd))
        , recvcnt(0)
        , sendcnt(0)
    {
    }
    ~Connection() {}
protected:
    /*virtual*/ void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent);
    static void onRecv(frame::aio::ReactorContext& _rctx, size_t _sz);
    static void onSend(frame::aio::ReactorContext& _rctx);

protected:
    typedef frame::aio::Stream<frame::aio::Socket> StreamSocketT;
    enum { BufferCapacity = 1024 * 2 };

    char          buf[BufferCapacity];
    StreamSocketT sock;
    uint64_t      recvcnt;
    uint64_t      sendcnt;
    size_t        sendcrt;
};

//-----------------------------------------------------------------------------
namespace client{
class Connection : public Dynamic<Connection, frame::aio::Object> {
public:
    Connection(const size_t _idx)
        : sock(this->proxy())
        , recvcnt(0)
        , sendcnt(0)
        , idx(_idx)
        , crt_send_idx(0)
        , expect_recvcnt(0)
    {
    }
    ~Connection()
    {
    }

private:
    void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent) override;
    void onStop(frame::Manager& _rm) override
    {
        unique_lock<mutex> lock(mtx);
        --concnt;
        recv_count += recvcnt;
        if (concnt == 0) {
            running = false;
            cnd.notify_one();
        }
    }

    void doSend(frame::aio::ReactorContext& _rctx);

    static void onRecv(frame::aio::ReactorContext& _rctx, size_t _sz);
    static void onSend(frame::aio::ReactorContext& _rctx);

    static void onConnect(frame::aio::ReactorContext& _rctx);
    
    bool checkRecvData(const size_t _sz)const;
    
private:
    typedef frame::aio::Stream<frame::aio::Socket> StreamSocketT;
    enum { BufferCapacity = 1024 * 2 };

    char          buf[BufferCapacity];
    StreamSocketT sock;
    uint64_t      recvcnt;
    uint64_t      sendcnt;
    const size_t  idx;
    size_t        crt_send_idx;
    size_t        expect_recvcnt;
};

void prepareSendData();

frame::aio::Resolver& async_resolver();

}//namespace client

int test_echo_tcp_stress(int argc, char **argv){
#ifdef SOLID_HAS_DEBUG
    Debug::the().levelMask("ew");
    Debug::the().moduleMask("frame_aio:ew any:ew");
    Debug::the().initStdErr(false, nullptr);
//Debug::the().initFile("test_clientserver_basic", false);
#endif
    size_t connection_count = 1;

    if (argc > 1) {
        connection_count = atoi(argv[1]);
        if (connection_count == 0) {
            connection_count = 1;
        }
        if (connection_count > 100) {
            connection_count = 100;
        }
    }

    bool secure   = false;
    bool relay = false;

    if (argc > 2) {
        if (*argv[2] == 's' or *argv[2] == 'S') {
            secure = true;
        }
        if (*argv[2] == 'r' or *argv[2] == 'R') {
            relay = true;
        }
    }
    
    if (argc > 3) {
        if (*argv[3] == 's' or *argv[3] == 'S') {
            secure = true;
        }
        if (*argv[3] == 'r' or *argv[3] == 'R') {
            relay = true;
        }
    }
    
    {
        AioSchedulerT   srv_sch;
        frame::Manager  srv_mgr;
        frame::ServiceT srv_svc{srv_mgr};
        
        if (srv_sch.start(thread::hardware_concurrency())) {
            running = false;
            cout << "Error starting server scheduler" << endl;
        } else {
            ResolveData rd = synchronous_resolve("0.0.0.0", "0", 0, SocketInfo::Inet4, SocketInfo::Stream);

            SocketDevice sd;

            sd.create(rd.begin());
            sd.prepareAccept(rd.begin(), Listener::backlog_size());

            if (sd) {
                {
                    SocketAddress local_address;
                    ostringstream oss;

                    sd.localAddress(local_address);

                    int srv_port = local_address.port();
                    oss<<srv_port;
                    srv_port_str = oss.str();
                }
                DynamicPointer<frame::aio::Object> objptr(new Listener(srv_svc, srv_sch, std::move(sd)));
                solid::ErrorConditionT             err;
                solid::frame::ObjectIdT            objuid;

                objuid = srv_sch.startObject(objptr, srv_svc, make_event(GenericEvents::Start), err);
                idbg("Started Listener object: " << objuid.index << ',' << objuid.unique);
            } else {
                cout << "Error creating listener socket" << endl;
                running = false;
            }
        }
        
        AioSchedulerT   clt_sch;
        frame::Manager  clt_mgr;
        frame::ServiceT clt_svc{clt_mgr};
        
        
        if (client::async_resolver().start(1) || clt_sch.start(thread::hardware_concurrency())) {
            running = false;
            cout << "Error starting client scheduler" << endl;
        } else {
            client::prepareSendData();
            
            for (size_t i = 0; i < connection_count; ++i) {
                DynamicPointer<frame::aio::Object> objptr(new client::Connection(i));
                solid::ErrorConditionT             err;
                solid::frame::ObjectIdT            objuid;

                ++concnt;
                objuid = clt_sch.startObject(objptr, clt_svc, make_event(GenericEvents::Start), err);
                if (objuid.isInvalid()) {
                    --concnt;
                }
                idbg("Started Connection Object: " << objuid.index << ',' << objuid.unique);
            }
        }
        {
            unique_lock<mutex> lock(mtx);

            if (not cnd.wait_for(lock, std::chrono::seconds(220), []() { return not running; })) {
                SOLID_THROW("Process is taking too long.");
            }
            
            cout << "Received " << recv_count / 1024 << "KB on " << connection_count << " connections" << endl;
        }
    }
    
    return 0;
}

//-----------------------------------------------------------------------------
//      Listener
//-----------------------------------------------------------------------------

/*virtual*/ void Listener::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    idbg("event = " << _revent);
    if (generic_event_start == _revent) {
        sock.postAccept(_rctx, [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd){onAccept(_rctx, _rsd);});
    } else if (generic_event_kill == _revent) {
        postStop(_rctx);
    }
}

void Listener::onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd)
{
    idbg("");
    size_t repeatcnt = backlog_size();

    do {
        if (!_rctx.error()) {
            _rsd.enableNoDelay();
            DynamicPointer<frame::aio::Object> objptr(new Connection(std::move(_rsd)));
            solid::ErrorConditionT             err;

            rsch.startObject(objptr, rsvc, make_event(GenericEvents::Start), err);
        } else {
            //e.g. a limit of open file descriptors was reached - we sleep for 10 seconds
            //timer.waitFor(_rctx, std::chrono::seconds(10), std::bind(&Listener::onEvent, this, _1, frame::Event(EventStartE)));
            break;
        }
        --repeatcnt;
    } while (repeatcnt && sock.accept(_rctx, [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd){onAccept(_rctx, _rsd);}, _rsd));

    if (!repeatcnt) {
        sock.postAccept(
            _rctx,
            [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd) { onAccept(_rctx, _rsd); }); //fully asynchronous call
    }
}

//-----------------------------------------------------------------------------
//      Connection
//-----------------------------------------------------------------------------
/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    edbg(this << " event = " << _revent);
    if (generic_event_start == _revent) {
        sock.postRecvSome(_rctx, buf, BufferCapacity, Connection::onRecv); //fully asynchronous call
    } else if (generic_event_kill == _revent) {
        edbg(this << " postStop");
        sock.shutdown(_rctx);
        postStop(_rctx);
    }
}

/*static*/ void Connection::onRecv(frame::aio::ReactorContext& _rctx, size_t _sz)
{
    unsigned    repeatcnt = 2;
    Connection& rthis     = static_cast<Connection&>(_rctx.object());
    idbg(&rthis << " " << _sz);
    do {
        if (!_rctx.error()) {
            idbg(&rthis << " write: " << _sz);
            rthis.recvcnt += _sz;
            rthis.sendcrt = _sz;
            if (rthis.sock.sendAll(_rctx, rthis.buf, _sz, Connection::onSend)) {
                if (_rctx.error()) {
                    edbg(&rthis << " postStop " << rthis.recvcnt << " " << rthis.sendcnt);
                    rthis.postStop(_rctx);
                    break;
                }
                rthis.sendcnt += rthis.sendcrt;
            } else {
                idbg(&rthis << "");
                break;
            }
        } else {
            edbg(&rthis << " postStop " << rthis.recvcnt << " " << rthis.sendcnt);
            rthis.postStop(_rctx);
            break;
        }
        --repeatcnt;
    } while (repeatcnt && rthis.sock.recvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv, _sz));

    idbg(&rthis << " " << repeatcnt);

    if (repeatcnt == 0) {
        bool rv = rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv); //fully asynchronous call
        SOLID_ASSERT(!rv);
    }
}

/*static*/ void Connection::onSend(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());
    if (!_rctx.error()) {
        idbg(&rthis << " postRecvSome");
        rthis.sendcnt += rthis.sendcrt;
        rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv); //fully asynchronous call
    } else {
        edbg(&rthis << " postStop " << rthis.recvcnt << " " << rthis.sendcnt);
        rthis.postStop(_rctx);
    }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
namespace client{
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
    8800,
    12800,
    24000,
    48000,
    96000,
    192000,
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

frame::aio::Resolver& async_resolver()
{
    static frame::aio::Resolver r;
    return r;
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

        idbg(this << " send resolv_message");
        rm.notify(objuid, std::move(ev));
    }
};

void Connection::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    idbg("event = " << _revent);
    if (_revent == generic_event_start) {
        //we must resolve the address then connect
        idbg("async_resolve = " << "127.0.0.1" << " " << srv_port_str);
        async_resolver().requestResolve(
            ResolvFunc(_rctx.service().manager(), _rctx.service().manager().id(*this)), "127.0.0.1",
            srv_port_str.c_str(), 0, SocketInfo::Inet4, SocketInfo::Stream);
    } else if (_revent == generic_event_kill) {
        postStop(_rctx);
    } else if (generic_event_message == _revent) {
        ResolveData* presolvemsg = _revent.any().cast<ResolveData>();
        if (presolvemsg) {
            if (presolvemsg->empty()) {
                edbg(this << " postStop");
                //++stats.donecnt;
                postStop(_rctx);
            } else {
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

    idbg(this << " sending " << str.size());
    
    sendcnt += str.size();

    sock.postSendAll(_rctx, str.data(), str.size(), &Connection::onSend);
}

/*static*/ void Connection::onConnect(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());
    
    if (!_rctx.error()) {
        idbg(&rthis << " SUCCESS");
        rthis.sock.device().enableNoDelay();
        rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv);
        rthis.doSend(_rctx);
    } else {
        edbg(&rthis << " postStop " << rthis.recvcnt << " " << _rctx.systemError().message());
        //++stats.donecnt;
        rthis.postStop(_rctx);
    }
}

/*static*/ void Connection::onRecv(frame::aio::ReactorContext& _rctx, size_t _sz)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());

    if (!_rctx.error()) {
        rthis.recvcnt += _sz;

        SOLID_CHECK(_sz <= rthis.expect_recvcnt);
        SOLID_CHECK(rthis.checkRecvData(_sz));
        
        idbg(&rthis << " received " << _sz);

        rthis.expect_recvcnt -= _sz;

        if (rthis.expect_recvcnt == 0) {
            ++rthis.crt_send_idx;
            if (rthis.crt_send_idx != repeat_count) {
                rthis.doSend(_rctx);
            } else {
                SOLID_CHECK(rthis.recvcnt == rthis.sendcnt);
                rthis.postStop(_rctx);
            }
        }
        rthis.sock.postRecvSome(_rctx, rthis.buf, BufferCapacity, Connection::onRecv);
    } else {
        edbg(&rthis << " postStop " << rthis.recvcnt << " " << _rctx.systemError().message());
        //++stats.donecnt;
        rthis.postStop(_rctx);
    }
}

/*static*/ void Connection::onSend(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());

    if (!_rctx.error()) {
    } else {
        edbg(&rthis << " postStop " << rthis.recvcnt << " " << _rctx.systemError().message());
        //++stats.donecnt;
        rthis.postStop(_rctx);
    }
}

bool Connection::checkRecvData(const size_t _sz)const{
    size_t      sendidx = (crt_send_idx + idx) % send_data_vec.size();
    const auto& str     = send_data_vec[sendidx];
    const char *pbuf    = str.data() + (str.size() - expect_recvcnt);
    return memcmp(buf, pbuf, _sz) == 0;
}

}//namespace client
//-----------------------------------------------------------------------------
