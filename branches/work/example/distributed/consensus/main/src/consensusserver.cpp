#include "example/distributed/concept/server/serverobject.hpp"
#include "example/distributed/concept/core/manager.hpp"
#include "example/distributed/concept/core/signals.hpp"

#include "foundation/service.hpp"
#include "foundation/scheduler.hpp"

#include "foundation/aio/aioselector.hpp"
#include "foundation/aio/aiosingleobject.hpp"
#include "foundation/objectselector.hpp"
#include "foundation/ipc/ipcservice.hpp"

#include "algorithm/serialization/binary.hpp"
#include "algorithm/serialization/idtypemap.hpp"

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
	int			ipc_port;
	string		dbg_levels;
	string		dbg_modules;
	string		dbg_addr;
	string		dbg_port;
	bool		dbg_buffered;
	bool		log;
};

struct IpcServiceController: foundation::ipc::Service::Controller{
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
}


bool parseArguments(Params &_par, int argc, char *argv[]);

int main(int argc, char *argv[]){
	
	Params p;
	if(parseArguments(p, argc, argv)) return 0;
	
	Thread::init();
	
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
	}else{
		Dbg::instance().initFile(
			*argv[0] == '.' ? argv[0] + 2 : argv[0],
			p.dbg_buffered,
			3,
			1024 * 1024 * 64,
			&dbgout
		);
	}
	cout<<"Debug output: "<<dbgout<<endl;
	dbgout.clear();
	Dbg::instance().moduleBits(dbgout);
	cout<<"Debug modules: "<<dbgout<<endl;
	}
#endif
	{
		typedef serialization::TypeMapper					TypeMapper;
		typedef serialization::IdTypeMap					IdTypeMap;
		typedef serialization::bin::Serializer				BinSerializer;
		
		TypeMapper::registerMap<IdTypeMap>(new IdTypeMap);
		TypeMapper::registerSerializer<BinSerializer>();
		
		mapSignals();
	}
	{
		
		foundation::Manager 	m(16);
		IpcServiceController	ipcctrl;
		
		m.registerScheduler(new SchedulerT(m));
		m.registerScheduler(new AioSchedulerT(m));
		
		//const IndexT svcidx = 
		m.registerService<SchedulerT>(new foundation::Service, 0, fdt::compute_service_id(serverUid().first));
		m.registerService<SchedulerT>(new foundation::ipc::Service(&ipcctrl), 0, ipcid);
		
		m.start();
		
		if(true){
			AddrInfo ai("0.0.0.0", p.ipc_port, 0, AddrInfo::Inet4, AddrInfo::Datagram);
			foundation::ipc::Service::the().insertTalker(ai.begin());
		}
		
		foundation::ObjectPointer<ServerObject>	op(new ServerObject);
		const fdt::IndexT						svcidx(fdt::compute_service_id(serverUid().first));
		const fdt::IndexT						srvidx(fdt::compute_index(serverUid().first));
		
		m.service(svcidx).insert<SchedulerT>(op, 0, srvidx);
		
		char c;
		cout<<"> "<<flush;
		cin>>c;
		//m.stop(true);
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
			("ipc_port,i", value<int>(&_par.ipc_port)->default_value(2000),
					"Base port")
			("debug_levels,l", value<string>(&_par.dbg_levels)->default_value("iew"),"Debug logging levels")
			("debug_modules,m", value<string>(&_par.dbg_modules),"Debug logging modules")
			("debug_address,a", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")
			("debug_port,p", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")
			("debug_unbuffered,s", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered")
			("use_log,L", value<bool>(&_par.log)->implicit_value(true)->default_value(false), "Debug buffered")
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


