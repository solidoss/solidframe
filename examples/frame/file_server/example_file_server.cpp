#include "solid/frame/manager.hpp"
#include "solid/frame/reactor.hpp"
#include "solid/frame/scheduler.hpp"

#include "solid/frame/aio/aioactor.hpp"
#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioreactorcontext.hpp"
#include "solid/frame/aio/aiosocket.hpp"
#include "solid/frame/aio/aiostream.hpp"

#include "solid/frame/service.hpp"

#include "solid/frame/file/filestream.hpp"

#include "solid/utility/event.hpp"

#include "solid/system/exception.hpp"
#include "solid/system/socketaddress.hpp"
#include "solid/system/socketdevice.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>

#include "cxxopts.hpp"

#include <fstream>
#include <functional>
#include <iostream>
#include <signal.h>

using namespace std;
using namespace solid;
using namespace std::placeholders;

typedef frame::IndexT                         IndexT;
typedef frame::Scheduler<frame::aio::Reactor> AioSchedulerT;
typedef frame::Scheduler<frame::Reactor>      SchedulerT;

enum Events {
    EventStartE = 0,
    EventRunE,
    EventStopE,
    EventMessageE,
};

//------------------------------------------------------------------
//------------------------------------------------------------------

struct Params {
    int            start_port;
    vector<string> dbg_modules;
    string         dbg_addr;
    string         dbg_port;
    bool           dbg_console;
    bool           dbg_buffered;
    bool           log;
};

namespace {

mutex              mtx;
condition_variable cnd;
bool               run(true);
void               term_handler(int signum)
{
    switch (signum) {
    case SIGINT:
    case SIGTERM: {
        if (run) {
            lock_guard<mutex> lock(mtx);
            run = false;
            cnd.notify_all();
        }
    }
    }
}

using FileStoreSharedPointerT = std::shared_ptr<frame::file::Store<>>;

FileStoreSharedPointerT filestoreptr;

} //namespace

//------------------------------------------------------------------
//------------------------------------------------------------------

class Listener : public frame::aio::Actor {
public:
    Listener(
        frame::Service& _rsvc,
        AioSchedulerT&  _rsched,
        SocketDevice&   _rsd)
        : rsvc(_rsvc)
        , rsch(_rsched)
        , sock(this->proxy(), std::move(_rsd))
    {
    }
    ~Listener()
    {
    }

private:
    /*virtual*/ void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent);
    void             onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd);

    typedef frame::aio::Listener ListenerSocketT;

    frame::Service& rsvc;
    AioSchedulerT&  rsch;
    ListenerSocketT sock;
};

//------------------------------------------------------------------
//------------------------------------------------------------------

struct FilePointerMessage;

class Connection : public frame::aio::Actor {
    typedef frame::file::FileIOStream<1024>        IOFileStreamT;
    typedef frame::aio::Stream<frame::aio::Socket> StreamSocketT;

public:
    Connection(SocketDevice& _rsd);
    ~Connection();

private:
    /*virtual*/ void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent);
    static void      onRecv(frame::aio::ReactorContext& _rctx, size_t _sz);
    static void      onSend(frame::aio::ReactorContext& _rctx);
    const char*      findEnd(const char* _p);
    void             doExecuteCommand(frame::aio::ReactorContext& _rctx);
    void             doRun(frame::aio::ReactorContext& _rctx);
    void             handle(FilePointerMessage& _rmsgptr);

private:
    enum { BufferCapacity = 1024 };
    enum {
        ReadInit,
        ReadCommand,
        ReadPath,
        ExecCommand,
        WaitRead,
        WaitWrite,
        RunRead,
        RunWrite,
        CloseFileError,
    };

    StreamSocketT sock;
    int           state;
    char          bbeg[BufferCapacity];
    char*         bpos;
    char*         bend;
    char          cmd;
    std::string   path;
    IOFileStreamT iofs;
    const char*   crtpat;
};

//------------------------------------------------------------------
//------------------------------------------------------------------

bool parseArguments(Params& _par, int argc, char* argv[]);
//------------------------------------------------------------------
//------------------------------------------------------------------

int main(int argc, char* argv[])
{
    Params params;
    if (parseArguments(params, argc, argv))
        return 0;

    signal(SIGINT, term_handler); /* Die on SIGTERM */

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
        AioSchedulerT aiosched;
        SchedulerT    sched;

        frame::Manager  m;
        frame::ServiceT svc(m);

        sched.start(1);
        aiosched.start(1);

        {
            {
                frame::file::Utf8Configuration utf8cfg;
                frame::file::TempConfiguration tempcfg;

                solid_check(system("[ -d /tmp/fileserver ] || mkdir -p /tmp/fileserver") >= 0, "Error system");

                const char* homedir = getenv("HOME");

                utf8cfg.storagevec.push_back(frame::file::Utf8Configuration::Storage());
                utf8cfg.storagevec.back().globalprefix = "/";
                utf8cfg.storagevec.back().localprefix  = homedir;
                if (utf8cfg.storagevec.back().localprefix.size() && *utf8cfg.storagevec.back().localprefix.rbegin() != '/') {
                    utf8cfg.storagevec.back().localprefix.push_back('/');
                }

                tempcfg.storagevec.push_back(frame::file::TempConfiguration::Storage());
                tempcfg.storagevec.back().level    = frame::file::MemoryLevelFlag;
                tempcfg.storagevec.back().capacity = 1024 * 1024 * 10; //10MB
                tempcfg.storagevec.back().minsize  = 0;
                tempcfg.storagevec.back().maxsize  = 1024 * 10;

                tempcfg.storagevec.push_back(frame::file::TempConfiguration::Storage());
                tempcfg.storagevec.back().path       = "/tmp/fileserver/";
                tempcfg.storagevec.back().level      = frame::file::VeryFastLevelFlag;
                tempcfg.storagevec.back().capacity   = 1024 * 1024 * 10; //10MB
                tempcfg.storagevec.back().minsize    = 0;
                tempcfg.storagevec.back().maxsize    = 1024 * 10;
                tempcfg.storagevec.back().removemode = frame::file::RemoveNeverE;

                filestoreptr = std::make_shared<frame::file::Store<>>(m, utf8cfg, tempcfg);
            }

            solid::ErrorConditionT err;
            solid::frame::ActorIdT actuid;

            {
                SchedulerT::ActorPointerT actptr(filestoreptr);
                actuid = sched.startActor(std::move(actptr), svc, make_event(GenericEvents::Start), err);
            }

            {
                ResolveData rd = synchronous_resolve("0.0.0.0", params.start_port, 0, SocketInfo::Inet4, SocketInfo::Stream);

                SocketDevice sd;

                sd.create(rd.begin());
                sd.prepareAccept(rd.begin(), 2000);

                if (sd) {
                    solid::ErrorConditionT err;
                    solid::frame::ActorIdT actuid;

                    actuid = aiosched.startActor(make_shared<Listener>(svc, aiosched, sd), svc, make_event(GenericEvents::Start), err);
                    solid_log(generic_logger, Info, "Started Listener actor: " << actuid.index << ',' << actuid.unique);
                } else {
                    cout << "Error creating listener socket" << endl;
                    run = false;
                }
            }
        }

        cout << "Here some examples how to test: " << endl;
        cout << "\t$ nc localhost 2000" << endl;

        if (0) {
            unique_lock<mutex> lock(mtx);
            while (run) {
                cnd.wait(lock);
            }
        } else if (run) {
            cin.ignore();
        }
        m.stop();
        aiosched.stop();
        sched.stop();
        filestoreptr.reset();
    }

    return 0;
}
//------------------------------------------------------------------
//------------------------------------------------------------------

//------------------------------------------------------------------
//------------------------------------------------------------------
bool parseArguments(Params& _par, int argc, char* argv[])
{
    using namespace cxxopts;
    try {
        Options options{argv[0], "SolidFrame concept application"};
        // clang-format off
        options.add_options()
            ("p,port", "Listen port", value<int>(_par.start_port)->default_value("2000"))
            ("l,use-log", "Debug buffered", value<bool>(_par.log)->implicit_value("true")->default_value("false"))
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
//------------------------------------------------------------------
//------------------------------------------------------------------
/*virtual*/ void Listener::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    solid_log(generic_logger, Info, "event = " << _revent);
    if (generic_event_start == _revent) {
        sock.postAccept(_rctx, [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd) { return onAccept(_rctx, _rsd); });
    } else if (generic_event_kill == _revent) {
        postStop(_rctx);
    }
}

void Listener::onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd)
{
    solid_log(generic_logger, Info, "");
    unsigned repeatcnt = 4;

    do {
        if (!_rctx.error()) {
            solid::ErrorConditionT err;

            rsch.startActor(make_shared<Connection>(_rsd), rsvc, make_event(GenericEvents::Start), err);
        } else {
            //e.g. a limit of open file descriptors was reached - we sleep for 10 seconds
            //timer.waitFor(_rctx, NanoTime(10), std::bind(&Listener::onEvent, this, _1, frame::Event(EventStartE)));
            break;
        }
        --repeatcnt;
    } while (repeatcnt && sock.accept(_rctx, std::bind(&Listener::onAccept, this, _1, _2), _rsd));

    if (!repeatcnt) {
        sock.postAccept(
            _rctx,
            [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd) { onAccept(_rctx, _rsd); });
    }
}

//------------------------------------------------------------------
//------------------------------------------------------------------
static const char* patt = "\n.\r\n";

struct FilePointerMessage {
    FilePointerMessage() {}
    FilePointerMessage(frame::file::FilePointerT _ptr)
        : ptr(_ptr)
    {
    }

    frame::file::FilePointerT ptr;
};

static frame::UniqueId tempuid;

Connection::Connection(SocketDevice& _rsd)
    : sock(this->proxy(), std::move(_rsd))
    , crtpat(patt)
{
    state = ReadInit;
    bend  = bbeg;
}
Connection::~Connection()
{
    //state(-1);
    solid_log(generic_logger, Info, "");
}

const char* Connection::findEnd(const char* _p)
{
    const char* p = _p;
    for (; p != bend; ++p) {
        if (*p == *crtpat) {
            ++crtpat;
            if (!*crtpat)
                break;
        } else {
            crtpat = patt;
        }
    }
    if (!*crtpat) { //we've found the pattern
        return p - 2;
    }

    return p - (crtpat - patt);
}

/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    solid_log(generic_logger, Info, "" << _revent);
    if (generic_event_start == _revent) {
        solid_log(generic_logger, Info, "Start");
        sock.postRecvSome(_rctx, bbeg, BufferCapacity, Connection::onRecv);
        state = ReadCommand;
    } else if (generic_event_kill == _revent) {
        solid_log(generic_logger, Info, "Stop");
        this->postStop(_rctx);
    } else if (generic_event_message == _revent) {
        solid_log(generic_logger, Info, "Message");
        FilePointerMessage* pfileptrmsg = _revent.any().cast<FilePointerMessage>();
        if (pfileptrmsg) {
            this->handle(*pfileptrmsg);
        }
    }
}

/*static*/ void Connection::onRecv(frame::aio::ReactorContext& _rctx, size_t _sz)
{
    Connection& rthis = static_cast<Connection&>(_rctx.actor());
    if (!_rctx.error()) {
        rthis.bend   = rthis.bbeg + _sz + (rthis.crtpat - patt);
        rthis.crtpat = patt;
        rthis.bpos   = rthis.bbeg;
        rthis.doRun(_rctx);
    } else {
        rthis.postStop(_rctx);
    }
}

void Connection::doRun(frame::aio::ReactorContext& _rctx)
{
    switch (state) {
    case ReadInit:
        state = ReadCommand;
        sock.postRecvSome(_rctx, bbeg, BufferCapacity, Connection::onRecv);
        break;
    case ReadCommand:
        cmd = *bpos;
        ++bpos;
        state = ReadPath;
    case ReadPath: {
        const char* p = bpos;
        for (; p != bend; ++p) {
            if (*p == '\n')
                break;
        }
        path.append(bpos, p - bpos);
        if (*p == '\n') {
            bpos  = const_cast<char*>(p);
            state = ExecCommand;
        } else {
            sock.postRecvSome(_rctx, bbeg, BufferCapacity, Connection::onRecv);
            break;
        }
    }
    case ExecCommand:
        doExecuteCommand(_rctx);
        break;
    case WaitRead:
    case WaitWrite:
        //keep waiting
        solid_log(generic_logger, Info, "keep waiting");
        break;
    case RunRead:
        if (!iofs.eof()) {
            iofs.read(bbeg, BufferCapacity);

            sock.postSendAll(_rctx, bbeg, iofs.gcount(), onSend);
        } else {
            iofs.close();
            postStop(_rctx);
        }
        break;
    case RunWrite: {
        const char* p = findEnd(bpos);
        iofs.write(bpos, p - bpos);
        if (crtpat != patt) {
            memcpy(bbeg, patt, crtpat - patt);
        }
        if (p != bend && *p == '.') {
            iofs.close();
            postStop(_rctx);
            return;
        }
        sock.postRecvSome(_rctx, bbeg + (crtpat - patt), BufferCapacity - (crtpat - patt), Connection::onRecv);
    } break;
    case CloseFileError:
        postStop(_rctx);
        break;
    }
}
struct OpenCbk {
    frame::Manager& rm;
    frame::ActorIdT uid;

    OpenCbk(
        frame::Manager&        _rm,
        const frame::ActorIdT& _ractuid)
        : rm(_rm)
        , uid(_ractuid)
    {
    }

    void operator()(
        frame::file::Store<>&,
        frame::file::FilePointerT& _rptr,
        ErrorCodeT const& /*_rerr*/
    )
    {
        solid_log(generic_logger, Info, "");
        Event ev{generic_event_message};
        ev.any().emplace<FilePointerMessage>(_rptr);

        rm.notify(uid, std::move(ev));
    }
};

void Connection::doExecuteCommand(frame::aio::ReactorContext& _rctx)
{
    if (path.size() && path[path.size() - 1] == '\r') {
        path.resize(path.size() - 1);
    }

    frame::Manager& rm = _rctx.service().manager();

    switch (cmd) {
    case 'R':
    case 'r':
        state = WaitRead;
        ++bpos;
        solid_log(generic_logger, Info, "Request open file: " << path);
        filestoreptr->requestOpenFile(OpenCbk(rm, rm.id(*this)), path.c_str(), FileDevice::ReadWriteE);
        //post(_rctx, [this](frame::aio::ReactorContext &_rctx, frame::Event const &_revent){this->run(_rctx);});
        break;
    case 'W':
    case 'w':
        state = WaitWrite;
        ++bpos;
        solid_log(generic_logger, Info, "Request create file: " << path);
        filestoreptr->requestCreateFile(OpenCbk(rm, rm.id(*this)), path.c_str(), FileDevice::ReadWriteE);
        //post(_rctx, [this](frame::aio::ReactorContext &_rctx, frame::Event const &_revent){this->run(_rctx);});
        break;
    case 'S':
    case 's':
        if (tempuid.isValid()) {
            solid_log(generic_logger, Info, "Request shared temp - no temp");
            this->postStop(_rctx);
        } else {
            solid_log(generic_logger, Info, "Request shared temp - " << tempuid.index << '.' << tempuid.unique);
            state = WaitRead;
            ++bpos;
            filestoreptr->requestShared(OpenCbk(rm, rm.id(*this)), tempuid);
            //post(_rctx, [this](frame::aio::ReactorContext &_rctx, frame::Event const &_revent){this->run(_rctx);});
        }
        break;
    case 'U':
    case 'u':
        if (tempuid.isValid()) {
            solid_log(generic_logger, Info, "Request unique temp - no temp");
            postStop(_rctx);
        } else {
            solid_log(generic_logger, Info, "Request unique temp - " << tempuid.index << '.' << tempuid.unique);
            state = WaitRead;
            ++bpos;
            filestoreptr->requestShared(OpenCbk(rm, rm.id(*this)), tempuid);
        }
        break;
    case 'T':
    case 't':
        state = WaitWrite;
        ++bpos;
        solid_log(generic_logger, Info, "Request read temp");
        filestoreptr->requestCreateTemp(OpenCbk(rm, rm.id(*this)), 4 * 1024);
        state = WaitWrite;
        break;
    case 'F':
    case 'f':
        state = WaitWrite;
        ++bpos;
        solid_log(generic_logger, Info, "Request file temp");
        filestoreptr->requestCreateTemp(OpenCbk(rm, rm.id(*this)), 4 * 1024, frame::file::VeryFastLevelFlag);
        break;
    case 'M':
    case 'm':
        state = WaitWrite;
        ++bpos;
        solid_log(generic_logger, Info, "Request memory temp");
        filestoreptr->requestCreateTemp(OpenCbk(rm, rm.id(*this)), 4 * 1024, frame::file::MemoryLevelFlag);
        state = WaitWrite;
    default:
        postStop(_rctx);
        break;
    }
}

/*static*/ void Connection::onSend(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.actor());
    if (!_rctx.error()) {
        rthis.doRun(_rctx);
    } else {
        rthis.postStop(_rctx);
    }
}

void Connection::handle(FilePointerMessage& _rmsgptr)
{
    solid_log(generic_logger, Info, "");
    if (!_rmsgptr.ptr.empty()) {
        iofs.device(_rmsgptr.ptr);
        if (state == WaitRead) {
            state = RunRead;
        } else if (state == WaitWrite) {
            state = RunWrite;
        }
    } else {
        state = CloseFileError;
    }
}
