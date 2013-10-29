#include "dchatstressconnection.hpp"

#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
#include "system/thread.hpp"
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"
#include "system/cstring.hpp"
#include "serialization/idtypemapper.hpp"
#include "serialization/binary.hpp"

#include "example/dchat/core/messages.hpp"
#include "example/dchat/core/compressor.hpp"

#include "cliparser.hpp"

#include <stack>

#include <signal.h>

#include "boost/program_options.hpp"

#include <iostream>

using namespace solid;
using namespace std;

typedef frame::Scheduler<frame::aio::Selector>	AioSchedulerT;

namespace{
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
		vector<string>		address_str_vec;
		vector<int>			port_vec;
		
		bool prepare();
	};

	Mutex					mtx;
	Condition				cnd;
	bool					running(true);
	Params					p;
	string					prefix;
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
	
	string create_char_set(){
		string s;
		for(char c = '0'; c <= '9'; ++c){
			s += c;
		}
		for(char c = 'a'; c <= 'z'; ++c){
			s += c;
		}
		for(char c = 'A'; c <= 'Z'; ++c){
			s += c;
		}
		return s;
	}
	void create_text(string &_rs, size_t _from, size_t _to, size_t _count, size_t _idx){
		static const string char_set = create_char_set();
		if(_from > _to){
			size_t tmp = _to;
			_to = _from;
			_from = tmp;
			_idx = _count - 1 - _idx;
		}
		const size_t newsz = (_from * (_count - _idx - 1) + _idx * _to) / (_count - 1);
		const size_t oldsz = _rs.size();
		_rs.resize(newsz + oldsz);
		for(size_t i = 0; i < newsz; ++i){
			_rs[oldsz + i] = char_set[i % char_set.size()];
		}
	}
}

struct TextMessage: TextMessageBase, solid::Dynamic<NoopMessage, solid::DynamicShared<solid::frame::Message> >{
	TextMessage(const std::string &_txt):TextMessageBase(_txt){}
	TextMessage(){}
};

struct Handle;



bool parseArguments(Params &_par, int argc, char *argv[]);

struct Handle{
	void beforeSerialization(BinSerializerT &_rs, void *_pt, ConnectionContext &_rctx){}
	
	void beforeSerialization(BinDeserializerT &_rs, void *_pt, ConnectionContext &_rctx){}
	bool checkStore(void *, ConnectionContext &_rctx)const{
		return true;
	}
	
	bool checkLoad(void *_pm, ConnectionContext &_rctx)const{
		return true;
	}
	void afterSerialization(BinSerializerT &_rs, void *_pm, ConnectionContext &_rctx){}
	
	void afterSerialization(BinDeserializerT &_rs, BasicMessage *_pm, ConnectionContext &_rctx);
		
	void afterSerialization(BinDeserializerT &_rs, TextMessage *_pm, ConnectionContext &_rctx);
	void afterSerialization(BinDeserializerT &_rs, NoopMessage *_pm, ConnectionContext &_rctx);
};

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
		typedef DynamicSharedPointer<Connection>	ConnectionPointerT;
		
		frame::Manager							m;
		AioSchedulerT							aiosched(m);
		UInt8TypeMapperT						tm;
		
		tm.insert<LoginRequest>();
		tm.insertHandle<BasicMessage, Handle>();
		tm.insertHandle<TextMessage, Handle>();
		tm.insertHandle<NoopMessage, Handle>();
		
		
		cliRun();
		
		idbg("");
		
		m.stop();
		
	}
	/*solid::*/Thread::waitAll();
	
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
	for(vector<string>::iterator it(endpoint_str_vec.begin()); it != endpoint_str_vec.end(); ++it){
		address_str_vec.push_back("");
		port_vec.push_back(0);
		split_endpoint_string(*it, address_str_vec.back(), port_vec.back());
	}
	return true;
}
//---------------------------------------------------------------------------------
void Handle::afterSerialization(BinDeserializerT &_rs, BasicMessage *_pm, ConnectionContext &_rctx){
	static const char *blancs = "                                    ";
	_rctx.rcon.onDoneIndex(_rctx.rcvmsgidx);
	if(_pm->v){
		Locker<Mutex> lock(mtx);
		cout<<'\r'<<blancs<<'\r'<<_rctx.rcvmsgidx<<" Error: "<<_pm->v<<endl;
		cout<<prefix<<flush;
	}
}
	
void Handle::afterSerialization(BinDeserializerT &_rs, TextMessage *_pm, ConnectionContext &_rctx){
	static const char *blancs = "                                    ";
	_rctx.rcon.onDoneIndex(_rctx.rcvmsgidx);
	{
		Locker<Mutex> lock(mtx);
		cout<<"\r"<<blancs<<'\r'<<_pm->user<<'>'<<' '<<_pm->text<<endl;
		cout<<prefix<<flush;
	}
}
void Handle::afterSerialization(BinDeserializerT &_rs, NoopMessage *_pm, ConnectionContext &_rctx){
	_rctx.rcon.onDoneIndex(_rctx.rcvmsgidx);
	_rctx.rcon.onReceiveNoop();
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
		if(!cstring::ncasecmp(pbuf, STRING_AND_SIZE("cms"))){
			len = STRING_SIZE("sleep");
			//TODO:
			//rc = executeCreateMessageSet(pbuf + len, cin.gcount() - len);
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
	
	cout<<"SLEEP "<<sec<<' '<<msec<<endl;
#ifdef __WIN32__
	Sleep(sec * 1000 + msec);
#else
	sleep(sec);
	Thread::sleep(msec);
#endif
	cout<<"Done SLEEP"<<endl;
    return 0;
}
