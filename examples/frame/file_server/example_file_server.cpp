#include "solid/frame/manager.hpp"
#include "solid/frame/reactor.hpp"
#include "solid/frame/scheduler.hpp"

#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioreactorcontext.hpp"
#include "solid/frame/aio/aiosocket.hpp"
#include "solid/frame/aio/aiostream.hpp"

#include "solid/frame/service.hpp"

#include "solid/frame/file/filestream.hpp"

#include "solid/utility/dynamictype.hpp"
#include "solid/utility/event.hpp"

#include "solid/system/socketaddress.hpp"
#include "solid/system/socketdevice.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>

#include "boost/program_options.hpp"

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
    int    start_port;
    string dbg_levels;
    string dbg_modules;
    string dbg_addr;
    string dbg_port;
    bool   dbg_console;
    bool   dbg_buffered;
    bool   log;
};

namespace {

mutex              mtx;
condition_variable cnd;
bool               run(true);
void term_handler(int signum)
{
    switch (signum) {
    case SIGINT:
    case SIGTERM: {
        if (run) {
            unique_lock<mutex> lock(mtx);
            run = false;
            cnd.notify_all();
        }
    }
    }
}

typedef DynamicPointer<frame::file::Store<>> FileStoreSharedPointerT;

FileStoreSharedPointerT filestoreptr;

} //namespace

//------------------------------------------------------------------
//------------------------------------------------------------------

class Listener : public Dynamic<Listener, frame::aio::Object> {
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
    void onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd);

    typedef frame::aio::Listener ListenerSocketT;

    frame::Service& rsvc;
    AioSchedulerT&  rsch;
    ListenerSocketT sock;
};

//------------------------------------------------------------------
//------------------------------------------------------------------

struct FilePointerMessage;

class Connection : public Dynamic<Connection, frame::aio::Object> {
    typedef std::vector<solid::DynamicPointer<>>   DynamicPointerVectorT;
    typedef frame::file::FileIOStream<1024>        IOFileStreamT;
    typedef frame::aio::Stream<frame::aio::Socket> StreamSocketT;

public:
    Connection(SocketDevice& _rsd);
    ~Connection();

private:
    /*virtual*/ void onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent);
    static void onRecv(frame::aio::ReactorContext& _rctx, size_t _sz);
    static void onSend(frame::aio::ReactorContext& _rctx);
    const char* findEnd(const char* _p);
    void doExecuteCommand(frame::aio::ReactorContext& _rctx);
    void doRun(frame::aio::ReactorContext& _rctx);
    void handle(FilePointerMessage& _rmsgptr);

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
    Params p;
    if (parseArguments(p, argc, argv))
        return 0;

    signal(SIGINT, term_handler); /* Die on SIGTERM */

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
        AioSchedulerT aiosched;
        SchedulerT    sched;

        frame::Manager  m;
        frame::ServiceT svc(m);

        if (!sched.start(1) && !aiosched.start(1)) {
            {
                frame::file::Utf8Configuration utf8cfg;
                frame::file::TempConfiguration tempcfg;

                (void)system("[ -d /tmp/fileserver ] || mkdir -p /tmp/fileserver");

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

                filestoreptr = new frame::file::Store<>(m, utf8cfg, tempcfg);
            }

            solid::ErrorConditionT  err;
            solid::frame::ObjectIdT objuid;

            {
                SchedulerT::ObjectPointerT objptr(filestoreptr);
                objuid = sched.startObject(objptr, svc, make_event(GenericEvents::Start), err);
            }

            {
                ResolveData rd = synchronous_resolve("0.0.0.0", p.start_port, 0, SocketInfo::Inet4, SocketInfo::Stream);

                SocketDevice sd;

                sd.create(rd.begin());
                sd.prepareAccept(rd.begin(), 2000);

                if (sd.ok()) {
                    DynamicPointer<frame::aio::Object> objptr(new Listener(svc, aiosched, sd));
                    solid::ErrorConditionT             err;
                    solid::frame::ObjectIdT            objuid;

                    objuid = aiosched.startObject(objptr, svc, make_event(GenericEvents::Start), err);
                    idbg("Started Listener object: " << objuid.index << ',' << objuid.unique);
                } else {
                    cout << "Error creating listener socket" << endl;
                    run = false;
                }
            }
        } else {
            run = false;
            std::cerr << "Could not start schedulers" << endl;
        }

        cout << "Here some examples how to test: " << endl;
        cout << "\t$ nc localhost 2000" << endl;

        if (0) {
            unique_lock<mutex> lock(mtx);
            while (run) {
                cnd.wait(lock);
            }
        } else if (run) {
            char c;
            cin >> c;
        }
        m.stop();
        aiosched.stop();
        sched.stop();
        filestoreptr.clear();
    }

    return 0;
}
//------------------------------------------------------------------
//------------------------------------------------------------------

//------------------------------------------------------------------
//------------------------------------------------------------------
bool parseArguments(Params& _par, int argc, char* argv[])
{
    using namespace boost::program_options;
    try {
        options_description desc("SolidFrame concept application");
        desc.add_options()("help,h", "List program options")("port,p", value<int>(&_par.start_port)->default_value(2000), "Listen port")("debug-levels,L", value<string>(&_par.dbg_levels)->default_value("iew"), "Debug logging levels")("debug-modules,M", value<string>(&_par.dbg_modules), "Debug logging modules")("debug-address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")("debug-port,P", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")("debug-console,C", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")("debug-unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered")("use-log,l", value<bool>(&_par.log)->implicit_value(true)->default_value(false), "Debug buffered");
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
//------------------------------------------------------------------
//------------------------------------------------------------------
/*virtual*/ void Listener::onEvent(frame::aio::ReactorContext& _rctx, Event&& _revent)
{
    idbg("event = " << _revent);
    if (generic_event_start == _revent) {
        sock.postAccept(_rctx, [this](frame::aio::ReactorContext& _rctx, SocketDevice& _rsd) { return onAccept(_rctx, _rsd); });
    } else if (generic_event_kill == _revent) {
        postStop(_rctx);
    }
}

void Listener::onAccept(frame::aio::ReactorContext& _rctx, SocketDevice& _rsd)
{
    idbg("");
    unsigned repeatcnt = 4;

    do {
        if (!_rctx.error()) {
            DynamicPointer<frame::aio::Object> objptr(new Connection(_rsd));
            solid::ErrorConditionT             err;

            rsch.startObject(objptr, rsvc, make_event(GenericEvents::Start), err);
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
    idbg("");
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
    idbg("" << _revent);
    if (generic_event_start == _revent) {
        idbg("Start");
        sock.postRecvSome(_rctx, bbeg, BufferCapacity, Connection::onRecv);
        state = ReadCommand;
    } else if (generic_event_kill == _revent) {
        idbg("Stop");
        this->postStop(_rctx);
    } else if (generic_event_message == _revent) {
        idbg("Message");
        FilePointerMessage* pfileptrmsg = _revent.any().cast<FilePointerMessage>();
        if (pfileptrmsg) {
            this->handle(*pfileptrmsg);
        }
    }
}

/*static*/ void Connection::onRecv(frame::aio::ReactorContext& _rctx, size_t _sz)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());
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
        idbg("keep waiting");
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
    frame::Manager&  rm;
    frame::ObjectIdT uid;

    OpenCbk(
        frame::Manager&         _rm,
        const frame::ObjectIdT& _robjuid)
        : rm(_rm)
        , uid(_robjuid)
    {
    }

    void operator()(
        frame::file::Store<>&,
        frame::file::FilePointerT& _rptr,
        ErrorCodeT const& /*_rerr*/
        )
    {
        idbg("");
        Event ev{generic_event_message};
        ev.any().reset(FilePointerMessage(_rptr));

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
        idbg("Request open file: " << path);
        filestoreptr->requestOpenFile(OpenCbk(rm, rm.id(*this)), path.c_str(), FileDevice::ReadWriteE);
        //post(_rctx, [this](frame::aio::ReactorContext &_rctx, frame::Event const &_revent){this->run(_rctx);});
        break;
    case 'W':
    case 'w':
        state = WaitWrite;
        ++bpos;
        idbg("Request create file: " << path);
        filestoreptr->requestCreateFile(OpenCbk(rm, rm.id(*this)), path.c_str(), FileDevice::ReadWriteE);
        //post(_rctx, [this](frame::aio::ReactorContext &_rctx, frame::Event const &_revent){this->run(_rctx);});
        break;
    case 'S':
    case 's':
        if (tempuid.isValid()) {
            idbg("Request shared temp - no temp");
            this->postStop(_rctx);
        } else {
            idbg("Request shared temp - " << tempuid.index << '.' << tempuid.unique);
            state = WaitRead;
            ++bpos;
            filestoreptr->requestShared(OpenCbk(rm, rm.id(*this)), tempuid);
            //post(_rctx, [this](frame::aio::ReactorContext &_rctx, frame::Event const &_revent){this->run(_rctx);});
        }
        break;
    case 'U':
    case 'u':
        if (tempuid.isValid()) {
            idbg("Request unique temp - no temp");
            postStop(_rctx);
        } else {
            idbg("Request unique temp - " << tempuid.index << '.' << tempuid.unique);
            state = WaitRead;
            ++bpos;
            filestoreptr->requestShared(OpenCbk(rm, rm.id(*this)), tempuid);
        }
        break;
    case 'T':
    case 't':
        state = WaitWrite;
        ++bpos;
        idbg("Request read temp");
        filestoreptr->requestCreateTemp(OpenCbk(rm, rm.id(*this)), 4 * 1024);
        state = WaitWrite;
        break;
    case 'F':
    case 'f':
        state = WaitWrite;
        ++bpos;
        idbg("Request file temp");
        filestoreptr->requestCreateTemp(OpenCbk(rm, rm.id(*this)), 4 * 1024, frame::file::VeryFastLevelFlag);
        break;
    case 'M':
    case 'm':
        state = WaitWrite;
        ++bpos;
        idbg("Request memory temp");
        filestoreptr->requestCreateTemp(OpenCbk(rm, rm.id(*this)), 4 * 1024, frame::file::MemoryLevelFlag);
        state = WaitWrite;
    default:
        postStop(_rctx);
        break;
    }
}

/*static*/ void Connection::onSend(frame::aio::ReactorContext& _rctx)
{
    Connection& rthis = static_cast<Connection&>(_rctx.object());
    if (!_rctx.error()) {
        rthis.doRun(_rctx);
    } else {
        rthis.postStop(_rctx);
    }
}

void Connection::handle(FilePointerMessage& _rmsgptr)
{
    idbg("");
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
