#include "foundation/manager.hpp"
#include "foundation/service.hpp"
#include "foundation/scheduler.hpp"

#include "foundation/aio/aioselector.hpp"
#include "foundation/objectselector.hpp"
#include "foundation/aio/aioobject.hpp"
#include "foundation/ipc/ipcservice.hpp"

#include "system/thread.hpp"
#include "system/socketaddress.hpp"

#include "boost/program_options.hpp"

#include <iostream>
using namespace std;

namespace {
	foundation::IndexT			ipcid(10);
	const foundation::IndexT	firstid(11);
	const foundation::IndexT	secondid(14);
	const foundation::IndexT	thirdid(12);
}


struct ThirdObject: Dynamic<ThirdObject, foundation::Object>{
	ThirdObject(uint _cnt, uint _waitsec):cnt(_cnt), waitsec(_waitsec){}
protected:
	/*virtual*/ int execute(ulong _evs, TimeSpec &_rtout);
private:
	uint	cnt;
	uint	waitsec;
};

typedef foundation::Scheduler<foundation::aio::Selector>	AioSchedulerT;
typedef foundation::Scheduler<foundation::ObjectSelector>	SchedulerT;

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

struct Params{
	int			start_port;
	string		dbg_levels;
	string		dbg_modules;
	string		dbg_addr;
	string		dbg_port;
	bool		dbg_buffered;
	bool		log;
};

bool parseArguments(Params &_par, int argc, char *argv[]);

int main(int argc, char *argv[]){
	
	Params p;
	if(parseArguments(p, argc, argv)) return 0;
	
	Thread::init();
	
#ifdef UDEBUG
	{
	string dbgout;
	Dbg::instance().levelMask("view");
	Dbg::instance().moduleMask("all");
	Dbg::instance().initStdErr(false);
	}
#endif
	
	{
		
		foundation::Manager 	m(16);
		
		IpcServiceController	ipcctrl;
		
		//AioSchedulerT	*pais = new AioSchedulerT(m);
		//SchedulerT		*ps1 = new SchedulerT(m);
		//SchedulerT		*ps2 = new SchedulerT(m);
		
		m.registerScheduler(new SchedulerT(m));
		m.registerScheduler(new SchedulerT(m));
		m.registerScheduler(new AioSchedulerT(m));
		
		
		m.registerService<SchedulerT>(new foundation::Service, 0, firstid);
		m.registerService<SchedulerT>(new foundation::Service, 0, secondid);
		ipcid = m.registerService<SchedulerT>(new foundation::ipc::Service(&ipcctrl));
		//m.registerObject(new ThirdObject(0, 10), thirdid);
		
		m.start();
		
		if(true){
			SocketAddressInfo ai("0.0.0.0", p.start_port, 0, SocketAddressInfo::Inet4, SocketAddressInfo::Datagram);
			foundation::ipc::Service::the().insertTalker(ai.begin());
		}
		
		//m.service(firstid).start();
		//m.service(secondid).start<SchedulerT>();
		//m.service(ipcid).start<SchedulerT>(1);
		//SchedulerT::schedule(foundation::ObjectPointer<>(m.objectPointer(thirdid)), 0);
		//m.startObject<SchedulerT>(thirdid);

		if(true){
			//ThirdObject	*po(new ThirdObject(10, 1));
			foundation::ObjectPointer<ThirdObject>	op(new ThirdObject(10, 1));
			m.service(firstid).insert<SchedulerT>(op);
			//SchedulerT::schedule(foundation::ObjectPointer<>(po), 0);
		}
		
		if(true){
			ThirdObject	*po(new ThirdObject(20, 2));
			m.service(secondid).insert(po);
			SchedulerT::schedule(foundation::ObjectPointer<>(po), 1);
		}
		
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
		options_description desc("SolidFrame concept application");
		desc.add_options()
			("help,h", "List program options")
			("base_port,b", value<int>(&_par.start_port)->default_value(2000),
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

/*virtual*/ int ThirdObject::execute(ulong _evs, TimeSpec &_rtout){
	ulong sm(0);
	if(signaled()){
		Locker<Mutex> lock(mutex());
		sm = grabSignalMask();
		if(sm & foundation::S_KILL){
			idbg("killing thirdobject");
			return BAD;
		}
	}
	idbg("event on "<<id()<<" remain count "<<cnt<<" wait sec "<<waitsec);
	_rtout.add(waitsec);
	if(cnt == 0) return NOK;
	--cnt;
	if(cnt == 0){
		return BAD;
	}
	return NOK;
}
