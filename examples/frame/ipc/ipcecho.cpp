#include "frame/manager.hpp"
#include "frame/scheduler.hpp"
#include "frame/service.hpp"

#include "frame/aio/aioreactor.hpp"
#include "frame/aio/aioobject.hpp"
#include "frame/aio/aiolistener.hpp"
#include "frame/aio/aiotimer.hpp"
#include "frame/aio/aioresolver.hpp"

#include "frame/aio/openssl/aiosecurecontext.hpp"
#include "frame/aio/openssl/aiosecuresocket.hpp"

#include "frame/ipc/ipcservice.hpp"
#include "frame/ipc/ipcconfiguration.hpp"
#include "frame/ipc/ipcprotocol_serialization_v1.hpp"

#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/socketaddress.hpp"

#include "boost/program_options.hpp"

#include <signal.h>
#include <string>
#include <iostream>

using namespace std;
using namespace solid;

typedef frame::Scheduler<frame::aio::Reactor>	AioSchedulerT;
typedef frame::aio::openssl::Context			SecureContextT;

//------------------------------------------------------------------
//------------------------------------------------------------------

struct Params{
	typedef std::vector<std::string>			StringVectorT;
	typedef std::vector<SocketAddressInet>		PeerAddressVectorT;
	string					dbg_levels;
	string					dbg_modules;
	string					dbg_addr;
	string					dbg_port;
	bool					dbg_console;
	bool					dbg_buffered;
	
	std::string				baseport;
	bool					log;
	StringVectorT			connectstringvec;
	
	bool prepare(frame::ipc::Configuration &_rcfg, string &_err);
};

//------------------------------------------------------------------

struct ServerStub{
    ServerStub():minmsec(0xffffffff), maxmsec(0), sz(0){}
    uint64_t	minmsec;
    uint64_t	maxmsec;
	uint64_t	sz;
};


struct MessageStub{
	MessageStub():count(0){}
	uint32_t count;
};

typedef std::vector<ServerStub>     ServerVectorT;
typedef std::vector<MessageStub>    MessageVectorT;

struct FirstMessage;

namespace{
	Mutex					mtx;
	Condition				cnd;
	//bool					run = true;
	//uint32_t					wait_count = 0;
	Params					app_params;
	void broadcast_message(frame::ipc::Service &_rsvc, std::shared_ptr<frame::ipc::Message> &_rmsgptr);
	void on_receive(FirstMessage const &_rmsg);
}

struct FirstMessage: frame::ipc::Message{
    std::string						str;
	
	FirstMessage(std::string const &_str):str(_str){
		idbg("CREATE ---------------- "<<(void*)this);
	}
	FirstMessage(){
		idbg("CREATE ---------------- "<<(void*)this);
	}
	~FirstMessage(){
		idbg("DELETE ---------------- "<<(void*)this);
	}

	template <class S>
	void serialize(S &_s, frame::ipc::ConnectionContext &_rctx){
		_s.push(str, "data");
	}
	
};

struct MessageHandler{
	frame::ipc::Service &rsvc;
	MessageHandler(frame::ipc::Service &_rsvc): rsvc(_rsvc){}
	
	void operator()(
		frame::ipc::ConnectionContext &_rctx,
		std::shared_ptr<FirstMessage> &_rsend_msg,
		std::shared_ptr<FirstMessage> &_rrecv_msg,
		ErrorConditionT const &_rerr
	);
};


void connection_stop(frame::ipc::ConnectionContext &_rctx, ErrorConditionT const&){
	idbg(_rctx.recipientId());
}

void incoming_connection_start(frame::ipc::ConnectionContext &_rctx){
	idbg(_rctx.recipientId());
}

void outgoing_connection_start(frame::ipc::ConnectionContext &_rctx){
	idbg(_rctx.recipientId());
}
//------------------------------------------------------------------

bool parseArguments(Params &_par, int argc, char *argv[]);

int main(int argc, char *argv[]){
	
	cout<<"Built on SolidFrame version "<<SOLID_VERSION_MAJOR<<'.'<<SOLID_VERSION_MINOR<<'.'<<SOLID_VERSION_PATCH<<endl;
	
	if(parseArguments(app_params, argc, argv)) return 0;
	
	Thread::init();
	
#ifdef SOLID_HAS_DEBUG
	{
	string dbgout;
	Debug::the().levelMask(app_params.dbg_levels.c_str());
	Debug::the().moduleMask(app_params.dbg_modules.c_str());
	if(app_params.dbg_addr.size() && app_params.dbg_port.size()){
		Debug::the().initSocket(
			app_params.dbg_addr.c_str(),
			app_params.dbg_port.c_str(),
			app_params.dbg_buffered,
			&dbgout
		);
	}else if(app_params.dbg_console){
		Debug::the().initStdErr(
			app_params.dbg_buffered,
			&dbgout
		);
	}else{
		Debug::the().initFile(
			*argv[0] == '.' ? argv[0] + 2 : argv[0],
			app_params.dbg_buffered,
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
		AioSchedulerT			sch;
		
		
		frame::Manager			m;
		frame::ipc::Service		ipcsvc(m);
		ErrorConditionT			err;
		
		frame::aio::Resolver	resolver;
		
		err = sch.start(1);
		
		if(err){
			cout<<"Error starting aio scheduler: "<<err.message()<<endl;
			return 1;
		}
		
		err = resolver.start(1);
		
		if(err){
			cout<<"Error starting aio resolver: "<<err.message()<<endl;
			return 1;
		}
		
		
		{
			frame::ipc::serialization_v1::Protocol	*proto = new frame::ipc::serialization_v1::Protocol;
			frame::ipc::Configuration				cfg(sch, proto);
			
			proto->registerType<FirstMessage>(
				MessageHandler(ipcsvc)
			);
			
			
			if(app_params.baseport.size()){
				cfg.listener_address_str = "0.0.0.0:";
				cfg.listener_address_str += app_params.baseport;
				cfg.listener_service_str = app_params.baseport;
			}
			
			if(app_params.connectstringvec.size()){
				cfg.name_resolve_fnc = frame::ipc::InternetResolverF(resolver, app_params.baseport.c_str());
			}
			cfg.connection_stop_fnc = connection_stop;
			cfg.connection_start_incoming_fnc = incoming_connection_start;
			cfg.connection_start_outgoing_fnc = outgoing_connection_start;
			cfg.connection_start_state = frame::ipc::ConnectionState::Active;
			
			err = ipcsvc.reconfigure(std::move(cfg));
			
			if(err){
				cout<<"Error starting ipcservice: "<<err.message()<<endl;
				Thread::waitAll();
				return 1;
			}
		}
		{
			string	s;
			do{
				getline(cin, s);
				if(s.size() == 1 && (s[0] == 'q' || s[0] == 'Q')){
					s.clear();
				}else{
					std::shared_ptr<frame::ipc::Message> msgptr(new FirstMessage(s));
					broadcast_message(ipcsvc, msgptr);
				}
			}while(s.size());
		}
		m.stop();
		vdbg("done stop");
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
			("listen-port,l", value<std::string>(&_par.baseport)->default_value("")->implicit_value("2000"), "IPC Listen port")
			("connect,c", value<vector<string> >(&_par.connectstringvec), "Peer to connect to: host:port")
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
	return true;
}


//------------------------------------------------------
//		FirstMessage
//------------------------------------------------------


void MessageHandler::operator()(
	frame::ipc::ConnectionContext &_rctx,
	std::shared_ptr<FirstMessage> &_rsend_msg,
	std::shared_ptr<FirstMessage> &_rrecv_msg,
	ErrorConditionT const &_rerr
){
	if(_rrecv_msg){
		idbg(_rctx.recipientId()<<" Message received: is_on_sender: "<<_rrecv_msg->isOnSender()<<", is_on_peer: "<<_rrecv_msg->isOnPeer()<<", is_back_on_sender: "<<_rrecv_msg->isBackOnSender());
		if(_rrecv_msg->isOnPeer()){
			rsvc.sendMessage(_rctx.recipientId(), _rrecv_msg);
		}else if(_rrecv_msg->isBackOnSender()){
			cout<<"Received from "<<_rctx.recipientName()<<": "<<_rrecv_msg->str<<endl;
		}
	}
}


namespace{

void broadcast_message(frame::ipc::Service &_rsvc, std::shared_ptr<frame::ipc::Message> &_rmsgptr){
	for(Params::StringVectorT::const_iterator it(app_params.connectstringvec.begin()); it != app_params.connectstringvec.end(); ++it){
		_rsvc.sendMessage(it->c_str(), _rmsgptr, 0|frame::ipc::MessageFlags::WaitResponse);
	}
}

void on_receive(FirstMessage const &_rmsg){
	Locker<Mutex> lock(mtx);
	cout<<"Received: "<<_rmsg.str<<endl;
}

}//namespace
