#include "example/consensus/server/serverobject.hpp"
#include "example/consensus/core/consensusmanager.hpp"
#include "example/consensus/core/consensusrequests.hpp"

#include "consensus/consensusregistrar.hpp"

#include "frame/service.hpp"
#include "frame/scheduler.hpp"

#include "frame/aio/aioselector.hpp"
#include "frame/aio/aiosingleobject.hpp"
#include "frame/objectselector.hpp"
#include "frame/ipc/ipcservice.hpp"

#include "system/debug.hpp"
#include "system/socketaddress.hpp"
#include <iostream>

#include "boost/program_options.hpp"

using namespace std;
using namespace solid;

typedef frame::IndexT										IndexT;
typedef frame::Scheduler<frame::aio::Selector>				AioSchedulerT;
typedef frame::Scheduler<frame::ObjectSelector>				SchedulerT;
typedef DynamicSharedPointer<ServerConfiguration>			ServerConfigurationSharedPointerT;
typedef DynamicPointer<consensus::server::Configuration>	ConfigurationPointerT;
//------------------------------------------------------------------
//------------------------------------------------------------------

struct Params{
	Params():cfgptr(new ServerConfiguration){}
	
	int									ipc_port;
	string								dbg_levels;
	string								dbg_modules;
	string								dbg_addr;
	string								dbg_port;
	bool								dbg_buffered;
	bool								dbg_console;
	bool								log;
	ServerConfigurationSharedPointerT	cfgptr;
};


bool parseArguments(Params &_par, int argc, char *argv[]);

int main(int argc, char *argv[]){
	
	Params p;
	if(parseArguments(p, argc, argv)) return 0;
	
	Thread::init();
	
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
		if(!p.cfgptr->init(p.ipc_port)){
		//cout<<"Failed ServerParams::init: "<<p.p.errorString()<<endl;
		idbg("Failed ServerParams::init: "<<p.cfgptr->errorString());
		return 0;
	}
	
		//cout<<p.p;
		idbg(*p.cfgptr);
	}
	{
		
		frame::Manager 		m;
		
		AioSchedulerT			aiosched(m);
		SchedulerT				objsched(m);
		
		frame::ipc::Service		ipcsvc(m, new frame::ipc::BasicController(aiosched));
		
		mapSignals(ipcsvc);
		ServerObject::dynamicRegister();
		ServerObject::registerMessages(ipcsvc);
		
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
		
		ConfigurationPointerT			cfgptr(p.cfgptr);
		DynamicPointer<ServerObject>	objptr(new ServerObject(ipcsvc, cfgptr));
		
		objptr->serverIndex(consensus::Registrar::the().registerObject(m.registerObject(*objptr)));
		
		objsched.schedule(objptr);
		
		char c;
		cout<<"> "<<flush;
		cin>>c;
		m.stop();
	}
	Thread::waitAll();
	return 0;
}

bool parseArguments(Params &_par, int argc, char *argv[]){
	using namespace boost::program_options;
	try{
		options_description desc("SolidFrame distributed concept server application");
		desc.add_options()
			("help,h", "List program options")
			("ipc_port,i", value<int>(&_par.ipc_port)->default_value(2000),"Base port")
			("debug_levels,L", value<string>(&_par.dbg_levels)->default_value("view"),"Debug logging levels")
			("debug_modules,M", value<string>(&_par.dbg_modules)->default_value("any"),"Debug logging modules")
			("debug_address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")
			("debug_port,P", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")
			("debug_console,C", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")
			("debug_unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered")
			("use_log,l", value<bool>(&_par.log)->implicit_value(true)->default_value(false), "Debug buffered")
			("server_addrs,a", value< vector<string> >(&_par.cfgptr->addrstrvec), "Server addresses")
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


