#include "example/consensus/client/clientobject.hpp"
#include "example/consensus/core/consensusmanager.hpp"
#include "example/consensus/core/consensusrequests.hpp"


#include "frame/service.hpp"
#include "frame/scheduler.hpp"

#include "frame/aio/aioselector.hpp"
#include "frame/aio/aiosingleobject.hpp"
#include "frame/objectselector.hpp"
#include "frame/ipc/ipcservice.hpp"

#include "system/debug.hpp"
#include "system/socketaddress.hpp"
#include <iostream>

#include <signal.h>

#include "boost/program_options.hpp"

using namespace std;
using namespace solid;


typedef frame::IndexT							IndexT;
typedef frame::Scheduler<frame::aio::Selector>	AioSchedulerT;
typedef frame::Scheduler<frame::ObjectSelector>	SchedulerT;

//------------------------------------------------------------------
//------------------------------------------------------------------

struct Params{
	Params():dbg_buffered(false), dbg_console(false){}
	int				ipc_port;
	string			dbg_levels;
	string			dbg_modules;
	string			dbg_addr;
	string			dbg_port;
	bool			dbg_buffered;
	bool			dbg_console;
	bool			log;
	ClientParams	p;
};

namespace{
	Mutex					mtx;
	Condition				cnd;
	bool					run(true);
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


bool parseArguments(Params &_par, int argc, char *argv[]);

int main(int argc, char *argv[]){
	
	Params p;
	if(parseArguments(p, argc, argv)) return 0;
	
	Thread::init();
	
	if(!p.p.init()){
		cout<<"Error: "<<p.p.errorString()<<endl;
		return 0;
	}
	
	p.p.print(cout);
	
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
	//cout<<"Debug output: "<<dbgout<<endl;
	dbgout.clear();
	//Debug::the().moduleBits(dbgout);
	//cout<<"Debug modules: "<<dbgout<<endl;
	}
#endif
	if(true){
		for(int i(0); i < argc; ++i){
			idbg("arg "<<i<<" = "<<argv[i]);
		}
		if(isdigit(argv[1][0])){
			int pos = atoi(argv[1]);
			sleep(pos + 1);
		}
	}
	
	if(true){
		
		frame::Manager 			m;
		
		AioSchedulerT			aiosched(m);
		SchedulerT				objsched(m);
		
		frame::ipc::Service		ipcsvc(m, new frame::ipc::BasicController(aiosched));
		
		//ipcsvc.typeMapper().insert<FirstMessage>();
		
		m.registerService(ipcsvc);
		
		{
			frame::ipc::Configuration	cfg;
			ResolveData					rd = synchronous_resolve("0.0.0.0", p.ipc_port, 0, SocketInfo::Inet4, SocketInfo::Datagram);
			//frame::aio::Error			err;
			bool						err;
			
			cfg.baseaddr = rd.begin();
			
			err = ipcsvc.reconfigure(cfg);
			if(!err){
				//TODO:
				//cout<<"Error starting ipcservice: "<<err.toString()<<endl;
				Thread::waitAll();
				return 0;
			}
		}
		ClientObject::dynamicRegister();
		mapSignals(ipcsvc);
		
		DynamicPointer<ClientObject> objptr(new ClientObject(p.p, ipcsvc));
	
		m.registerObject(*objptr);
		objsched.schedule(objptr);
		
		
		{
			Locker<Mutex>	lock(mtx);
			while(run){
				cnd.wait(lock);
			}
		}
	
		m.stop();
	}
	Thread::waitAll();
	return 0;
}

bool parseArguments(Params &_par, int argc, char *argv[]){
	using namespace boost::program_options;
	try{
		options_description desc("SolidFrame distributed concept client application");
		desc.add_options()
			("help,h", "List program options")
			("ipc_port,i", value<int>(&_par.ipc_port)->default_value(0),"Base port")
			("debug_levels,L", value<string>(&_par.dbg_levels)->default_value("view"),"Debug logging levels")
			("debug_modules,M", value<string>(&_par.dbg_modules)->default_value("any"),"Debug logging modules")
			("debug_address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")
			("debug_port,P", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")
			("debug_console,C", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")
			("debug_unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered")
			("use_log,l", value<bool>(&_par.log)->implicit_value(true)->default_value(false), "Use audit logging")
			("repeat_count,c", value<uint32>(&_par.p.cnt)->default_value(1), "Repeat count")
			("server_addrs,a", value< vector<string> >(&_par.p.addrstrvec), "Server addresses")
			("strings,g", value< vector<uint32> >(&_par.p.strszvec), "Generated string sizes")
			("sequence,s", value<string>(&_par.p.seqstr)->default_value("i0 i1 p100 f1 p200 e1 f1 p300 f0 E"), "Opperation sequence")
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
			cout << desc << "\n\n";
			cout<<"Examples:"<<endl<<endl;
			cout<<"$ ./example_consensus_client -a 127.0.0.1:2000 -a 127.0.0.1:3000 -a 127.0.0.1:4000 -C -M \"any\" -s \"i10\""<<endl;
			cout<<"New terminal:"<<endl;
			cout<<"$ ./example_consensus_server -i 2000 -a 127.0.0.1:2000 -a 127.0.0.1:3000 -a 127.0.0.1:4000 -C -M \"any\""<<endl;
			cout<<"New terminal:"<<endl;
			cout<<"$ ./example_consensus_server -i 3000 -a 127.0.0.1:2000 -a 127.0.0.1:3000 -a 127.0.0.1:4000 -C -M \"any\""<<endl;
			cout<<"New terminal:"<<endl;
			cout<<"$ ./example_consensus_server -i 4000 -a 127.0.0.1:2000 -a 127.0.0.1:3000 -a 127.0.0.1:4000 -C -M \"any\""<<endl;
			return true;
		}
		return false;
	}catch(exception& e){
		cout << e.what() << "\n";
		return true;
	}
}


