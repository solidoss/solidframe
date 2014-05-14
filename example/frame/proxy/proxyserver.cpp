#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
#include "frame/objectselector.hpp"

#include "frame/aio/aioselector.hpp"
#include "frame/aio/aiomultiobject.hpp"
#include "frame/aio/aiosingleobject.hpp"
#include "frame/aio/openssl/opensslsocket.hpp"

#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"

#include "boost/program_options.hpp"

#include <signal.h>
#include <iostream>

using namespace std;
using namespace solid;

typedef frame::IndexT							IndexT;
typedef frame::Scheduler<frame::aio::Selector>	AioSchedulerT;
typedef frame::Scheduler<frame::ObjectSelector>	SchedulerT;

//------------------------------------------------------------------
//------------------------------------------------------------------

struct Params{
	typedef std::vector<std::string>			StringVectorT;
	typedef std::vector<SocketAddressInet>		PeerAddressVectorT;
	string					dbg_levels;
	string					dbg_modules;
	string					dbg_addr;
	string					dbg_port;
	bool					dbg_console;
	bool					dbg_buffered;
	
	int						baseport;
	bool					log;
	StringVectorT			connectstringvec;
	
	PeerAddressVectorT		connectvec;
    
	bool prepare();
};

namespace{
	Mutex					mtx;
	Condition				cnd;
	Params					p;
	bool					run(true);
	SocketDevice			crtsd;
}

static void term_handler(int signum){
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
//------------------------------------------------------------------
//------------------------------------------------------------------

class Listener: public Dynamic<Listener, frame::aio::SingleObject>{
public:
	Listener(frame::Manager &_rm, AioSchedulerT &_rsched, const SocketDevice &_rsd);
	~Listener();
private:
	/*virtual*/ void execute(ExecuteContext &_rexectx);
	int					state;
	SocketDevice		sd;
	frame::Manager		&rm;
	AioSchedulerT		&rsched;
	frame::ObjectUidT	conid;
};

//------------------------------------------------------------------
//------------------------------------------------------------------
class MultiConnection: public solid::Dynamic<MultiConnection, solid::frame::aio::MultiObject>{
public:
	MultiConnection();
	MultiConnection(const solid::SocketDevice &_rsd);
	~MultiConnection();
private:
	/*virtual*/ void execute(ExecuteContext &_rexectx);
	solid::AsyncE doReadAddress();
	solid::AsyncE doProxy(const solid::TimeSpec &_tout);
	solid::AsyncE doRefill();
	void state(int _st){
		st  = _st;
	}
	int state()const{
		return st;
	}
private:
	enum{
		Proxy,
		InitConnectFirst,
		ConnectFirst,
		WaitConnectFirst,
		InitConnectSecond,
		ConnectSecond,
		WaitConnectSecond,
		WaitSecond
	};
	struct Buffer{
		enum{
			Capacity = 2*1024
		};
		Buffer():size(0), usecnt(0){}
		char		data[Capacity];
		unsigned	size;
		unsigned	usecnt;
	};
	struct Stub{
		Buffer			recvbuf;
	};
	int						st;
	Stub					stubs[2];
	char*					bp;
	char*					be;
};

//------------------------------------------------------------------
//------------------------------------------------------------------

bool parseArguments(Params &_par, int argc, char *argv[]);
void insertListener(frame::Manager &_rm, AioSchedulerT &_rsched);
void insertConnection(frame::Manager &_rm, AioSchedulerT &_rsched);
//------------------------------------------------------------------
//------------------------------------------------------------------

int main(int argc, char *argv[]){
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
	p.prepare();
	
	{
		frame::Manager	m;
		AioSchedulerT	aiosched(m);
		
		if(p.baseport > 0){
			insertListener(m, aiosched);
		}else{
			cassert(p.connectvec.size() == 2);
			insertConnection(m, aiosched);
		}
		
		if(1){
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

void insertListener(frame::Manager &_rm, AioSchedulerT &_rsched){
	ResolveData		rd =  synchronous_resolve("0.0.0.0", p.baseport, 0, SocketInfo::Inet4, SocketInfo::Stream);
	
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
	
	cout<<"inserted listener on port "<<p.baseport<<endl;
}
//------------------------------------------------------------------
void insertConnection(frame::Manager &_rm, AioSchedulerT &_rsched){
	DynamicPointer<MultiConnection> conptr(new MultiConnection);
	
	_rm.registerObject(*conptr);
	_rsched.schedule(conptr);
}
//------------------------------------------------------------------
//------------------------------------------------------------------
bool parseArguments(Params &_par, int argc, char *argv[]){
	using namespace boost::program_options;
	try{
		options_description desc("SolidFrame ipc stress test");
		desc.add_options()
			("help,h", "List program options")
			("debug-levels,L", value<string>(&_par.dbg_levels)->default_value("view"),"Debug logging levels")
			("debug-modules,M", value<string>(&_par.dbg_modules),"Debug logging modules")
			("debug-address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 9999)")
			("debug-port,P", value<string>(&_par.dbg_port)->default_value("9999"), "Debug server port (e.g. on linux use: nc -l 9999)")
			("debug-console,C", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")
			("debug-unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered")
			("listen-port,l", value<int>(&_par.baseport)->default_value(-1), "IPC Listen port")
			("connect,c", value<vector<string> >(&_par.connectstringvec), "Peer to connect to: YYY.YYY.YYY.YYY:port")
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
//------------------------------------------------------
bool Params::prepare(){
	const uint16	default_port = 2000;
	size_t			pos;
	
	for(std::vector<std::string>::iterator it(connectstringvec.begin()); it != connectstringvec.end(); ++it){
		pos = it->rfind(':');
		if(pos == std::string::npos){
			connectvec.push_back(SocketAddressInet(it->c_str(), default_port));
		}else{
			(*it)[pos] = '\0';
			int port = atoi(it->c_str() + pos + 1);
			connectvec.push_back(SocketAddressInet(it->c_str(), port));
		}
		idbg("added connect address "<<connectvec.back());
	}

	return true;
}
//------------------------------------------------------------------
//------------------------------------------------------------------
Listener::Listener(
	frame::Manager &_rm,
	AioSchedulerT &_rsched,
	const SocketDevice &_rsd
):BaseT(_rsd, true), rm(_rm), rsched(_rsched),conid(frame::invalid_uid()){
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
		if(frame::is_invalid_uid(conid)){
			DynamicPointer<MultiConnection> conptr(new MultiConnection(sd));
			conid = rm.registerObject(*conptr);
			rsched.schedule(conptr);
		}else{
			crtsd = sd;
			rm.notify(frame::S_RAISE, conid);
		}
	}
	_rexectx.reschedule();
}

//------------------------------------------------------------------
//------------------------------------------------------------------
MultiConnection::MultiConnection()
{
	bp = be = NULL;
	state(InitConnectFirst);
}
MultiConnection::MultiConnection(const SocketDevice &_rsd):
	BaseT(_rsd)
{
	bp = be = NULL;
	if(p.connectvec.size()){
		state(InitConnectSecond);
	}else{
		state(WaitSecond);
	}
}
MultiConnection::~MultiConnection(){
}
enum{
	Receive,
	ReceiveWait,
	Send,
	SendWait
};
/*virtual*/ void MultiConnection::execute(ExecuteContext &_rexectx){
	idbg("time.sec "<<_rexectx.currentTime().seconds()<<" time.nsec = "<<_rexectx.currentTime().nanoSeconds());
	if(_rexectx.eventMask() & (frame::EventTimeout | frame::EventDoneError | frame::EventTimeoutRecv | frame::EventTimeoutSend)){
		idbg("connecton timeout or error : tout "<<frame::EventTimeout<<" err "<<frame::EventDoneError<<" toutrecv "<<frame::EventTimeoutRecv<<" tout send "<<frame::EventTimeoutSend);
		//We really need this check as epoll upsets if we register an unconnected socket
		if(state() != WaitConnectFirst && state() != WaitConnectSecond){
			//lets see which socket has timeout:
			while(this->signaledSize()){
				solid::uint evs = socketEvents(signaledFront());
				idbg("for "<<signaledFront()<<" evs "<<evs);
				this->signaledPop();
			}
			_rexectx.close();
			return;
		}
	}
	if(notified()){
		frame::Manager &rm = frame::Manager::specific();
		Locker<Mutex>	lock(rm.mutex(*this));
		solid::ulong sm = grabSignalMask();
		if(sm & frame::S_KILL){
			_rexectx.close();
			return;
		}
		if(sm & frame::S_RAISE && state() == WaitSecond){
			socketRequestRegister(socketInsert(crtsd));
			socketState(0, Receive);
			socketState(1, Receive);
			state(Proxy);
			return;
		}
	}
	AsyncE rv = AsyncWait;
	switch(state()){
		case Proxy:
			idbgx(Debug::any, "PROXY");
			rv =  doProxy(_rexectx.currentTime());
			break;
		case InitConnectFirst:{
			idbgx(Debug::any, "InitConnectFirst");
			SocketDevice	sd;
			sd.create();
			sd.makeNonBlocking();
			socketRequestRegister(socketInsert(sd));
			state(ConnectFirst);
			}return;
		case ConnectFirst:
			while(this->signaledSize()){
				this->signaledPop();
			}
			idbgx(Debug::any, "ConnectFirst");
			switch(socketConnect(0, p.connectvec[0])){
				case frame::aio::AsyncError:
					idbgx(Debug::any, "Failure");
					_rexectx.close();
					return;
				case frame::aio::AsyncSuccess:
					state(InitConnectSecond);
					idbgx(Debug::any, "Success");
					_rexectx.reschedule();
					return;
				case frame::aio::AsyncWait:
					state(WaitConnectFirst);
					idbgx(Debug::any, "Wait");
					return;
			};
		case WaitConnectFirst:{
			idbgx(Debug::any, "WaitConnectFirst");
			uint32 evs = socketEvents(0);
			if(!evs || !(evs & frame::EventDoneSend)){
				_rexectx.close();
				return;
			}
			state(InitConnectSecond);
		}
		case InitConnectSecond:{
			idbgx(Debug::any, "InitConnectSecond");
			SocketDevice	sd;
			sd.create();
			sd.makeNonBlocking();
			socketRequestRegister(socketInsert(sd));
			state(ConnectSecond);
			}return;
		case ConnectSecond:
			while(this->signaledSize()){
				this->signaledPop();
			}
			idbgx(Debug::any, "ConnectSecond");
			switch(socketConnect(1, p.connectvec[p.connectvec.size() == 1 ? 0 : 1])){
				case frame::aio::AsyncError:
					idbgx(Debug::any, "Failure");
					_rexectx.close();
					return;
				case frame::aio::AsyncSuccess:
					state(Proxy);
					idbgx(Debug::any, "Success");
					_rexectx.reschedule();
					return;
				case frame::aio::AsyncWait:
					state(WaitConnectSecond);
					idbgx(Debug::any, "Wait");
					return;
			};
		case WaitConnectSecond:{
			idbgx(Debug::any, "WaitConnectSecond");
			uint32 evs = socketEvents(1);
			socketState(0, Receive);
			socketState(1, Receive);
			if(!evs || !(evs & frame::EventDoneSend)){
				_rexectx.close();
				return;
			}
			state(Proxy);
			_rexectx.reschedule();
		}
	}
	while(this->signaledSize()){
		this->signaledPop();
	}
	if(rv == solid::AsyncError){
		_rexectx.close();
	}else if(rv == solid::AsyncSuccess){
		_rexectx.reschedule();
	}
}
AsyncE MultiConnection::doProxy(const TimeSpec &_tout){
	AsyncE retv = solid::AsyncWait;
	if((socketEvents(0) & frame::EventDoneError) || (socketEvents(1) & frame::EventDoneError)){
		idbg("bad errdone "<<socketEvents(1)<<' '<<frame::EventDoneError);
		return solid::AsyncError;
	}
	switch(socketState(0)){
		case Receive:
			idbg("receive 0");
			switch(socketRecv(0, stubs[0].recvbuf.data, Buffer::Capacity)){
				case frame::aio::AsyncError:
					idbg("bad recv 0");
					return solid::AsyncError;
				case frame::aio::AsyncSuccess:
					idbg("receive ok 0");
					socketState(0, Send);
					retv = solid::AsyncSuccess;
					break;
				case frame::aio::AsyncWait:
					idbg("receive nok 0");
					socketTimeoutRecv(0, 30);
					socketState(0, ReceiveWait);
					break;
			}
			break;
		case ReceiveWait:
			idbg("receivewait 0");
			if(socketEvents(0) & frame::EventDoneRecv){
				socketState(0, Send);
			}else break;
		case Send:
			idbg("send 0");
			switch(socketSend(1, stubs[0].recvbuf.data, socketRecvSize(0))){
				case frame::aio::AsyncError:
					return solid::AsyncError;
				case frame::aio::AsyncSuccess:
					idbg("send ok 0");
					socketState(0, Receive);
					retv = solid::AsyncSuccess;
					break;
				case frame::aio::AsyncWait:
					idbg("send nok 0");
					socketState(0, SendWait);
					socketTimeoutSend(1, 50);
					break;
			}
			break;
		case SendWait:
			idbg("sendwait 0");
			if(socketEvents(1) & frame::EventDoneSend){
				socketState(0, Receive);
			}
			break;
	}
	
	switch(socketState(1)){
		idbg("receive 1");
		case Receive:
			switch(socketRecv(1, stubs[1].recvbuf.data, Buffer::Capacity)){
				case frame::aio::AsyncError:
					idbg("bad recv 1");
					return solid::AsyncError;
				case frame::aio::AsyncSuccess:
					idbg("receive ok 1");
					socketState(1, Send);
					retv = solid::AsyncSuccess;
					break;
				case frame::aio::AsyncWait:
					idbg("receive nok 1");
					socketTimeoutRecv(1, 50);
					socketState(1, ReceiveWait);
					break;
			}
			break;
		case ReceiveWait:
			idbg("receivewait 1");
			if(socketEvents(1) & frame::EventDoneRecv){
				socketState(1, Send);
			}else break;
		case Send:
			idbg("send 1");
			switch(socketSend(0, stubs[1].recvbuf.data, socketRecvSize(1))){
				case frame::aio::AsyncError:
					idbg("bad recv 1");
					return solid::AsyncError;
				case frame::aio::AsyncSuccess:
					idbg("send ok 1");
					socketState(1, Receive);
					retv = solid::AsyncSuccess;
					break;
				case frame::aio::AsyncWait:
					idbg("send nok 1");
					socketState(1, SendWait);
					socketTimeoutSend(0, 30);
					break;
			}
			break;
		case SendWait:
			idbg("sendwait 1");
			if(socketEvents(0) & frame::EventDoneSend){
				socketState(1, Receive);
				retv = solid::AsyncSuccess;
			}
			break;
	}
	idbg("retv "<<retv);
	return retv;
}

AsyncE MultiConnection::doRefill(){
	idbgx(Debug::any, "");
	if(bp == NULL){//we need to issue a read
		switch(socketRecv(0, stubs[0].recvbuf.data, Buffer::Capacity)){
			case frame::aio::AsyncError:	return solid::AsyncError;
			case frame::aio::AsyncSuccess:
				bp = stubs[0].recvbuf.data;
				be = stubs[0].recvbuf.data + socketRecvSize(0);
				return solid::AsyncSuccess;
			case frame::aio::AsyncWait:
				be = NULL;
				bp = stubs[0].recvbuf.data;
				idbgx(Debug::any, "NOK");
				return solid::AsyncWait;
		}
	}
	if(be == NULL){
		if(socketEvents(0) & frame::EventDoneRecv){
			be = stubs[0].recvbuf.data + socketRecvSize(0);
		}else{
			idbgx(Debug::any, "Wait");
			return solid::AsyncWait;
		}
	}
	return solid::AsyncSuccess;
}
//------------------------------------------------------------------
