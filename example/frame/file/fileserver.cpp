#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
#include "frame/reactor.hpp"

#include "frame/aio/aioreactor.hpp"
#include "frame/aio/aioreactorcontext.hpp"
#include "frame/aio/aioobject.hpp"
#include "frame/aio/aiostream.hpp"
#include "frame/aio/aiolistener.hpp"
#include "frame/aio/aiosocket.hpp"

#include "frame/message.hpp"
#include "frame/service.hpp"

#include "frame/file/filestream.hpp"

#include "utility/dynamictype.hpp"

#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"

#include "boost/program_options.hpp"

#include <signal.h>
#include <iostream>
#include <fstream>
#include <functional>

using namespace std;
using namespace solid;
using namespace std::placeholders;

typedef frame::IndexT							IndexT;
typedef frame::Scheduler<frame::aio::Reactor>	AioSchedulerT;
typedef frame::Scheduler<frame::Reactor>		SchedulerT;


enum Events{
	EventStartE = 0,
	EventRunE,
	EventStopE,
	EventMessageE,
};

//------------------------------------------------------------------
//------------------------------------------------------------------

struct Params{
	int			start_port;
	string		dbg_levels;
	string		dbg_modules;
	string		dbg_addr;
	string		dbg_port;
	bool		dbg_console;
	bool		dbg_buffered;
	bool		log;
};

namespace{

	Mutex					mtx;
	Condition				cnd;
	bool					run(true);
	void term_handler(int signum){
		switch(signum) {
			case SIGINT:
			case SIGTERM:{
				if(run){
					Locker<Mutex>  lock(mtx);
					run = false;
					cnd.broadcast();
				}
			}
		}
	}
	
	typedef DynamicPointer<frame::file::Store<> >	FileStoreSharedPointerT;
	
	FileStoreSharedPointerT		filestoreptr;

}//namespace

//------------------------------------------------------------------
//------------------------------------------------------------------

class Listener: public Dynamic<Listener, frame::aio::Object>{
public:
	Listener(
		frame::Service &_rsvc,
		AioSchedulerT &_rsched,
		SocketDevice &_rsd
	):rsvc(_rsvc), rsch(_rsched), sock(this->proxy(), _rsd){}
	~Listener(){
	}
private:
	/*virtual*/ void onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent);
	void onAccept(frame::aio::ReactorContext &_rctx, SocketDevice &_rsd);
	
	typedef frame::aio::Listener		ListenerSocketT;
	
	frame::Service		&rsvc;
	AioSchedulerT		&rsch;
	ListenerSocketT		sock;
};

//------------------------------------------------------------------
//------------------------------------------------------------------

struct FilePointerMessage;

class Connection: public Dynamic<Connection, frame::aio::Object>{
	typedef solid::DynamicMapper<void, Connection>	DynamicMapperT;
	typedef std::vector<solid::DynamicPointer<> >	DynamicPointerVectorT;
	typedef frame::file::FileIOStream< 1024  >		IOFileStreamT;
	typedef frame::aio::Stream<frame::aio::Socket>	StreamSocketT;
	
	struct DynamicRegister{
		DynamicRegister(DynamicMapperT &_rdm);
	};
public:
	Connection(SocketDevice &_rsd);
	~Connection();
	
	void dynamicHandle(solid::DynamicPointer<> &_dp);
	void dynamicHandle(solid::DynamicPointer<FilePointerMessage> &_rmsgptr);
private:
	/*virtual*/ void onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent);
	static void onRecv(frame::aio::ReactorContext &_rctx, size_t _sz);
	static void onSend(frame::aio::ReactorContext &_rctx);
	const char * findEnd(const char *_p);
	void doExecuteCommand(frame::aio::ReactorContext &_rctx);
	void doRun(frame::aio::ReactorContext &_rctx);
private:
	enum {BufferCapacity = 1024};
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
	
	static DynamicMapperT		dm;
	static DynamicRegister		dr;
	StreamSocketT				sock;
	int							state;
	char						bbeg[BufferCapacity];
	char 						*bpos;
	char						*bend;
	bool						b;
	char						cmd;
	std::string					path;
	IOFileStreamT				iofs;
	const char 					*crtpat;
	DynamicPointerVectorT		dv;
};

//------------------------------------------------------------------
//------------------------------------------------------------------

bool parseArguments(Params &_par, int argc, char *argv[]);
//------------------------------------------------------------------
//------------------------------------------------------------------



int main(int argc, char *argv[]){
	Params p;
	if(parseArguments(p, argc, argv)) return 0;
	
	signal(SIGINT,term_handler); /* Die on SIGTERM */
	
	/*solid::*/Thread::init();
	
#ifdef UDEBUG
	{
	string dbgout;
	Debug::the().levelMask(p.dbg_levels.c_str());
	Debug::the().moduleMask(p.dbg_modules.c_str());
	if(p.dbg_addr.size() && p.dbg_port.size()){
		Debug::the().initSocket(
			p.dbg_addr.c_str(),
			p.dbg_port.c_str(),
			p.dbg_buffered,
			&dbgout
		);
	}else if(p.dbg_console){
		Debug::the().initStdErr(
			p.dbg_buffered,
			&dbgout
		);
	}else{
		Debug::the().initFile(
			*argv[0] == '.' ? argv[0] + 2 : argv[0],
			p.dbg_buffered,
			3,
			1024 * 1024 * 64,
			&dbgout
		);
	}
	cout<<"Debug output: "<<dbgout<<endl;
	dbgout.clear();
	Debug::the().moduleNames(dbgout);
	cout<<"Debug modules: "<<dbgout<<endl;
	}
#endif
	
	{
		AioSchedulerT	aiosched;
		SchedulerT		sched;
		
		frame::Manager	m;
		frame::Service	svc(m, frame::Event(EventStopE));
		
		if(!sched.start(1) && !aiosched.start(1)){
			{
				frame::file::Utf8Configuration	utf8cfg;
				frame::file::TempConfiguration	tempcfg;
				
				system("[ -d /tmp/fileserver ] || mkdir -p /tmp/fileserver");
				
				const char *homedir = getenv("HOME");
				
				utf8cfg.storagevec.push_back(frame::file::Utf8Configuration::Storage());
				utf8cfg.storagevec.back().globalprefix = "/";
				utf8cfg.storagevec.back().localprefix = homedir;
				if(utf8cfg.storagevec.back().localprefix.size() && *utf8cfg.storagevec.back().localprefix.rbegin() != '/'){
					utf8cfg.storagevec.back().localprefix.push_back('/');
				}
				
				tempcfg.storagevec.push_back(frame::file::TempConfiguration::Storage());
				tempcfg.storagevec.back().level = frame::file::MemoryLevelFlag;
				tempcfg.storagevec.back().capacity = 1024 * 1024 * 10;//10MB
				tempcfg.storagevec.back().minsize = 0;
				tempcfg.storagevec.back().maxsize = 1024 * 10;
				
				tempcfg.storagevec.push_back(frame::file::TempConfiguration::Storage());
				tempcfg.storagevec.back().path = "/tmp/fileserver/";
				tempcfg.storagevec.back().level = frame::file::VeryFastLevelFlag;
				tempcfg.storagevec.back().capacity = 1024 * 1024 * 10;//10MB
				tempcfg.storagevec.back().minsize = 0;
				tempcfg.storagevec.back().maxsize = 1024 * 10;
				tempcfg.storagevec.back().removemode = frame::file::RemoveNeverE;
			
				filestoreptr = new frame::file::Store<>(m, EventRunE, utf8cfg, tempcfg);
			}
			
			solid::ErrorConditionT				err;
			solid::frame::ObjectUidT			objuid;
			
			{
				SchedulerT::ObjectPointerT		objptr(filestoreptr);
				objuid = sched.startObject(objptr, svc, frame::Event(EventStartE), err);
			}
			
			{
				ResolveData		rd =  synchronous_resolve("0.0.0.0", p.start_port, 0, SocketInfo::Inet4, SocketInfo::Stream);
	
				SocketDevice	sd;
				
				sd.create(rd.begin());
				sd.prepareAccept(rd.begin(), 2000);
				
				if(sd.ok()){
					DynamicPointer<frame::aio::Object>	objptr(new Listener(svc, aiosched, sd));
					solid::ErrorConditionT				err;
					solid::frame::ObjectUidT			objuid;
					
					objuid = aiosched.startObject(objptr, svc, frame::Event(EventStartE), err);
					idbg("Started Listener object: "<<objuid.index<<','<<objuid.unique);
				}else{
					cout<<"Error creating listener socket"<<endl;
					run = false;
				}
			}
		}else{
			run = false;
			std::cerr<<"Could not start schedulers"<<endl;
		}
		
		
		cout<<"Here some examples how to test: "<<endl;
		cout<<"\t$ nc localhost 2000"<<endl;
		
		if(0){
			Locker<Mutex>	lock(mtx);
			while(run){
				cnd.wait(lock);
			}
		}else if(!run){
			char c;
			cin>>c;
		}
		m.stop();
		
	}
	/*solid::*/Thread::waitAll();
	
	return 0;
}
//------------------------------------------------------------------
//------------------------------------------------------------------

//------------------------------------------------------------------
//------------------------------------------------------------------
bool parseArguments(Params &_par, int argc, char *argv[]){
	using namespace boost::program_options;
	try{
		options_description desc("SolidFrame concept application");
		desc.add_options()
			("help,h", "List program options")
			("port,p", value<int>(&_par.start_port)->default_value(2000),"Listen port")
			("debug-levels,L", value<string>(&_par.dbg_levels)->default_value("iew"),"Debug logging levels")
			("debug-modules,M", value<string>(&_par.dbg_modules),"Debug logging modules")
			("debug-address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")
			("debug-port,P", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")
			("debug-console,C", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")
			("debug-unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered")
			("use-log,l", value<bool>(&_par.log)->implicit_value(true)->default_value(false), "Debug buffered")
		;
		variables_map vm;
		store(parse_command_line(argc, argv, desc), vm);
		notify(vm);
		if (vm.count("help")) {
			cout << desc << "\n";
			return true;
		}
		return false;
	}catch(exception& e){
		cout << e.what() << "\n";
		return true;
	}
}
//------------------------------------------------------------------
//------------------------------------------------------------------
/*virtual*/ void Listener::onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
	idbg("event = "<<_revent.id);
	if(_revent.id == EventStartE){
		sock.postAccept(_rctx, [this](frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){return onAccept(_rctx, _rsd);});
	}else if(_revent.id == EventStopE){
		postStop(_rctx);
	}
}

void Listener::onAccept(frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){
	idbg("");
	unsigned	repeatcnt = 4;
	
	do{
		if(!_rctx.error()){
			//cout<<"recvbuffsz = "<<_rsd.recvBufferSize().second<<endl;
			//cout<<"sendbuffsz = "<<_rsd.sendBufferSize().second<<endl;
			_rsd.recvBufferSize(1024 * 64);
			_rsd.sendBufferSize(1024 * 32);
			DynamicPointer<frame::aio::Object>	objptr(new Connection(_rsd));
			solid::ErrorConditionT				err;
			
			rsch.startObject(objptr, rsvc, frame::Event(EventStartE), err);
		}else{
			//e.g. a limit of open file descriptors was reached - we sleep for 10 seconds
			//timer.waitFor(_rctx, TimeSpec(10), std::bind(&Listener::onEvent, this, _1, frame::Event(EventStartE)));
			break;
		}
		--repeatcnt;
	}while(repeatcnt && sock.accept(_rctx, std::bind(&Listener::onAccept, this, _1, _2), _rsd));
	
	if(!repeatcnt){
		sock.postAccept(
			_rctx,
			[this](frame::aio::ReactorContext &_rctx, SocketDevice &_rsd){onAccept(_rctx, _rsd);}
		);
	}
}

//------------------------------------------------------------------
//------------------------------------------------------------------
static const char * patt = "\n.\r\n";

struct FilePointerMessage: solid::Dynamic<FilePointerMessage, solid::frame::Message>{
	FilePointerMessage(){}
	FilePointerMessage(frame::file::FilePointerT _ptr):ptr(_ptr){}
	
	frame::file::FilePointerT	ptr;
};

Connection::DynamicRegister::DynamicRegister(Connection::DynamicMapperT& _rdm){
	_rdm.insert<FilePointerMessage, Connection>();
}

/*static*/ Connection::DynamicMapperT		Connection::dm;
/*static*/ Connection::DynamicRegister		Connection::dr(Connection::dm);

static frame::UidT							tempuid;


Connection::Connection(SocketDevice &_rsd):
	sock(this->proxy(), _rsd), b(false), crtpat(patt)
{
	state = ReadInit;
	bend = bbeg;
}
Connection::~Connection(){
	//state(-1);
	idbg("");
}

const char * Connection::findEnd(const char *_p){
	const char *p = _p;
	for(; p != bend; ++p){
		if(*p == *crtpat){
			++crtpat;
			if(!*crtpat) break;
		}else{
			crtpat = patt;
		}
	}
	if(!*crtpat){//we've found the pattern
		return p - 2;
	}
	
	return p - (crtpat - patt);
}

/*virtual*/ void Connection::onEvent(frame::aio::ReactorContext &_rctx, frame::Event const &_revent){
	if(_revent.id == EventStartE){
		sock.postRecvSome(_rctx, bbeg, BufferCapacity, Connection::onRecv);
	}else if(_revent.id == EventStopE){
		this->postStop(_rctx);
	}
}

/*static*/ void Connection::onRecv(frame::aio::ReactorContext &_rctx, size_t _sz){
	Connection &rthis = static_cast<Connection&>(_rctx.object());
	if(!_rctx.error()){
		rthis.bend = rthis.bbeg + _sz + (rthis.crtpat - patt);
		rthis.crtpat = patt;
		rthis.bpos = rthis.bbeg;
		rthis.doRun(_rctx);
	}else{
		rthis.postStop(_rctx);
	}
}


void Connection::doRun(frame::aio::ReactorContext &_rctx){
	switch(state){
		case ReadInit:
			state = ReadCommand;
			sock.postRecvSome(_rctx, bbeg, BufferCapacity, Connection::onRecv);
			break;
		case ReadCommand:
			cmd = *bpos;
			++bpos;
			state = ReadPath;
		case ReadPath:{
			const char *p = bpos;
			for(; p != bend; ++p){
				if(*p == '\n') break;
			}
			path.append(bpos, p - bpos);
			if(*p == '\n'){
				bpos = const_cast<char *>(p);
				state = ExecCommand;
			}else{
				sock.postRecvSome(_rctx, bbeg, BufferCapacity, Connection::onRecv);
				break;
			}
		}
		case ExecCommand:
			doExecuteCommand(_rctx);
			break;
	}
}
struct OpenCbk{
	frame::Manager 		&rm;
	frame::ObjectUidT	uid;
	
	OpenCbk(
		frame::Manager &_rm,
		const frame::ObjectUidT &_robjuid
	):rm(_rm), uid(_robjuid){}
	
	void operator()(
		frame::file::Store<> &,
		frame::file::FilePointerT &_rptr,
		ERROR_NS::error_code err
	){
		idbg("");
		frame::MessagePointerT	msgptr(new FilePointerMessage(_rptr));
		rm.notify(uid, frame::Event(msgptr, EventMessageE));
	}
};

void Connection::doExecuteCommand(frame::aio::ReactorContext &_rctx){
	if(path.size() && path[path.size() - 1] == '\r'){
		path.resize(path.size() - 1);
	}
	
	frame::Manager &rm = _rctx.service().manager();
	
	switch(cmd){
		case 'R':
		case 'r':
			state = WaitRead;
			++bpos;
			idbg("Request open file: "<<path);
			filestoreptr->requestOpenFile(OpenCbk(rm, rm.id(*this)), path.c_str(), FileDevice::ReadWriteE);
			//post(_rctx, [this](frame::aio::ReactorContext &_rctx, frame::Event const &_revent){this->run(_rctx);});
			break;
		case 'W':
		case 'w':
			state = WaitWrite;
			++bpos;
			idbg("Request create file: "<<path);
			filestoreptr->requestCreateFile(OpenCbk(rm, rm.id(*this)), path.c_str(), FileDevice::ReadWriteE);
			//post(_rctx, [this](frame::aio::ReactorContext &_rctx, frame::Event const &_revent){this->run(_rctx);});
			break;
		case 'S':
		case 's':
			if(tempuid.isValid()){
				idbg("Request shared temp - no temp");
				this->postStop(_rctx);
			}else{
				idbg("Request shared temp - "<<tempuid.index<<'.'<<tempuid.unique);
				state = WaitRead;
				++bpos;
				filestoreptr->requestShared(OpenCbk(rm, rm.id(*this)), tempuid);
				//post(_rctx, [this](frame::aio::ReactorContext &_rctx, frame::Event const &_revent){this->run(_rctx);});
			}
			break;
		case 'U':
		case 'u':
			if(tempuid.isValid()){
				idbg("Request unique temp - no temp");
				postStop(_rctx);
			}else{
				idbg("Request unique temp - "<<tempuid.index<<'.'<<tempuid.unique);
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

/*static*/ void Connection::onSend(frame::aio::ReactorContext &_rctx){
	Connection &rthis = static_cast<Connection&>(_rctx.object());
}
#if 0



/*virtual*/ void Connection::execute(ExecuteContext &_rexectx){
	idbg("time.sec "<<_rexectx.currentTime().seconds()<<" time.nsec = "<<_rexectx.currentTime().nanoSeconds());
	if(_rexectx.eventMask() & (frame::EventTimeout | frame::EventDoneError | frame::EventTimeoutRecv | frame::EventTimeoutSend)){
		idbg("connecton timeout or error");
		_rexectx.close();
		return;
	}
	frame::Manager &rm = frame::Manager::specific();
	
	if(notified()){
		DynamicHandler<DynamicMapperT>	dh(dm);
		solid::ulong					sm;
		{
			Locker<Mutex>				lock(rm.mutex(*this));
			
			sm = grabSignalMask();
			if(sm & frame::S_KILL){
				_rexectx.close();
				return;
			}
			if(sm & frame::S_SIG){//we have signals
				dh.init(dv.begin(), dv.end());
				dv.clear();
			}
		}
		if(sm & frame::S_SIG){//we've grabed signals, execute them
			for(size_t i = 0; i < dh.size(); ++i){
				dh.handle(*this, i);
			}
		}
	}
	const uint32 sevs(socketEventsGrab());
	if(sevs & frame::EventDoneError){
		_rexectx.close();
		return;
	}
	if(sevs & frame::EventDoneRecv){
		bend = bbeg + socketRecvSize() + (crtpat - patt);
		crtpat = patt;
		bpos = bbeg;
	}
	
	frame::aio::AsyncE rv;
	switch(state){
		case ReadInit:
			rv = this->socketRecv(bbeg, BUFSZ);
			if(rv == frame::aio::AsyncSuccess){
				state = ReadCommand;
				bend = bbeg + socketRecvSize();
				bpos = bbeg;
			}else if(rv == frame::aio::AsyncWait){
				state = ReadCommand;
				return;
			}else{
				_rexectx.close();
				return;
			}
		case ReadCommand:
			if(socketHasPendingRecv()){
				return;//continue waiting
			}
			cmd = *bpos;
			++bpos;
			state = ReadPath;
		case ReadPath:{
			const char *p = bpos;
			for(; p != bend; ++p){
				if(*p == '\n') break;
			}
			path.append(bpos, p - bpos);
			if(*p == '\n'){
				bpos = const_cast<char *>(p);
				state = ExecCommand;
			}else{
				rv = this->socketRecv(bbeg, BUFSZ);
				if(rv == frame::aio::AsyncSuccess){
					_rexectx.reschedule();
				}else if(rv == frame::aio::AsyncWait){
					return;
				}else{
					_rexectx.close();
					return;
				}
			}
		}
		case ExecCommand:
			if(path.size() && path[path.size() - 1] == '\r'){
				path.resize(path.size() - 1);
			}
			if(cmd == 'R' || cmd == 'r'){
				state = WaitRead;
				++bpos;
				idbg("Request open file: "<<path);
				filestoreptr->requestOpenFile(OpenCbk(rm.id(*this)), path.c_str(), FileDevice::ReadWriteE);
				_rexectx.reschedule();
			}else if(cmd == 'w' || cmd == 'W'){
				state = WaitWrite;
				++bpos;
				idbg("Request create file: "<<path);
				filestoreptr->requestCreateFile(OpenCbk(rm.id(*this)), path.c_str(), FileDevice::ReadWriteE);
				_rexectx.reschedule();
			}else if(cmd == 's' || cmd == 'S'){//temp read
				if(frame::is_valid_uid(tempuid)){
					idbg("Request shared temp - no temp");
					_rexectx.close();
				}else{
					idbg("Request shared temp - "<<tempuid.first<<'.'<<tempuid.second);
					state = WaitRead;
					++bpos;
					filestoreptr->requestShared(OpenCbk(rm.id(*this)), tempuid);
					_rexectx.reschedule();
				}
			}else if(cmd == 'u' || cmd == 'U'){//temp read
				if(frame::is_valid_uid(tempuid)){
					idbg("Request unique temp - no temp");
					_rexectx.close();
				}else{
					idbg("Request unique temp - "<<tempuid.first<<'.'<<tempuid.second);
					state = WaitRead;
					++bpos;
					filestoreptr->requestShared(OpenCbk(rm.id(*this)), tempuid);
					_rexectx.reschedule();
				}
			}else if(cmd == 'T' || cmd == 't'){//temp write
				state = WaitWrite;
				++bpos;
				idbg("Request read temp");
				filestoreptr->requestCreateTemp(OpenCbk(rm.id(*this)), 4 * 1024);
				state = WaitWrite;
				_rexectx.reschedule();
			}else if(cmd == 'F' || cmd == 'f'){//temp write
				state = WaitWrite;
				++bpos;
				idbg("Request file temp");
				filestoreptr->requestCreateTemp(OpenCbk(rm.id(*this)), 4 * 1024, frame::file::VeryFastLevelFlag);
				state = WaitWrite;
				_rexectx.reschedule();
			}else if(cmd == 'm' || cmd == 'M'){//temp write
				state = WaitWrite;
				++bpos;
				idbg("Request memory temp");
				filestoreptr->requestCreateTemp(OpenCbk(rm.id(*this)), 4 * 1024, frame::file::MemoryLevelFlag);
				state = WaitWrite;
				_rexectx.reschedule();
			}else{
				_rexectx.close();
			}
			break;
		case WaitRead:
		case WaitWrite:
			//keep waiting
			idbg("keep waiting");
			break;
		case RunRead:
			if(socketHasPendingSend()){
				return;
			}
			if(!iofs.eof()){
				iofs.read(bbeg, BUFSZ);
				rv = socketSend(bbeg, iofs.gcount());
				if(rv == frame::aio::AsyncSuccess){
					_rexectx.reschedule();
				}else if(rv == frame::aio::AsyncWait){
				}else{
					_rexectx.close();
				}
			}else{
				iofs.close();
				_rexectx.close();
			}
			break;
		case RunWrite:{
			if(socketHasPendingRecv()){
				return;
			}
			const char *p = findEnd(bpos);
			iofs.write(bpos, p - bpos);
			if(crtpat != patt){
				memcpy(bbeg, patt, crtpat - patt);
			}
			if(p != bend && *p == '.'){
				iofs.close();
				_rexectx.close();
				return;
			}
			rv = socketRecv(bbeg + (crtpat - patt), BUFSZ - (crtpat - patt));
			if(rv == frame::aio::AsyncSuccess){
				bpos = bbeg;
				bend = bbeg + socketRecvSize();
				crtpat = patt;
				p = findEnd(bpos);
				iofs.write(bpos, p - bpos);
				if(crtpat != patt){
					memcpy(bbeg, patt, crtpat - patt);
				}
				if(p != bend && *p == '.'){
					iofs.close();
					_rexectx.close();
					return;
				}
				_rexectx.reschedule();
			}else if(rv == frame::aio::AsyncWait){
			}else{
				_rexectx.close();
			}
		}break;
		case CloseFileError:
			_rexectx.close();
			break;
	}
}

/*virtual*/ bool Connection::notify(solid::DynamicPointer<solid::frame::Message> &_rmsgptr){
	dv.push_back(DynamicPointer<>(_rmsgptr));
	return Object::notify(frame::S_SIG | frame::S_RAISE);
}
#endif

void Connection::dynamicHandle(solid::DynamicPointer<> &_dp){
	idbg("");
	cassert(false);
	state = CloseFileError;
}
void Connection::dynamicHandle(solid::DynamicPointer<FilePointerMessage> &_rmsgptr){
	idbg("");
	if(!_rmsgptr->ptr.empty()){
		iofs.device(_rmsgptr->ptr);
		if(state == WaitRead){
			state = RunRead;
		}else if(state == WaitWrite){
			state = RunWrite;
		}
	}else{
		state = CloseFileError;
	}
}
