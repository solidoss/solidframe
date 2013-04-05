/* Implementation file concept.cpp
	
	Copyright 2007, 2008, 2013 Valentin Palade 
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
#include "alpha/alphaservice.hpp"
#include "beta/betaservice.hpp"
#include "beta/betamessages.hpp"
#include "proxy/proxyservice.hpp"
#include "gamma/gammaservice.hpp"
#include "audit/log/logmanager.hpp"
#include "audit/log/logconnectors.hpp"
#include "audit/log.hpp"
#include "utility/iostream.hpp"
#include "system/directory.hpp"

#include "frame/ipc/ipcservice.hpp"

#include "boost/program_options.hpp"


using namespace std;
using namespace solid;

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


struct DeviceInputOutputStream: InputOutputStream{
	DeviceInputOutputStream(int _d, int _pd):d(_d), pd(_pd){}
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
	uint32		network_id;
	string		dbg_levels;
	string		dbg_modules;
	string		dbg_addr;
	string		dbg_port;
	bool		dbg_buffered;
	bool		dbg_console;
	bool		log;
};

struct ResultWaiter: concept::beta::SignalWaiter{
	ResultWaiter():s(false){}
	void prepare(){
		s = false;
	}
	void signal(const ObjectUidT &_v){
		Locker<Mutex> lock(m);
		v = _v;
		s = true;
		c.signal();
	}
	void signal(){
		Locker<Mutex> lock(m);
		s = true;
		c.signal();
	}
	ObjectUidT wait(){
		Locker<Mutex> lock(m);
		while(!s){
			c.wait(lock);
		}
		return v;
	}
	Mutex		m;
	Condition	c;
	ObjectUidT	v;
	bool		s;
};


bool parseArguments(Params &_par, int argc, char *argv[]);

bool insertListener(
	concept::Service &_rsvc,
	const char *_name,
	const char *_addr,
	int _port,
	bool _secure
);

void insertConnection(
	concept::beta::Service &_rsvc,
	const char *_name,
	const char *_addr,
	const char *_port,
	bool _secure
);

int insertConnection(
	concept::beta::Service &_rsvc,
	const char *_name,
	char *_pc,
	int _len
);

int sendBetaLogin(concept::Manager& _rm, ResultWaiter &_rw, char *_pc, int _len);

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
	
	pipe(pairfd);
	audit::LogManager lm;
	if(p.log){
		lm.start();
		lm.insertChannel(new DeviceInputOutputStream(pairfd[0], pairfd[1]));
		lm.insertListener("localhost", "3333");
		Directory::create("log");
		lm.insertConnector(new audit::LogBasicConnector("log"));
		solid::Log::the().reinit(argv[0], Log::AllLevels, "ALL", new DeviceInputOutputStream(pairfd[1],-1));
	}
	int stime;
	long ltime;

	ltime = time(NULL); /* get current calendar time */
	stime = (unsigned int) ltime/2;
	srand(stime);
	
	idbg("Built on SolidFrame version "<<SF_MAJOR<<'.'<<SF_MINOR<<'.'<<SF_PATCH);
	
	idbg("sizeof ulong = "<<sizeof(long));
#ifdef _LP64
	idbg("64bit architecture");
#else
	idbg("32bit architecture");
#endif

	do{

		concept::Manager			m;
		concept::alpha::Service		alphasvc(m);
		concept::proxy::Service		proxysvc(m);
		concept::gamma::Service		gammasvc(m);
		concept::beta::Service		betasvc(m);
		ResultWaiter				rw;
		
		m.start();
		
		m.registerService(alphasvc);
		m.registerService(proxysvc);
		m.registerService(gammasvc);
		m.registerService(betasvc);
		
		if(true){
			int port = p.start_port + 222;
			frame::ipc::Configuration	cfg;
			ResolveData					rd = synchronous_resolve("0.0.0.0", port, 0, SocketInfo::Inet4, SocketInfo::Datagram);
			int							err;
			
			cfg.baseaddr = rd.begin();
			
			err = m.ipc().reconfigure(cfg);
			
			if(err){
				cout<<"Error configuring ipcservice: "/*<<err.toString()*/<<endl;
				m.stop();
				break;
			}
		}
		
		if(true){//creates and registers a new alpha service
			if(!insertListener(alphasvc, "alpha", "0.0.0.0", p.start_port + 114, false)){
				m.stop();
				break;
			}
			if(!insertListener(alphasvc, "alpha", "0.0.0.0", p.start_port + 124, true)){
				m.stop();
				break;
			}
		}
		
		if(true){//creates and registers a new alpha service
			if(!insertListener(proxysvc, "proxy", "0.0.0.0", p.start_port + 214, false)){
				m.stop();
				break;
			}
		}
		
		if(true){//creates and registers a new alpha service
			if(!insertListener(gammasvc, "gamma", "0.0.0.0", p.start_port + 314, false)){
				m.stop();
				break;
			}
		}
		
		if(true){//creates and registers a new alpha service
			if(!insertListener(betasvc, "beta", "0.0.0.0", p.start_port + 414, false)){
				m.stop();
				break;
			}
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
			if(!strncasecmp(buf,"help",4)){
				printHelp();
				continue;
			}
			if(!strncasecmp(buf,"addbetaconnection", 17)){
				rc = insertConnection(betasvc, "beta", buf + 17, cin.gcount() - 17);
				continue;
			}
			if(!strncasecmp(buf,"betalogin", 9)){
				rc = sendBetaLogin(m, rw, buf + 9, cin.gcount() - 9);
				continue;
			}
			cout<<"Error parsing command line"<<endl;
		}
	}while(false);
	lm.stop();
	Thread::waitAll();
	return 0;
}

void printHelp(){
	cout<<"Server commands:"<<endl;
	cout<<"[quit]:\tStops the server and exits the application"<<endl;
	cout<<"[help]:\tPrint this text"<<endl;
	cout<<"[addbetaconnection SP addr SP port]: adds new alpha connection"<<endl;
	cout<<"[betalogin SP user SP pass]: send a beta::login command"<<endl;
	cout<<"[betacancel SP tag]: send a beta::cancel command"<<endl;
}

bool insertListener(
	concept::Service &_rsvc,
	const char *_name,
	const char *_addr,
	int _port,
	bool _secure
){
	ResolveData		rd = synchronous_resolve(_addr, _port, 0, SocketInfo::Inet4, SocketInfo::Stream);
	
	if(!_rsvc.insertListener(rd, _secure)){
		cout<<"["<<_name<<"] Failed adding listener on port "<<_port<<endl;
		return false;
	}else{
		cout<<"["<<_name<<"] Added listener on port "<<_port<<" objid = "<<endl;
		return true;
	}
}

static frame::ObjectUidT crtcon(frame::invalid_uid());

void insertConnection(
	concept::beta::Service &_rsvc,
	const char *_name,
	const char *_addr,
	const char *_port,
	bool _secure
){
	ResolveData		rd = synchronous_resolve(_addr, _port, 0, SocketInfo::Inet4, SocketInfo::Stream);
	
	crtcon = _rsvc.insertConnection(rd, NULL, _secure);
}

int insertConnection(
	concept::beta::Service &_rsvc,
	const char *_name,
	char *_pc,
	int _len
){
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
	insertConnection(_rsvc, _name, node.c_str(), srv.c_str(), false);
	return 0;
}

int sendBetaLogin(concept::Manager &_rm, ResultWaiter &_rw, char *_pc, int _len){
	if(*_pc != ' ') return -1;
	++_pc;
	string user;
	while(*_pc != ' ' && *_pc != 0){
		user += *_pc;
		++_pc;
	}
	if(*_pc != ' ') return -1;
	++_pc;
	string pass;
	while(*_pc != ' ' && *_pc != 0){
		pass += *_pc;
		++_pc;
	}
	
	_rw.prepare();
	
	concept::beta::LoginMessage		login(_rw);
	DynamicPointer<frame::Message>	dp(&login);
	login.user = user;
	login.pass = pass;
	
	_rm.notify(dp, crtcon);
	
	_rw.wait();
	
	cout<<_rw.oss.str()<<endl;
	return 0;
}

bool parseArguments(Params &_par, int argc, char *argv[]){
	using namespace boost::program_options;
	try{
		options_description desc("SolidFrame concept application");
		desc.add_options()
			("help,h", "List program options")
			("base_port,b", value<int>(&_par.start_port)->default_value(1000),"Base port")
			("network_id,n", value<uint32>(&_par.network_id)->default_value(0), "Network id")
			("debug_levels,L", value<string>(&_par.dbg_levels)->default_value("view"),"Debug logging levels")
			("debug_modules,M", value<string>(&_par.dbg_modules),"Debug logging modules")
			("debug_address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")
			("debug_port,P", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")
			("debug_console,C", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")
			("debug_unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(true)->default_value(false), "Debug unbuffered")
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

