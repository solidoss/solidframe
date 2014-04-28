#include "frame/manager.hpp"
#include "frame/scheduler.hpp"

#include "frame/aio/aioselector.hpp"
#include "frame/aio/aioobject.hpp"

#include "frame/ipc/ipcservice.hpp"

//#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/socketaddress.hpp"

#include "boost/program_options.hpp"

#include <signal.h>
#include <iostream>

using namespace std;
using namespace solid;

typedef frame::IndexT							IndexT;
typedef frame::Scheduler<frame::aio::Selector>	AioSchedulerT;


ostream& operator<<(ostream& _ros, const SocketAddressInet4& _rsa){
	string hoststr;
	string servstr;
	_rsa.toString(hoststr, servstr, ReverseResolveInfo::NumericHost | ReverseResolveInfo::NumericService);
	_ros<<hoststr<<':'<<servstr;
	return _ros;
}

struct Params{
	typedef std::vector<std::string>						StringVectorT;
	typedef std::vector<SocketAddressInet4>					SocketAddressVectorT;
	typedef frame::ipc::Configuration::RelayAddressVectorT	RelayAddressVectorT;
	string					dbg_levels;
	string					dbg_modules;
	string					dbg_addr;
	string					dbg_port;
	bool					dbg_console;
	bool					dbg_buffered;
	
	uint16					baseport;
	uint32					netid;
	bool					log;
	StringVectorT			relaystringvec;
	string					acceptaddrstring;
    
	bool prepare(frame::ipc::Configuration &_rcfg, string &_err);
};

namespace{
	Mutex					mtx;
	Condition				cnd;
	bool					run(true);
	Params					p;
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
	if(parseArguments(p, argc, argv)) return 0;
	
	signal(SIGINT,term_handler); /* Die on SIGTERM */
	
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
		
		frame::Manager			m;
		
		AioSchedulerT			aiosched(m);
		
		frame::ipc::Service		ipcsvc(m, new frame::ipc::BasicController(aiosched));
		
		m.registerService(ipcsvc);
		
		{
			frame::ipc::Configuration	cfg;
			ResolveData					rd = synchronous_resolve("0.0.0.0", p.baseport, 0, SocketInfo::Inet4, SocketInfo::Datagram);
			int							err;
			{
				string errstr;
				if(!p.prepare(cfg, errstr)){
					cout<<"Error preparing ipc configuration: "<<errstr<<endl;
					Thread::waitAll();
					return 0;
				}
			}
			cfg.baseaddr = rd.begin();
			err = ipcsvc.reconfigure(cfg);
			if(err){
				//TODO:
				//cout<<"Error starting ipcservice: "<<err.toString()<<endl;
				cout<<"Error starting ipcservice"<<endl;
				Thread::waitAll();
				return 0;
			}
		}
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
			("base-port,b", value<uint16>(&_par.baseport)->default_value(3000), "IPC Base port")
			("accept-addr,a", value<string>(&_par.acceptaddrstring)->default_value("localhost:4000"), "IPC Accept address")
			("netid,n", value<uint32>(&_par.netid), "Network identifier")
			("relay,r", value<vector<string> >(&_par.relaystringvec), "Peer relay: netid:address:port")
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
bool Params::prepare(frame::ipc::Configuration &_rcfg, string &_err){
	const uint16	default_port = 4000;
	size_t 			posa;
	size_t 			posb;
	for(std::vector<std::string>::iterator it(relaystringvec.begin()); it != relaystringvec.end(); ++it){
		posa = it->find(':');
		posb = it->rfind(':');
		if(posa == std::string::npos){
			_err = "Error parsing relay address: ";
			_err += *it;
			return false;
		}
		
		(*it)[posa] = '\0';
		int netid = atoi(it->c_str());
		int port  = -1;
		
		if(posb == posa){
			port = default_port;
		}else{
			(*it)[posb] = '\0';
			port = atoi(it->c_str() + posb + 1);
		}
		
		const char *addr = it->c_str() + posa + 1;
		
		ResolveData	rd = synchronous_resolve(addr, port, 0, SocketInfo::Inet4, SocketInfo::Stream);
		
		if(!rd.empty()){
			_rcfg.relayaddrvec.push_back(frame::ipc::Configuration::RelayAddress());
			_rcfg.relayaddrvec.back().networkid = netid;
			_rcfg.relayaddrvec.back().address = rd.begin();
			idbg("added relay address "<<*it);
		}else{
			idbg("skiped relay address "<<*it);
		}
	}
	posb = acceptaddrstring.rfind(':');
	int port = 4000;
	if(posb != string::npos){
		acceptaddrstring[posb] = '\0';
		port = atoi(acceptaddrstring.c_str() + posb + 1);
	}
	ResolveData	rd = synchronous_resolve(acceptaddrstring.c_str(), port, 0, SocketInfo::Inet4, SocketInfo::Stream);
	if(!rd.empty()){
		_rcfg.acceptaddr = rd.begin();
	}
	_rcfg.localnetid = netid;
	return true;
}
