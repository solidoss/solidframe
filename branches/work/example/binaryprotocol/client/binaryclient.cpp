#include "protocol/binary/client/clientsession.hpp"

#include "frame/aio/aiosingleobject.hpp"
#include "frame/aio/aioselector.hpp"

#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
#include "system/thread.hpp"
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"
#include "example/binaryprotocol/core/messages.hpp"

#include <signal.h>

#include "boost/program_options.hpp"

#include <iostream>

using namespace solid;
using namespace std;

typedef frame::Scheduler<frame::aio::Selector>	AioSchedulerT;

namespace{
	struct Params{
		int			port;
		string		address_str;
		string		dbg_levels;
		string		dbg_modules;
		string		dbg_addr;
		string		dbg_port;
		bool		dbg_console;
		bool		dbg_buffered;
		bool		log;
	};

	Mutex					mtx;
	Condition				cnd;
	bool					run(true);
	Params					p;

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
	
}

class ClientConnection: public frame::aio::SingleObject{
	typedef std::vector<DynamicPointer<solid::frame::Message> > MessageVectorT;
	enum{
		RecvBufferCapacity = 1024 * 4,
		SendBufferCapacity = 1024 * 4,
	};
	enum{
		InitState = 1,
		PrepareState,
		ConnectState,
		ConnectWaitState,
		RunningState
	};
public:
	ClientConnection(frame::Manager &_rm, const ResolveData &_rd):rm(_rm), rd(_rd), st(PrepareState){}
	
	/*virtual*/ int execute(ulong _evs, TimeSpec& _crtime);
	
	void send(DynamicPointer<solid::frame::Message>	&_rmsgptr){
		Locker<Mutex>	lock(rm.mutex(*this));
		
		sndmsgvec.push_back(_rmsgptr);
		
		if(Object::notify(frame::S_SIG | frame::S_RAISE)){
			rm.raise(*this);
		}
	}
	int done(){
		idbg("");
		Locker<Mutex>	lock(mtx);
		run = false;
		cnd.signal();
		return BAD;
	}
private:
	frame::Manager						&rm;
	ResolveData							rd;
	uint16								st;
	protocol::binary::client::Session	protoses;
	MessageVectorT						sndmsgvec;
	char								recvbuf[RecvBufferCapacity];
	char								sendbuf[SendBufferCapacity];
};

/*virtual*/ int ClientConnection::execute(ulong _evs, TimeSpec& _crtime){
	ulong sm = grabSignalMask();
	if(sm){
		if(sm & frame::S_KILL) return done();
		if(sm & frame::S_SIG){
			Locker<Mutex>	lock(rm.mutex(*this));
			protoses.schedule(sndmsgvec.begin(), sndmsgvec.end());
			sndmsgvec.clear();
		}
	}
	if(_evs & frame::ERRDONE){
		idbg("ioerror "<<_evs<<' '<<socketEventsGrab());
		return done();
	}
	if(_evs & frame::INDONE){
		idbg("indone");
		if(!protoses.consume(recvbuf, this->socketRecvSize())){
			return done();
		}
	}
	bool reenter = false;
	if(st == RunningState){
		idbg("RunningState");
		if(!this->socketHasPendingRecv()){
			switch(this->socketRecv(recvbuf, RecvBufferCapacity)){
				case BAD: return done();
				case OK:
					if(!protoses.consume(recvbuf, this->socketRecvSize())){
						return done();
					}
					reenter = true;
					break;
				default:
					break;
			}
		}
		if(!this->socketHasPendingSend()){
			int cnt = 4;
			while((cnt--) > 0){
				int rv = protoses.fill(sendbuf, SendBufferCapacity);
				if(rv == BAD) return done();
				if(rv == NOK) break;
				switch(this->socketSend(sendbuf, rv)){
					case BAD: 
						return done();
					case NOK:
						cnt = 0;
						break;
					default:
						break;
				}
			}
			if(cnt == 0){
				reenter = true;
			}
		}
	}else if(st == PrepareState){
		SocketDevice	sd;
		sd.create(rd.begin());
		sd.makeNonBlocking();
		socketInsert(sd);
		socketRequestRegister();
		st = ConnectState;
		reenter = false;
	}else if(st == ConnectState){
		idbg("ConnectState");
		switch(socketConnect(rd.begin())){
			case BAD: return done();
			case OK:
				idbg("");
				st = InitState;
				reenter = true;
				break;
			case NOK:
				st = ConnectWaitState;
				idbg("");
				break;
		}
	}else if(st == ConnectWaitState){
		idbg("ConnectWaitState");
		st = InitState;
		reenter = true;
	}else if(st == InitState){
		idbg("InitState");
		st = RunningState;
		reenter = true;
	}
	return reenter ? OK : NOK;
}

bool parseArguments(Params &_par, int argc, char *argv[]);

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
	Debug::the().moduleBits(dbgout);
	cout<<"Debug modules: "<<dbgout<<endl;
	}
#endif
	
	{
		typedef DynamicSharedPointer<ClientConnection>	ClientConnectionPointerT;
		
		frame::Manager							m;
		AioSchedulerT							aiosched(m);
		
		ClientConnectionPointerT				ccptr(
			new ClientConnection(
				m, synchronous_resolve(p.address_str.c_str(), p.port, 0, SocketInfo::Inet4, SocketInfo::Stream)
			)
		);
		
		m.registerObject(*ccptr);
		aiosched.schedule(ccptr);
		
		DynamicPointer<solid::frame::Message>	msgptr;
		
		msgptr = new FirstRequest;
		
		ccptr->send(msgptr);
		
		msgptr = new SecondRequest;
		
		ccptr->send(msgptr);
		idbg("");
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


bool parseArguments(Params &_par, int argc, char *argv[]){
	using namespace boost::program_options;
	try{
		options_description desc("SolidFrame concept application");
		desc.add_options()
			("help,h", "List program options")
			("port,p", value<int>(&_par.port)->default_value(2000),"Server port")
			("address,a", value<string>(&_par.address_str)->default_value("localhost"),"Server address")
			("debug_levels,L", value<string>(&_par.dbg_levels)->default_value("view"),"Debug logging levels")
			("debug_modules,M", value<string>(&_par.dbg_modules)->default_value("any"),"Debug logging modules")
			("debug_address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")
			("debug_port,P", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")
			("debug_console,C", value<bool>(&_par.dbg_console)->implicit_value(false)->default_value(true), "Debug console")
			("debug_unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered")
			("use_log,l", value<bool>(&_par.log)->implicit_value(true)->default_value(false), "Debug buffered")
	/*		("verbose,v", po::value<int>()->implicit_value(1),
					"enable verbosity (optionally specify level)")*/
	/*		("listen,l", po::value<int>(&portnum)->implicit_value(1001)
					->default_value(0,"no"),
					"listen on a port.")
			("include-path,I", po::value< vector<string> >(),
					"include path")
			("input-file", po::value< vector<string> >(), "input file")*/
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