#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
#include "frame/objectselector.hpp"

#include "frame/aio/aioselector.hpp"
#include "frame/aio/aiosingleobject.hpp"
#include "frame/aio/openssl/opensslsocket.hpp"
#include "frame/message.hpp"

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

using namespace std;
using namespace solid;

typedef frame::IndexT							IndexT;
typedef frame::Scheduler<frame::aio::Selector>	AioSchedulerT;
typedef frame::Scheduler<frame::ObjectSelector>	SchedulerT;

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
	
	typedef DynamicSharedPointer<frame::file::Store<> >	FileStoreSharedPointerT;
	
	FileStoreSharedPointerT		filestoreptr;

}//namespace

//------------------------------------------------------------------
//------------------------------------------------------------------

class Listener: public Dynamic<Listener, frame::aio::SingleObject>{
public:
	Listener(
		frame::Manager &_rm,
		AioSchedulerT &_rsched,
		const SocketDevice &_rsd
	);
	~Listener();
private:
	/*virtual*/ void execute(ExecuteContext &_rexectx);
	
	int					state;
	SocketDevice		sd;
	frame::Manager		&rm;
	AioSchedulerT		&rsched;
};

//------------------------------------------------------------------
//------------------------------------------------------------------

struct FilePointerMessage;

class Connection: public Dynamic<Connection, frame::aio::SingleObject>{
	typedef solid::DynamicMapper<void, Connection>	DynamicMapperT;
	typedef std::vector<solid::DynamicPointer<> >	DynamicPointerVectorT;
	typedef frame::file::FileIOStream< 1024  >		IOFileStreamT;
	struct DynamicRegister{
		DynamicRegister(DynamicMapperT &_rdm);
	};
public:
	Connection(const char *_node, const char *_srv);
	Connection(const SocketDevice &_rsd);
	~Connection();
	
	void dynamicHandle(solid::DynamicPointer<> &_dp);
	void dynamicHandle(solid::DynamicPointer<FilePointerMessage> &_rmsgptr);
private:
	/*virtual*/ bool notify(solid::DynamicPointer<solid::frame::Message> &_rmsgptr);
	/*virtual*/ void execute(ExecuteContext &_rexectx);
	const char * findEnd(const char *_p);
private:
	enum {BUFSZ = 1024};
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
	int							state;
	char						bbeg[BUFSZ];
	char 						*bpos;
	char						*bend;
	ResolveData					rd;
	ResolveIterator				it;
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
void insertListener(frame::Manager &_rm, AioSchedulerT &_rsched, const char *_addr, int _port);
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
		frame::Manager	m;
		AioSchedulerT	aiosched(m);
		SchedulerT		sched(m);
		
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
		
			filestoreptr = new frame::file::Store<>(m, utf8cfg, tempcfg);
		}
		
		m.registerObject(*filestoreptr);
		sched.schedule(filestoreptr);
		
		insertListener(m, aiosched, "0.0.0.0", p.start_port);
		
		
		
		cout<<"Here some examples how to test: "<<endl;
		cout<<"\t$ nc localhost 2000"<<endl;
		
		if(0){
			Locker<Mutex>	lock(mtx);
			while(run){
				cnd.wait(lock);
			}
		}else{
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

void insertListener(frame::Manager& _rm, AioSchedulerT& _rsched, const char* _addr, int _port){
	ResolveData		rd =  synchronous_resolve(_addr, _port, 0, SocketInfo::Inet4, SocketInfo::Stream);
	
	SocketDevice	sd;
	
	sd.create(rd.begin());
	sd.makeNonBlocking();
	sd.prepareAccept(rd.begin(), 100);
	if(!sd.ok()){
		cout<<"error creating listener"<<endl;
		return;
	}
	
	DynamicPointer<Listener> lsnptr(new Listener(_rm, _rsched, sd));
	
	_rm.registerObject(*lsnptr);
	_rsched.schedule(lsnptr);
	
	cout<<"inserted listener on port "<<_port<<endl;
}
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
Listener::Listener(
	frame::Manager& _rm,
	AioSchedulerT& _rsched,
	const SocketDevice& _rsd
):BaseT(_rsd, true), rm(_rm), rsched(_rsched){
	state = 0;
}

Listener::~Listener(){
}

/*virtual*/ void Listener::execute(ExecuteContext &_rexectx){
	idbg("here");
	cassert(this->socketOk());
	if(notified()){
		//Locker<Mutex>	lock(this->mutex());
		solid::ulong sm = this->grabSignalMask();
		if(sm & frame::S_KILL){
			_rexectx.close();
			return;
		}
	}
	solid::uint cnt(10);
	while(cnt--){
		if(state == 0){
			switch(this->socketAccept(sd)){
				case frame::aio::AsyncError:
					_rexectx.close();
					return;
				case frame::aio::AsyncSuccess:break;
				case frame::aio::AsyncWait:
					state = 1;
					return;
			}
		}
		state = 0;
		cassert(sd.ok());
		DynamicPointer<Connection> conptr(new Connection(sd));
		rm.registerObject(*conptr);
		rsched.schedule(conptr);
	}
	_rexectx.reschedule();
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

static frame::UidT							tempuid = frame::invalid_uid();

Connection::Connection(const char *_node, const char *_srv): 
	BaseT(), b(false), crtpat(patt)
{
	cassert(_node && _srv);
	rd = synchronous_resolve(_node, _srv);
	it = rd.begin();
	state = ReadInit;
	bend = bbeg;
	
}
Connection::Connection(const SocketDevice &_rsd):
	BaseT(_rsd), b(false), crtpat(patt)
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

struct OpenCbk{
	frame::ObjectUidT	uid;
	OpenCbk(){}
	OpenCbk(const frame::ObjectUidT &_robjuid):uid(_robjuid){}
	
	void operator()(
		frame::file::Store<> &,
		frame::file::FilePointerT &_rptr,
		ERROR_NS::error_code err
	){
		idbg("");
		tempuid = _rptr.id();
		frame::Manager 			&rm = frame::Manager::specific();
		frame::MessagePointerT	msgptr(new FilePointerMessage(_rptr));
		rm.notify(msgptr, uid);
	}
};

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
