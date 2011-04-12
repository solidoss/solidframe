/* Implementation file concept.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <signal.h>
#include "system/debug.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/thread.hpp"
#include "system/socketaddress.hpp"

#include "core/manager.hpp"
//#include "echo/echoservice.hpp"
#include "alpha/alphaservice.hpp"
//#include "beta/betaservice.hpp"
#include "proxy/proxyservice.hpp"
#include "gamma/gammaservice.hpp"
#include "audit/log/logmanager.hpp"
#include "audit/log/logconnectors.hpp"
#include "audit/log.hpp"
#include "utility/iostream.hpp"
#include "system/directory.hpp"

#include "foundation/ipc/ipcservice.hpp"

#include "boost/program_options.hpp"


namespace fdt = foundation;
using namespace std;

/*
	The proof of concept application.
	It instantiate a manager, creates some services,
	registers some listeners talkers on those services
	and offers a small CLI.
*/
// prints the CLI help
void printHelp();
// inserts a new talker
int insertTalker(char *_pc, int _len, concept::Manager &_rtm);
// inserts a new connection
int insertConnection(char *_pc, int _len, concept::Manager &_rtm);


struct DeviceIOStream: IOStream{
	DeviceIOStream(int _d, int _pd):d(_d), pd(_pd){}
	void close(){
		int tmp = d;
		d = -1;
		if(pd > 0){
			::close(tmp);
			::close(pd);
		}
	}
	/*virtual*/ int read(char *_pb, uint32 _bl, uint32 _flags = 0){
		int rv = ::read(d, _pb, _bl);
		return rv;
	}
	/*virtual*/ int write(const char *_pb, uint32 _bl, uint32 _flags = 0){
		return ::write(d, _pb, _bl);
	}
	int64 seek(int64, SeekRef){
		return -1;
	}
	int d;
	int pd;
};

int pairfd[2];

struct Params{
	int			start_port;
	string		dbg_levels;
	string		dbg_modules;
	string		dbg_addr;
	string		dbg_port;
	bool		dbg_buffered;
	bool		log;
};

struct SignalResultWaiter{
	SignalResultWaiter():s(false){}
	void prepare(){
		s = false;
	}
	void signal(int _v){
		Mutex::Locker lock(m);
		v = _v;
		s = true;
		c.signal();
	}
	int wait(){
		Mutex::Locker lock(m);
		while(!s){
			c.wait(m);
		}
		return v;
	}
	Mutex		m;
	Condition	c;
	int			v;
	bool		s;
};

struct AddrInfoSignal: concept::AddrInfoSignal{
	AddrInfoSignal(uint32 _v, SignalResultWaiter &_rwait):concept::AddrInfoSignal(_v), pwait(&_rwait){}
	~AddrInfoSignal(){
		if(pwait)
			pwait->signal(-2);
	}
	void result(int _rv){
		pwait->signal(_rv);
		pwait = NULL;
	}
	SignalResultWaiter	*pwait;
};

bool parseArguments(Params &_par, int argc, char *argv[]);

void insertListener(SignalResultWaiter &_rw, const char *_name, IndexT _idx, const char *_addr, int _port, bool _secure);

int main(int argc, char* argv[]){
	signal(SIGPIPE, SIG_IGN);
	
	Params p;
	if(parseArguments(p, argc, argv)) return 0;
	
	cout<<"Built on SolidFrame version "<<SF_MAJOR<<'.'<<SF_MINOR<<'.'<<SF_PATCH<<endl;
	
	//this must be called from the main thread
	//so that the main thread can also have specific data
	//like any other threads from solidground::system::thread
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
	
	pipe(pairfd);
	audit::LogManager lm;
	if(p.log){
		lm.start();
		lm.insertChannel(new DeviceIOStream(pairfd[0], pairfd[1]));
		lm.insertListener("localhost", "3333");
		Directory::create("log");
		lm.insertConnector(new audit::LogBasicConnector("log"));
		Log::instance().reinit(argv[0], Log::AllLevels, "ALL", new DeviceIOStream(pairfd[1],-1));
	}
	int stime;
	long ltime;

	ltime = time(NULL); /* get current calendar time */
	stime = (unsigned int) ltime/2;
	srand(stime);
	
	idbg("Built on SolidFrame version "<<SF_MAJOR<<'.'<<SF_MINOR<<'.'<<SF_PATCH);
	idbg("sizeof(IndexT) = "<<sizeof(foundation::IndexT)<<" SERVICEBITCNT = "<<foundation::SERVICEBITCNT<<" INDEXBITCNT = "<<foundation::INDEXBITCNT);
	idbg("sizeof ulong = "<<sizeof(long));
#ifdef _LP64
	idbg("64bit architecture");
#else
	idbg("32bit architecture");
#endif

	{

		concept::Manager	m;
		SignalResultWaiter	rw;
		int 				rv;
		
		const IndexT alphaidx = m.registerService<concept::SchedulerT>(concept::alpha::Service::create(m));
		const IndexT proxyidx = m.registerService<concept::SchedulerT>(concept::proxy::Service::create());
		const IndexT gammaidx = m.registerService<concept::SchedulerT>(concept::gamma::Service::create());
		
		m.start();
		
		if(true){
			int port = p.start_port + 222;
			AddrInfo ai("0.0.0.0", port, 0, AddrInfo::Inet4, AddrInfo::Datagram);
			if(!ai.empty() && !(rv = foundation::ipc::Service::the().insertTalker(ai.begin()))){
				cout<<"[ipc] Added talker on port "<<port<<endl;
			}else{
				cout<<"[ipc] Failed adding talker on port "<<port<<" rv = "<<rv<<endl;
			}
		}
		
		if(true){//creates and registers a new alpha service
			insertListener(rw, "alpha", alphaidx, "0.0.0.0", p.start_port + 114, false);
			insertListener(rw, "alpha", alphaidx, "0.0.0.0", p.start_port + 124, true);
		}
		
		if(true){//creates and registers a new alpha service
			insertListener(rw, "proxy", proxyidx, "0.0.0.0", p.start_port + 214, false);
			//insertListener(rw, "alpha", alphaidx, "0.0.0.0", p.start_port + 124, true);
		}
		
		if(true){//creates and registers a new alpha service
			insertListener(rw, "gamma", gammaidx, "0.0.0.0", p.start_port + 314, false);
			//insertListener(rw, "alpha", alphaidx, "0.0.0.0", p.start_port + 124, true);
		}
		
		char buf[2048];
		int rc = 0;
		// the small CLI loop
		while(true){
			if(rc == -1){
				cout<<"Error: Parsing command line"<<endl;
			}
			if(rc == 1){
				cout<<"Error: executing command"<<endl;
			}
			rc = 0;
			cout<<'>';cin.getline(buf,2048);
			if(!strcasecmp(buf,"quit") || !strcasecmp(buf,"q")){
				m.stop();
				cout<<"signalled to stop"<<endl;
				break;
			}
			if(!strncasecmp(buf,"fetchobj",8)){
				//rc = fetchobj(buf + 8,cin.gcount() - 8,tm,ti);
				continue;
			}
			if(!strncasecmp(buf,"printobj",8)){
				//rc = printobj(buf + 8,cin.gcount() - 8,tm,ti);
				continue;
			}
			if(!strncasecmp(buf,"signalobj",9)){
				//rc = signalobj(buf + 9,cin.gcount() - 9,tm,ti);
				continue;
			}
			if(!strncasecmp(buf,"help",4)){
				printHelp();
				continue;
			}
			if(!strncasecmp(buf,"addtalker",9)){
				rc = insertTalker(buf + 9,cin.gcount() - 9,m);
				continue;
			}
			if(!strncasecmp(buf,"addconnection", 13)){
				rc = insertConnection(buf + 13, cin.gcount() - 13, m);
				continue;
			}
			cout<<"Error parsing command line"<<endl;
		}
	}
	lm.stop();
	Thread::waitAll();
	return 0;
}

void printHelp(){
	cout<<"Server commands:"<<endl;
	cout<<"[quit]:\tStops the server and exits the application"<<endl;
	cout<<"[help]:\tPrint this text"<<endl;
	cout<<"[fetchobj SP service_name SP object_type]: Fetches the list of object_type (int) objects from service named service_name (string)"<<endl;
	cout<<"[printobj]: Prints the list of objects previously fetched"<<endl;
	cout<<"[signalobj SP fetch_list_index SP signal]: Signals an object in the list previuously fetched"<<endl;
}

void insertListener(SignalResultWaiter &_rw, const char *_name, IndexT _idx, const char *_addr, int _port, bool _secure){
	concept::AddrInfoSignal *psig(new AddrInfoSignal(_secure? concept::Service::AddSslListener : concept::Service::AddListener, _rw));

	psig->init(_addr, _port, 0, AddrInfo::Inet4, AddrInfo::Stream);
	DynamicPointer<foundation::Signal> dp(psig);
	_rw.prepare();
	concept::m().signalService(_idx, dp);
	int rv;
	switch((rv = _rw.wait())){
		case -2:
			cout<<"["<<_name<<"] No such service"<<endl;
			break;
		case true:
			cout<<"["<<_name<<"] Added listener on port "<<_port<<endl;
			break;
		default:
			cout<<"["<<_name<<"] Failed adding listener on port "<<_port<<" rv = "<<rv<<endl;
	}
}
//>fetchobj servicename type
/*
int fetchobj(char *_pc, int _len,TestServer &_rts,TheInspector &_rti){
	if(*_pc != ' ') return -1;
	++_pc;
	string srvname;
	while(*_pc != ' ' && *_pc != 0){
		srvname += *_pc;
		++_pc;
	}
	_rti.inspect(_rts, srvname.c_str());
	_rti.print();
	return 0;
}
int printobj(char *_pc, int _len, TestServer &_rts, TheInspector &_rti){
	_rti.print();
	return OK;
}
int signalobj(char *_pc, int _len,TestServer &_rts,TheInspector &_rti){
	if(*_pc != ' ') return -1;
	++_pc;
	
	long idx = strtol(_pc,&_pc,10);
	if(*_pc != ' ') return -1;
	++_pc;
	long sig = strtol(_pc,&_pc,10);
	_rti.signal(_rts,idx,sig);
	return OK;
}
*/

int insertTalker(char *_pc, int _len,concept::Manager &_rtm){
	if(*_pc != ' ') return -1;
	++_pc;
	string srvname;
	while(*_pc != ' ' && *_pc != 0){
		srvname += *_pc;
		++_pc;
	}
	if(*_pc != ' ') return -1;
	++_pc;
	string node;
	while(*_pc != ' ' && *_pc != 0){
		node += *_pc;
		++_pc;
	}
	if(*_pc != ' ') return -1;
	++_pc;
	string srv;
	while(*_pc != ' ' && *_pc != 0){
		srv += *_pc;
		++_pc;
	}
	//TODO:
// 	if(_rts.insertTalker(srvname.c_str(), fdt::udp::Station::create(), node.c_str(), srv.c_str())){
// 		cout<<"Failed adding talker"<<endl;
// 	}
	return 0;
}
int insertConnection(char *_pc, int _len,concept::Manager &_rtm){
	if(*_pc != ' ') return -1;
	++_pc;
	string srvname;
	while(*_pc != ' ' && *_pc != 0){
		srvname += *_pc;
		++_pc;
	}
	if(*_pc != ' ') return -1;
	++_pc;
	string node;
	while(*_pc != ' ' && *_pc != 0){
		node += *_pc;
		++_pc;
	}
	if(*_pc != ' ') return -1;
	++_pc;
	string srv;
	while(*_pc != ' ' && *_pc != 0){
		srv += *_pc;
		++_pc;
	}
	AddrInfo ai("0.0.0.0","");
// 	if(ai.empty() || _rtm.insertConnection(srvname.c_str(), ai.begin(), node.c_str(), srv.c_str())){
// 		cout<<"Failed adding connection"<<endl;
// 	}
	//TODO:
// 	if(_rts.insertConnection(srvname.c_str(), new fdt::tcp::Channel, node.c_str(), srv.c_str())){
// 		cout<<"Failed adding connection"<<endl;
// 	}
	return 0;
}

// bool parseArguments(Params &_par, int argc, char *argv[]){
// 	try {  
// 
// 		TCLAP::CmdLine cmd("SolidFrame concept application", ' ', "0.8");
// 		
// 		TCLAP::ValueArg<uint16> port("b","base_port","Base port",false,1000,"integer");
// 		
// 		TCLAP::ValueArg<std::string> lvls("l","debug_levels","Debug logging levels",false,"","string");
// 		TCLAP::ValueArg<std::string> mdls("m","debug_modules","Debug logging modules",false,"","string");
// 		TCLAP::ValueArg<std::string> da("a","debug_address","Debug server address",false,"","string");
// 		TCLAP::ValueArg<std::string> dp("p","debug_port","Debug server ports",false,"","string");
// 		TCLAP::SwitchArg dl("s","debug_buffered", "Debug buffered output", false);
// 	
// 	
// 		cmd.add(port);
// 		cmd.add(lvls);
// 		cmd.add(mdls);
// 		cmd.add(da);
// 		cmd.add(dp);
// 		cmd.add(dl);
// 	
// 		// Parse the argv array.
// 		cmd.parse( argc, argv );
// 	
// 		// Get the value parsed by each arg. 
// 		_par.dbg_levels = lvls.getValue();
// 		_par.start_port = port.getValue();
// 		_par.dbg_modules = mdls.getValue();
// 		_par.dbg_addr = da.getValue();
// 		_par.dbg_port = dp.getValue();
// 		_par.dbg_buffered = dl.getValue();
// 		return false;
// 	}catch (TCLAP::ArgException &e){// catch any exceptions
// 		std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
// 		return true;
// 	}
// 
// }
bool parseArguments(Params &_par, int argc, char *argv[]){
	using namespace boost::program_options;
	try{
		options_description desc("SolidFrame concept application");
		desc.add_options()
			("help,h", "List program options")
			("base_port,b", value<int>(&_par.start_port)->default_value(1000),
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

