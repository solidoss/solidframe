/* Implementation file test.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <signal.h>
#include "system/debug.hpp"
#include "system/thread.hpp"
#include "system/socketaddress.hpp"

#include "core/manager.hpp"
#include "echo/echoservice.hpp"
#include "alpha/alphaservice.hpp"
#include "beta/betaservice.hpp"
#include "proxy/proxyservice.hpp"
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
	The proof of concept test application.
	It instantiate a manager, creates some services,
	registers some listeners talkers on those services
	and offers a small CLI.
*/
// prints the CLI help
void printHelp();
// inserts a new talker
int insertTalker(char *_pc, int _len, test::Manager &_rtm);
// inserts a new connection
int insertConnection(char *_pc, int _len, test::Manager &_rtm);


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
};

bool parseArguments(Params &_par, int argc, char *argv[]);

int main(int argc, char* argv[]){
	signal(SIGPIPE, SIG_IGN);
	
	Params p;
	if(parseArguments(p, argc, argv)) return 0;
	
	cout<<"Built on SolidGround version "<<SG_MAJOR<<'.'<<SG_MINOR<<'.'<<SG_PATCH<<endl;
	
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
	lm.start();
	lm.insertChannel(new DeviceIOStream(pairfd[0], pairfd[1]));
	lm.insertListener("localhost", "3333");
	Directory::create("log");
	lm.insertConnector(new audit::LogBasicConnector("log"));
	Log::instance().reinit(argv[0], 0, "ALL", new DeviceIOStream(pairfd[1],-1));
	
	int stime;
	long ltime;

	ltime = time(NULL); /* get current calendar time */
	stime = (unsigned int) ltime/2;
	srand(stime);
	
	idbg("Built on SolidGround version "<<SG_MAJOR<<'.'<<SG_MINOR<<'.'<<SG_PATCH);
	idbg("sizeof(IndexTp) = "<<sizeof(foundation::IndexTp)<<" SERVICEBITCNT = "<<foundation::SERVICEBITCNT<<" INDEXBITCNT = "<<foundation::INDEXBITCNT);
	idbg("sizeof ulong = "<<sizeof(long));
#ifdef _LP64
	idbg("64bit architecture");
#else
	idbg("32bit architecture");
#endif

	{

		test::Manager	tm;
		if(true){// create and register the echo service
			test::Service* psrvc = test::echo::Service::create();
			tm.insertService("echo", psrvc);
			
			{//add a new listener
				int port = p.start_port + 111;
				AddrInfo ai("0.0.0.0", port, 0, AddrInfo::Inet4, AddrInfo::Stream);
				if(!ai.empty()){
					if(!tm.insertListener("echo", ai.begin())){
						cout<<"added listener to echo "<<port<<endl;
					}else{
						cout<<"failed adding listener for service echo port "<<port<<endl;
					}
				}else{
					cout<<"failed create address for port "<<port<<" listener not created"<<endl;
				}
			}
			{//add a new talker
				int port = p.start_port + 112;
				AddrInfo ai("0.0.0.0", port, 0, AddrInfo::Inet4, AddrInfo::Datagram);
				if(!ai.empty()){
					AddrInfoIterator it(ai.begin());
					if(!tm.insertTalker("echo", ai.begin())){
						cout<<"added talker to echo "<<port<<endl;
					}else{
						cout<<"failed creating udp listener station "<<port<<endl;
					}
				}else{
					cout<<"empty addr info - no listener talker"<<endl;
				}
			}
		}
		if(true){//creates and registers a new beta service
			test::Service* psrvc = test::beta::Service::create();
			tm.insertService("beta", psrvc);
			int port = p.start_port + 113;
			AddrInfo ai("0.0.0.0", port, 0, AddrInfo::Inet4, AddrInfo::Datagram);
			if(!ai.empty()){//ands a talker
				if(!tm.insertTalker("beta", ai.begin())){
					cout<<"added talker to beta "<<port<<endl;
				}else{
					cout<<"failed creating udp listener station "<<port<<endl;
				}
			}else{
				cout<<"empty addr info - no listener talker"<<endl;
			}
		}
		if(true){//creates and registers a new alpha service
			test::Service* psrvc = test::alpha::Service::create(tm);
			tm.insertService("alpha", psrvc);
			{
				int port = p.start_port + 114;
				AddrInfo ai("0.0.0.0", port, 0, AddrInfo::Inet4, AddrInfo::Stream);
				if(!ai.empty() && !tm.insertListener("alpha", ai.begin())){//adds a listener
					cout<<"added listener for service alpha "<<port<<endl;
				}else{
					cout<<"failed adding listener for service alpha port "<<port<<endl;
				}
			}
			{
				int port = p.start_port + 124;
				AddrInfo ai("0.0.0.0", port, 0, AddrInfo::Inet4, AddrInfo::Stream);
				if(!ai.empty() && !tm.insertListener("alpha", ai.begin(), true)){//adds a listener
					cout<<"added listener for service alpha "<<port<<endl;
				}else{
					cout<<"failed adding listener for service alpha port "<<port<<endl;
				}
			}
		}
		if(true){// create and register the proxy service
			test::Service* psrvc = test::proxy::Service::create();
			tm.insertService("proxy", psrvc);
			
			{//add a new listener
				int port = p.start_port + 115;
				AddrInfo ai("0.0.0.0", port, 0, AddrInfo::Inet4, AddrInfo::Stream);
				if(!ai.empty()){
					if(!tm.insertListener("proxy", ai.begin())){
						cout<<"added listener to proxy "<<port<<endl;
					}else{
						cout<<"failed adding listener for service proxy port "<<port<<endl;
					}
				}else{
					cout<<"failed create address for port "<<port<<" listener not created"<<endl;
				}
			}
		}
		if(true){//adds the base ipc talker
			int port = p.start_port + 222;
			AddrInfo ai("0.0.0.0", port, 0, AddrInfo::Inet4, AddrInfo::Datagram);
			if(!ai.empty()){
				if(!tm.insertIpcTalker(ai.begin())){
					cout<<"added talker to ipc "<<port<<endl;
				}else{
					cout<<"failed creating udp listener station "<<port<<endl;
				}
			}else{
				cout<<"empty addr info - no listener talker"<<endl;
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
			if(!strncasecmp(buf,"quit",4)){
				tm.stop();
				lm.stop();
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
				rc = insertTalker(buf + 9,cin.gcount() - 9,tm);
				continue;
			}
			if(!strncasecmp(buf,"addconnection", 13)){
				rc = insertConnection(buf + 13, cin.gcount() - 13, tm);
				continue;
			}
			cout<<"Error parsing command line"<<endl;
		}
	}
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

int insertTalker(char *_pc, int _len,test::Manager &_rtm){
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
int insertConnection(char *_pc, int _len,test::Manager &_rtm){
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
	if(ai.empty() || _rtm.insertConnection(srvname.c_str(), ai.begin(), node.c_str(), srv.c_str())){
		cout<<"Failed adding connection"<<endl;
	}
	//TODO:
// 	if(_rts.insertConnection(srvname.c_str(), new fdt::tcp::Channel, node.c_str(), srv.c_str())){
// 		cout<<"Failed adding connection"<<endl;
// 	}
	return 0;
}

// bool parseArguments(Params &_par, int argc, char *argv[]){
// 	try {  
// 
// 		TCLAP::CmdLine cmd("SolidGround test application", ' ', "0.8");
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
		options_description desc("SolidGround test application");
		desc.add_options()
			("help,h", "List program options")
			("base_port,b", value<int>(&_par.start_port)->default_value(1000),
					"Base port")
			("debug_levels,l", value<string>(&_par.dbg_levels)->default_value("iew"),"Debug logging levels")
			("debug_modules,m", value<string>(&_par.dbg_modules),"Debug logging modules")
			("debug_address,a", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")
			("debug_port,p", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")
			("debug_buffered,s", value<bool>(&_par.dbg_buffered)->default_value(true), "Debug buffered")
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

