#include "foundation/manager.hpp"
#include "foundation/service.hpp"
#include "foundation/scheduler.hpp"
#include "foundation/objectselector.hpp"

#include "foundation/aio/aioselector.hpp"
#include "foundation/aio/aioobject.hpp"
//#include "foundation/aio/openssl/opensslsocket.hpp"

#include "foundation/ipc/ipcservice.hpp"

//#include "system/thread.hpp"
#include "system/socketaddress.hpp"
//#include "system/socketdevice.hpp"

#include "boost/program_options.hpp"

#include <iostream>

using namespace std;

namespace fdt=foundation;

typedef foundation::IndexT									IndexT;
typedef foundation::Scheduler<foundation::aio::Selector>	AioSchedulerT;
typedef foundation::Scheduler<foundation::ObjectSelector>	SchedulerT;

//------------------------------------------------------------------
//------------------------------------------------------------------

struct Params{
	typedef std::vector<std::string>	StringVectorT;
	int				start_port;
	string			dbg_levels;
	string			dbg_modules;
	string			dbg_addr;
	string			dbg_port;
	bool			dbg_console;
	bool			dbg_buffered;
	bool			log;
	StringVectorT	serverstringvec;
};

//------------------------------------------------------------------

struct IpcServiceController: foundation::ipc::Controller{
	IpcServiceController():foundation::ipc::Controller(400, 0/* AuthenticationFlag*/, 1000, 10 * 1000), authidx(0){
		use();
	}
	
	/*virtual*/ void scheduleTalker(foundation::aio::Object *_po);
	/*virtual*/ void scheduleListener(foundation::aio::Object *_po);
	/*virtual*/ void scheduleNode(foundation::aio::Object *_po);
	
	
	/*virtual*/ bool compressBuffer(
		foundation::ipc::BufferContext &_rbc,
		const uint32 _bufsz,
		char* &_rpb,
		uint32 &_bl
	);
	/*virtual*/ bool decompressBuffer(
		foundation::ipc::BufferContext &_rbc,
		char* &_rpb,
		uint32 &_bl
	);
	
	void localNetworkId(uint32 _netid){
		netid = _netid;
	}
	/*virtual*/ uint32 localNetworkId()const{
		return netid;
	}
	/*virtual*/ SocketAddressStub gatewayAddress(
		const uint _idx,
		const uint32 _netid_dest,
		const SocketAddressStub &_rsas_dest
	){
		return _rsas_dest;
	}
	
	//retval:
	// -1 : wait for asynchrounous event and retry
	// 0: no gateway
	// > 0: the count
	/*virtual*/ int gatewayCount(
		const uint32 _netid_dest,
		const SocketAddressStub &_rsas_dest
	)const{
		return 1;
	}
private:
	//qlz_state_compress		qlz_comp_ctx;
	//qlz_state_decompress	qlz_decomp_ctx;
	int						authidx;
	uint32					netid;
};

namespace{
	IpcServiceController	ipcctrl;
}

//------------------------------------------------------------------

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
	cout<<"Debug output: "<<dbgout<<endl;
	dbgout.clear();
	Dbg::instance().moduleBits(dbgout);
	cout<<"Debug modules: "<<dbgout<<endl;
	}
#endif
	
	{
		
		foundation::Manager 	m(16);
		
		//m.registerScheduler(new SchedulerT(m));
		m.registerScheduler(new AioSchedulerT(m/*, 0, 6, 1000*/));
		
		
		const IndexT svcidx = m.registerService<SchedulerT>(new foundation::Service, 0);
		
		ipcctrl.localNetworkId(0);
	
		m.registerService<SchedulerT>(new foundation::ipc::Service(ipcctrl.pointer()), 0);
	
		//fdt::ipc::Service::the().typeMapper().insert<AuthSignal>();
		
		m.start();
		
		
		char c;
		cout<<"> "<<flush;
		cin>>c;
		//m.stop(true);
	}
	Thread::waitAll();
	return 0;
}

//------------------------------------------------------------------
bool parseArguments(Params &_par, int argc, char *argv[]){
	using namespace boost::program_options;
	try{
		options_description desc("SolidFrame ipc stress test");
		desc.add_options()
			("help,h", "List program options")
			("debug_levels,L", value<string>(&_par.dbg_levels)->default_value("view"),"Debug logging levels")
			("debug_modules,M", value<string>(&_par.dbg_modules),"Debug logging modules")
			("debug_address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 9999)")
			("debug_port,P", value<string>(&_par.dbg_port)->default_value("9999"), "Debug server port (e.g. on linux use: nc -l 9999)")
			("debug_console,C", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")
			("debug_unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered")
			("use_log,L", value<bool>(&_par.log)->implicit_value(true)->default_value(false), "Debug buffered")
			("base_port,b", value<int>(&_par.start_port)->default_value(2000), "Base port")
			("connect, c", value<vector<string> >(&_par.serverstringvec), "Peer to connect to: YYY.YYY.YYY.YYY:port")
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
//------------------------------------------------------------------
//------------------------------------------------------
//		IpcServiceController
//------------------------------------------------------
void IpcServiceController::scheduleTalker(foundation::aio::Object *_po){
	idbg("");
	foundation::ObjectPointer<foundation::aio::Object> op(_po);
	AioSchedulerT::schedule(op, 0);
}

void IpcServiceController::scheduleListener(foundation::aio::Object *_po){
	idbg("");
	foundation::ObjectPointer<foundation::aio::Object> op(_po);
	AioSchedulerT::schedule(op, 0);
}
void IpcServiceController::scheduleNode(foundation::aio::Object *_po){
	idbg("");
	foundation::ObjectPointer<foundation::aio::Object> op(_po);
	AioSchedulerT::schedule(op, 0);
}

/*virtual*/ bool IpcServiceController::compressBuffer(
	foundation::ipc::BufferContext &_rbc,
	const uint32 _bufsz,
	char* &_rpb,
	uint32 &_bl
){
// 	if(_bufsz < 1024){
// 		return false;
// 	}
	return false;
	uint32	destcp(0);
	char 	*pdest =  allocateBuffer(_rbc, destcp);
	size_t	len/* = qlz_compress(_rpb, pdest, _bl, &qlz_comp_ctx)*/;
	_rpb = pdest;
	_bl = len;
	return true;
}

/*virtual*/ bool IpcServiceController::decompressBuffer(
	foundation::ipc::BufferContext &_rbc,
	char* &_rpb,
	uint32 &_bl
){
	uint32	destcp(0);
	char 	*pdest =  allocateBuffer(_rbc, destcp);
	size_t	len/* = qlz_decompress(_rpb, pdest, &qlz_decomp_ctx)*/;
	_rpb = pdest;
	_bl = len;
	return true;
}


