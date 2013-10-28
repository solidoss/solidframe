#include "dchatstressconnection.hpp"

#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
#include "system/thread.hpp"
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"
#include "serialization/idtypemapper.hpp"
#include "serialization/binary.hpp"

#include "example/dchat/core/messages.hpp"
#include "example/dchat/core/compressor.hpp"

#include <signal.h>

#include "boost/program_options.hpp"

#include <iostream>

using namespace solid;
using namespace std;

typedef frame::Scheduler<frame::aio::Selector>	AioSchedulerT;

namespace{
	struct Params{
		int			port;
		string		address_str;
		string		dbg_levels;
		string		dbg_modules;
		string		dbg_addr;
		string		dbg_port;
		bool		dbg_console;
		bool		dbg_buffered;
		bool		log;
		string		user;
		string		pass;
	};

	Mutex					mtx;
	Condition				cnd;
	bool					run(true);
	Params					p;
	string					prefix;
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


int main(int argc, char *argv[]){
	if(parseArguments(p, argc, argv)) return 0;
	signal(SIGINT,term_handler); /* Die on SIGTERM */
	/*solid::*/Thread::init();
	
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
		
		
		ConnectionPointerT				ccptr(
			new Connection(
				m,
				synchronous_resolve(p.address_str.c_str(), p.port, 0, SocketInfo::Inet4, SocketInfo::Stream),
				tm
			)
		);
		
		m.registerObject(*ccptr);
		aiosched.schedule(ccptr);
		
		DynamicPointer<solid::frame::Message>	msgptr;
		string									s;
		if(p.pass.empty()){
			p.pass = p.user;
		}
		msgptr = new LoginRequest(p.user, p.pass);
		ccptr->send(msgptr);
		
		do{
			{
				Locker<Mutex> lock(mtx);
				prefix = "me [q to exit]> ";
				cout<<prefix<<flush;
			}
			s.clear();
			getline(cin, s);
			if(s.size() == 1 && s[0] == 'q' || s[0] == 'Q') break;
			msgptr = new TextMessage(s);
			ccptr->send(msgptr);
		}while(true);
		
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
		options_description desc("SolidFrame concept application");
		desc.add_options()
			("help,h", "List program options")
			("port,p", value<int>(&_par.port)->default_value(2000),"Server port")
			("user,u", value<string>(&_par.user),"Login User")
			("pass", value<string>(&_par.pass),"Login Password")
			("address,a", value<string>(&_par.address_str)->default_value("localhost"),"Server address")
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
