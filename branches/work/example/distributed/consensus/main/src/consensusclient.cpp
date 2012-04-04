#include "example/distributed/consensus/client/clientobject.hpp"
#include "example/distributed/consensus/core/consensusmanager.hpp"
#include "example/distributed/consensus/core/consensusrequests.hpp"


#include "foundation/service.hpp"
#include "foundation/scheduler.hpp"

#include "foundation/aio/aioselector.hpp"
#include "foundation/aio/aiosingleobject.hpp"
#include "foundation/objectselector.hpp"
#include "foundation/ipc/ipcservice.hpp"

#include "system/debug.hpp"
#include "system/socketaddress.hpp"
#include <iostream>

#include "boost/program_options.hpp"

using namespace std;

namespace fdt=foundation;

typedef foundation::IndexT									IndexT;
typedef foundation::Scheduler<foundation::aio::Selector>	AioSchedulerT;
typedef foundation::Scheduler<foundation::ObjectSelector>	SchedulerT;

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

struct IpcServiceController: foundation::ipc::Controller{
	void scheduleTalker(foundation::aio::Object *_po){
		idbg("");
		foundation::ObjectPointer<foundation::aio::Object> op(_po);
		AioSchedulerT::schedule(op);
	}
	bool release(){
		return false;
	}
};

namespace {
	const foundation::IndexT	ipcid(10);
	const foundation::IndexT	svcid(11);
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
	Dbg::instance().levelMask(p.dbg_levels.c_str());
	Dbg::instance().moduleMask(p.dbg_modules.c_str());
	if(p.dbg_addr.size() && p.dbg_port.size()){
		Dbg::instance().initSocket(
			p.dbg_addr.c_str(),
			p.dbg_port.c_str(),
			p.dbg_buffered,
			&dbgout
		);
	}else if(p.dbg_console){
		Dbg::instance().initStdErr(
			p.dbg_buffered,
			&dbgout
		);
	}else{
		Dbg::instance().initFile(
			*argv[0] == '.' ? argv[0] + 2 : argv[0],
			p.dbg_buffered,
			3,
			1024 * 1024 * 64,
			&dbgout
		);
	}
	//cout<<"Debug output: "<<dbgout<<endl;
	dbgout.clear();
	//Dbg::instance().moduleBits(dbgout);
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
	
	IpcServiceController	ipcctrl;
	
	if(true){
		
		foundation::Manager 	m(16);
		
		m.registerScheduler(new SchedulerT(m));
		m.registerScheduler(new AioSchedulerT(m));
		
		//const IndexT svcidx = 
		m.registerService<SchedulerT>(new foundation::Service, 0, svcid);
		m.registerService<SchedulerT>(new foundation::ipc::Service(&ipcctrl), 0, ipcid);
		
		mapSignals();
		
		m.start();
		
		if(true){
			SocketAddressInfo ai("0.0.0.0", p.ipc_port, 0, SocketAddressInfo::Inet4, SocketAddressInfo::Datagram);
			foundation::ipc::Service::the().insertTalker(ai.begin());
		}
		
		foundation::ObjectPointer<ClientObject>	op(new ClientObject(p.p));
		fdt::ObjectUidT objuid(m.service(svcid).insert<SchedulerT>(op));
		
		m.stop(true);//wait for m.signalStop(), to start stopping process
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
			("ipc_port,i", value<int>(&_par.ipc_port)->default_value(0),
					"Base port")
			("debug_levels,l", value<string>(&_par.dbg_levels)->default_value("view"),"Debug logging levels")
			("debug_modules,m", value<string>(&_par.dbg_modules)->default_value("any"),"Debug logging modules")
			("debug_address,a", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")
			("debug_port,p", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")
			("debug_console,c", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")
			("debug_unbuffered,s", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered")
			("use_log,L", value<bool>(&_par.log)->implicit_value(true)->default_value(false), "Use audit logging")
			("repeat_count,C", value<uint32>(&_par.p.cnt)->default_value(1), "Repeat count")
			("server_addrs,A", value< vector<string> >(&_par.p.addrstrvec), "Server addresses")
			("strings,G", value< vector<uint32> >(&_par.p.strszvec), "Generated string sizes")
			("sequence,S", value<string>(&_par.p.seqstr)->default_value("i0 i1 p100 f1 p200 e1 f1 p300 f0 E"), "Opperation sequence")
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


