#include "dchatstressservice.hpp"
#include "dchatmessagematrix.hpp"

#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
#include "frame/aio/aiosingleobject.hpp"

#include "system/thread.hpp"
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"
#include "system/cstring.hpp"
#include "serialization/idtypemapper.hpp"
#include "serialization/binary.hpp"

#include "example/dchat/core/compressor.hpp"

#include "cliparser.hpp"

#include <stack>

#include <signal.h>

#include "boost/program_options.hpp"

#include <deque>
#include <iostream>

using namespace solid;
using namespace std;

namespace{
	typedef DynamicPointer<Service>		ServicePointerT;
	typedef std::deque<ServicePointerT>	ServiceVectorT;
	struct Params{
		string				dbg_levels;
		string				dbg_modules;
		string				dbg_addr;
		string				dbg_port;
		bool				dbg_console;
		bool				dbg_buffered;
		bool				log;
		string				user_prefix;
		vector<string>		endpoint_str_vec;
		AddressVectorT		endpoint_vec;
		
		bool prepare();
	};

	Mutex					mtx;
	Condition				cnd;
	bool					running(true);
	Params					p;
	string					prefix;
	MessageMatrix			&mm = *(new MessageMatrix);
	ServiceVectorT			svcvec;
	AioSchedulerT			*paio;
	
	
	static void term_handler(int signum){
		switch(signum) {
			case SIGINT:
			case SIGTERM:{
				if(running){
					
					Locker<Mutex>  lock(mtx);
					running = false;
					cnd.broadcast();
				}
			}
		}
	}
	
	bool split_endpoint_string(std::string &_endpoint, std::string &_addr, int &_port){
		size_t			pos= _endpoint.rfind(':');
		if(pos == std::string::npos){
		}else{
			_endpoint[pos] = '\0';
			_port = atoi(_endpoint.c_str() + pos + 1);
			_addr = _endpoint.c_str();
			_endpoint[pos] = ':';
		}
		return true;
	}
}

bool parseArguments(Params &_par, int argc, char *argv[]);

void cliRun();

int main(int argc, char *argv[]){
	if(parseArguments(p, argc, argv)) return 0;
	signal(SIGINT,term_handler); /* Die on SIGTERM */
	/*solid::*/Thread::init();
	p.prepare();
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
			1024 * 1024 * 1,
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
		frame::Manager							m;
		AioSchedulerT							aiosched(m);
		
		paio = &aiosched;
		
		Service::registerMessages();
		
		cliRun();
		
		idbg("");
		
		m.stop();
		
	}
	/*solid::*/Thread::waitAll();
	delete &mm;
	return 0;
}

//-----------------------------------------------------------------------

bool parseArguments(Params &_par, int argc, char *argv[]){
	using namespace boost::program_options;
	try{
		options_description desc("SolidFrame dchat stress client application");
		desc.add_options()
			("help,h", "List program options")
			("server,e", value<vector<string> >(&_par.endpoint_str_vec),"Server endpoints")
			("user_prefix", value<string>(&_par.user_prefix)->default_value("user"), "Login user prefix")
			("debug_levels,L", value<string>(&_par.dbg_levels)->default_value("view"),"Debug logging levels")
			("debug_modules,M", value<string>(&_par.dbg_modules)->default_value(""),"Debug logging modules")
			("debug_address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")
			("debug_port,P", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")
			("debug_console,C", value<bool>(&_par.dbg_console)->implicit_value(false)->default_value(true), "Debug console")
			("debug_unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(false)->default_value(true), "Debug unbuffered")
			("use_log,l", value<bool>(&_par.log)->implicit_value(true)->default_value(false), "Debug buffered")
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
bool Params::prepare(){
	string	addrstr;
	int		port;
	for(vector<string>::iterator it(endpoint_str_vec.begin()); it != endpoint_str_vec.end(); ++it){
		addrstr.clear();
		port = 0;
		split_endpoint_string(*it, addrstr, port);
		
		ResolveData		rd =  synchronous_resolve(addrstr.c_str(), port, 0, SocketInfo::Inet4, SocketInfo::Stream);
		if(rd.empty()){
			cout<<"Error resolving address: "<<*it<<endl;
			return false;
		}
		endpoint_vec.push_back(SocketAddressInet(rd.begin()));
	}
	return true;
}
//---------------------------------------------------------------------------------
#define STRING_AND_SIZE(s) s, (sizeof(s) - 1)
#define STRING_SIZE(s) (sizeof(s) - 1)

struct CommandStub{
	CommandStub(
		uint32 _cnt = 0,
		uint32 _cmdidx = 0
	):cnt(_cnt), cmdidx(_cmdidx){}
	
	uint32    cnt;
	uint32    cmdidx;
};

typedef std::deque<std::string>		StringDequeT;
typedef std::stack<CommandStub>		CommandStackT;

void executeHelp();
int executeSleep(const char* _pb, int _b);
int executeWait(const char* _pb, int _b);
int executeSend(const char* _pb, int _b);
int executeCreateMessageRow(const char* _pb, int _b);
int executePrintMessageRow(const char* _pb, int _b);
int executeCreateConnectionSet(const char* _pb, int _b);

void cliRun(){
	char            buf[2048];
	int             rc = 0;
	int             len = 0;
	const char      *pbuf = NULL;
	StringDequeT    cmdvec;
	uint32_t        cmdidx = 0;
	CommandStackT   cmdstk;
	
	// the small CLI loop
	while(true){
		if(rc == -1){
			cout<<"Error: Parsing command line"<<endl;
		}
		if(rc == 1){
			cout<<"Error: executing command"<<endl;
		}
		if(rc == -2){
			cout<<"Exit on error"<<endl;
			break;
		}
		rc = 0;
		
		if(cmdidx < cmdvec.size()){
			pbuf = cmdvec[cmdidx].c_str();
		}else{
			cout<<'>';cin.getline(buf,2048);
			buf[cin.gcount()] = 0;
			pbuf = buf;
			cmdvec.push_back(buf);
		}
		
		++cmdidx;
		
		if(!cstring::casecmp(pbuf,"quit") || !cstring::casecmp(pbuf,"q")){
			break;
		}
		if(!cstring::ncasecmp(pbuf, STRING_AND_SIZE("loop"))){
			cli::Parser	mp(pbuf + STRING_SIZE("loop"));
			int			count;

			mp.skipWhites();
			if(mp.isAtEnd()){
				cout<<"ERROR: expected loop count"<<endl;
				continue;
			}
			mp.parse(count);

			cmdstk.push(CommandStub(count, cmdidx));

			continue;
		}

		if(!cstring::ncasecmp(pbuf, STRING_AND_SIZE("endloop"))){
			cout<<"Done ENDLOOP "<<cmdstk.top().cmdidx<<' '<<cmdstk.top().cnt<<endl;
			--cmdstk.top().cnt;
			if(cmdstk.size() == 1 && !running){//outer most loop
				cmdstk.top().cnt = 0;
			}
			if(cmdstk.top().cnt == 0){
				cmdstk.pop();
			}else{
				cmdidx = cmdstk.top().cmdidx;
			}
			continue;
		}
		
		if(!cstring::ncasecmp(pbuf, STRING_AND_SIZE("help"))){
			executeHelp();
			continue;
		}
		if(!cstring::ncasecmp(pbuf, STRING_AND_SIZE("noop"))){
			if(cmdstk.size()){
				cout<<"Done NOOP "<<cmdstk.top().cmdidx<<endl;
			}else{
				cout<<"Done NOOP"<<endl;
			}
			continue;
		}
		if(!cstring::ncasecmp(pbuf, STRING_AND_SIZE("sleep"))){
			len = STRING_SIZE("sleep");
			rc = executeSleep(pbuf + len, cin.gcount() - len);
			continue;
		}
		if(!cstring::ncasecmp(pbuf, STRING_AND_SIZE("cmr"))){
			len = STRING_SIZE("cmr");
			rc = executeCreateMessageRow(pbuf + len, cin.gcount() - len);
			continue;
		}
		if(!cstring::ncasecmp(pbuf, STRING_AND_SIZE("pmr"))){
			len = STRING_SIZE("pmr");
			rc = executePrintMessageRow(pbuf + len, cin.gcount() - len);
			continue;
		}
		if(!cstring::ncasecmp(pbuf, STRING_AND_SIZE("ccs"))){
			len = STRING_SIZE("ccs");
			rc = executeCreateConnectionSet(pbuf + len, cin.gcount() - len);
			continue;
		}
		if(!cstring::ncasecmp(pbuf, STRING_AND_SIZE("wait"))){
			len = STRING_SIZE("wait");
			rc = executeWait(pbuf + len, cin.gcount() - len);
			continue;
		}
		if(!cstring::ncasecmp(pbuf, STRING_AND_SIZE("send"))){
			len = STRING_SIZE("send");
			rc = executeSend(pbuf + len, cin.gcount() - len);
			continue;
		}
		cout<<"Error: parsing command line"<<endl;
	}
}

void executeHelp(){
	cout<<"Available commands:"<<endl;
	cout<<"\tSLEEP"<<endl;
	cout<<"\t\tSLEEP 60"<<endl;
	cout<<"\t\t\tSleeps for 60 seconds"<<endl;
	cout<<"\t\tSLEEP 10 100"<<endl;
	cout<<"\t\t\tSleeps for 10 seconds and 100 milliseconds"<<endl;
	cout<<endl;
}
int executeSleep(const char* _pb, int _b){
	cli::Parser		mp(_pb);
	int				sec;
	int				msec = 0;

	mp.skipWhites();
	if(mp.isAtEnd()) return -1;
	mp.parse(sec);
	if(!mp.isAtEnd()){
		mp.parse(msec);
	}
	
	cout<<"SLEEP sec = "<<sec<<" msec = "<<msec<<endl;
#ifdef __WIN32__
	Sleep(sec * 1000 + msec);
#else
	sleep(sec);
	Thread::sleep(msec);
#endif
	cout<<"Done SLEEP"<<endl;
    return 0;
}
int executeCreateMessageRow(const char* _pb, int _b){
	cli::Parser		mp(_pb);
	int				row_idx;
	int				row_cnt;
	int				from_sz;
	int				to_sz;
	
	mp.skipWhites();
	if(mp.isAtEnd()) return -1;
	mp.parse(row_idx);
	mp.skipWhites();
	if(mp.isAtEnd()) return -1;
	mp.parse(row_cnt);
	mp.skipWhites();
	if(mp.isAtEnd()) return -1;
	mp.parse(from_sz);
	mp.skipWhites();
	if(mp.isAtEnd()) return -1;
	mp.parse(to_sz);
	
	cout<<"CREATE_MESSAGE_ROW row_idx = "<<row_idx<<" row_cnt = "<<row_cnt<<" from_sz = "<<from_sz<<" to_sz = "<<to_sz<<endl;
	
	mm.createRow(row_idx, row_cnt, from_sz, to_sz);
	
	cout<<"Done CREATE_MESSAGE_ROW"<<endl;
	return 0;
}

int executePrintMessageRow(const char* _pb, int _b){
	cli::Parser		mp(_pb);
	int				row_idx;
	int				verbose = 0;
	mp.skipWhites();
	if(mp.isAtEnd()) return -1;
	mp.parse(row_idx);
	mp.skipWhites();
	if(!mp.isAtEnd()){
		mp.parse(verbose);
	}
	cout<<"PRINT_MESSAGE_ROW row_idx = "<<row_idx<<" verbose = "<<verbose<<endl;
	
	mm.printRowInfo(cout, row_idx, verbose == 1);
	
	cout<<"Done PRINT_MESSAGE_ROW"<<endl;
	return 0;
}

int executeCreateConnectionSet(const char* _pb, int _b){
	cli::Parser		mp(_pb);
	int				idx;
	int				cnt = 10;
	int				retryconnectcnt = 0;
	int				retryconnectsleepms = 1000;
	mp.skipWhites();
	if(mp.isAtEnd()) return -1;
	mp.parse(idx);
	mp.skipWhites();
	if(!mp.isAtEnd()){
		mp.parse(cnt);
	}
	mp.skipWhites();
	if(!mp.isAtEnd()){
		mp.parse(retryconnectcnt);
	}
	mp.skipWhites();
	if(!mp.isAtEnd()){
		mp.parse(retryconnectsleepms);
	}
	if(idx >= svcvec.size()){
		svcvec.resize(idx + 1);
	}
	
	cout<<"CREATE_CONNECTION_SET idx = "<<idx<<" cnt = "<<cnt<<" retryconnectcnt = "<<retryconnectcnt<<" retryconnectsleepms = "<<retryconnectsleepms<<endl;
	
	svcvec[idx] = new Service(frame::Manager::specific(), *paio, p.endpoint_vec, idx);
	
	StartData sd;
	sd.concnt = cnt;
	sd.reconnectcnt = retryconnectcnt;
	sd.reconnectsleepmsec = retryconnectsleepms;
	
	svcvec[idx]->start(sd);
	
	cout<<"Done CREATE_CONNECTION_SET"<<endl;
	return 0;
}
int executeWait(const char* _pb, int _b){
	cli::Parser		mp(_pb);
	int				idx;
	char			choice;
	
	mp.skipWhites();
	if(mp.isAtEnd()) return -1;
	mp.parse(idx);
	mp.skipWhites();
	if(mp.isAtEnd()) return -1;
	
	mp.parse(choice);
	
	cout<<"WAIT idx = "<<idx<<" choice = "<<choice<<endl;
	
	if(idx >= svcvec.size() || svcvec[idx].empty()){
		cout<<"Fail WAIT"<<endl;
		return 0;
	}
	Service		&rsvc = *svcvec[idx];
	TimeSpec	start_time;
	switch(choice){
		case 'c':
			rsvc.waitCreate();
			start_time = rsvc.startTime();
			break;
		case 'C':
			rsvc.waitConnect();
			start_time = rsvc.startTime();
			break;
		case 'r':
			rsvc.waitReceive();
			start_time = rsvc.sendTime();
			break;
		case 'l':
			rsvc.waitLogin();
			start_time = rsvc.sendTime();
			break;
		default:
			cout<<"Choice unknown!!"<<endl;
			break;
	}
	
	TimeSpec	crt_time;
	crt_time.currentRealTime();
	crt_time -= start_time;
	cout<<"Done WAIT "<<crt_time.seconds()<<'.'<<crt_time.nanoSeconds()<<endl;
	return 0;
}

int executeSend(const char* _pb, int _b){
	cli::Parser		mp(_pb);
	int				conset;
	int				msgrow;
	int				count;
	int				sleepms = 10;
	
	mp.skipWhites();
	if(mp.isAtEnd()) return -1;
	mp.parse(conset);
	mp.skipWhites();
	if(mp.isAtEnd()) return -1;
	mp.parse(msgrow);
	mp.skipWhites();
	if(mp.isAtEnd()) return -1;
	mp.parse(count);
	mp.skipWhites();
	if(!mp.isAtEnd()){
		mp.parse(sleepms);
	}
	cout<<"SEND conset = "<<conset<<" msgrow = "<<msgrow<<" count = "<<count<<" sleepms = "<<sleepms<<endl;
	
	if(conset >= svcvec.size() || svcvec[conset].empty() || !mm.hasRow(msgrow)){
		cout<<"Fail SEND"<<endl;
		return 0;
	}
	Service &rsvc = *svcvec[conset];
	
	rsvc.send(msgrow, sleepms, count);
	cout<<"Done SEND"<<endl;
	return 0;
}