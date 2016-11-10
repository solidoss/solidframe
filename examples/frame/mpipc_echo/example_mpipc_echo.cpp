#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aiotimer.hpp"
#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/aio/openssl/aiosecurecontext.hpp"
#include "solid/frame/aio/openssl/aiosecuresocket.hpp"

#include "solid/frame/mpipc/mpipcservice.hpp"
#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v1.hpp"
#include "solid/frame/mpipc/mpipcsocketstub_openssl.hpp"

#include <mutex>
#include <condition_variable>
#include "solid/system/socketaddress.hpp"

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
	bool					secure;
	
	bool prepare(frame::mpipc::Configuration &_rcfg, string &_err);
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
	mutex					mtx;
	condition_variable		cnd;
	//bool					run = true;
	//uint32_t					wait_count = 0;
	Params					app_params;
	void broadcast_message(frame::mpipc::Service &_rsvc, std::shared_ptr<frame::mpipc::Message> &_rmsgptr);
}

struct FirstMessage: frame::mpipc::Message{
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
	void serialize(S &_s, frame::mpipc::ConnectionContext &_rctx){
		_s.push(str, "data");
	}
	
};

struct MessageHandler{
	frame::mpipc::Service &rsvc;
	MessageHandler(frame::mpipc::Service &_rsvc): rsvc(_rsvc){}
	
	void operator()(
		frame::mpipc::ConnectionContext &_rctx,
		std::shared_ptr<FirstMessage> &_rsend_msg,
		std::shared_ptr<FirstMessage> &_rrecv_msg,
		ErrorConditionT const &_rerr
	);
};


void connection_stop(frame::mpipc::ConnectionContext &_rctx){
	idbg(_rctx.recipientId()<<" error: "<<_rctx.error().message());
}

void incoming_connection_start(frame::mpipc::ConnectionContext &_rctx){
	idbg(_rctx.recipientId());
}

void outgoing_connection_start(frame::mpipc::ConnectionContext &_rctx){
	idbg(_rctx.recipientId());
}
//------------------------------------------------------------------

bool parseArguments(Params &_par, int argc, char *argv[]);

int main(int argc, char *argv[]){
	
	cout<<"Built on SolidFrame version "<<SOLID_VERSION_MAJOR<<'.'<<SOLID_VERSION_MINOR<<'.'<<SOLID_VERSION_PATCH<<endl;
	
	if(parseArguments(app_params, argc, argv)) return 0;
	
	
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
		frame::mpipc::ServiceT	ipcsvc(m);
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
			auto						proto = frame::mpipc::serialization_v1::Protocol::create();
			
			frame::mpipc::Configuration	cfg(sch, proto);
			
			proto->registerType<FirstMessage>(
				MessageHandler(ipcsvc)
			);
			
			
			if(app_params.baseport.size()){
				cfg.server.listener_address_str = "0.0.0.0:";
				cfg.server.listener_address_str += app_params.baseport;
				cfg.server.listener_service_str = app_params.baseport;
			}
			
			if(app_params.connectstringvec.size()){
				cfg.client.name_resolve_fnc = frame::mpipc::InternetResolverF(resolver, app_params.baseport.c_str());
			}
			cfg.connection_stop_fnc = connection_stop;
			cfg.server.connection_start_fnc = incoming_connection_start;
			cfg.client.connection_start_fnc = outgoing_connection_start;
			cfg.client.connection_start_state = frame::mpipc::ConnectionState::Active;
			cfg.server.connection_start_state = frame::mpipc::ConnectionState::Active;
#if 1
			if(app_params.secure){
				//configure OpenSSL:
				idbg("Configure SSL ---------------------------------------");
				frame::mpipc::openssl::setup_client(
					cfg,
					[](frame::aio::openssl::Context &_rctx) -> ErrorCodeT{
						_rctx.loadVerifyFile("echo-ca-cert.pem"/*"/etc/pki/tls/certs/ca-bundle.crt"*/);
						_rctx.loadCertificateFile("echo-client-cert.pem");
						_rctx.loadPrivateKeyFile("echo-client-key.pem");
						return ErrorCodeT();
					},
					frame::mpipc::openssl::NameCheckSecureStart{"echo-server"}
				);
				
				frame::mpipc::openssl::setup_server(
					cfg,
					[](frame::aio::openssl::Context &_rctx) -> ErrorCodeT{
						_rctx.loadVerifyFile("echo-ca-cert.pem"/*"/etc/pki/tls/certs/ca-bundle.crt"*/);
						_rctx.loadCertificateFile("echo-server-cert.pem");
						_rctx.loadPrivateKeyFile("echo-server-key.pem");
						return ErrorCodeT();
					},
					frame::mpipc::openssl::NameCheckSecureStart{"echo-client"}
				);
			}
#endif			
			err = ipcsvc.reconfigure(std::move(cfg));
			
			if(err){
				cout<<"Error starting ipcservice: "<<err.message()<<endl;
				return 1;
			}
			
			{
				std::ostringstream oss;
				oss<<ipcsvc.configuration().server.listenerPort();
				idbg("server listens on port: "<<oss.str());
			}
		}
		{
			string	s;
			do{
				getline(cin, s);
				if(s.size() == 1 && (s[0] == 'q' || s[0] == 'Q')){
					s.clear();
				}else{
					std::shared_ptr<frame::mpipc::Message> msgptr(new FirstMessage(s));
					broadcast_message(ipcsvc, msgptr);
				}
			}while(s.size());
		}
		vdbg("done stop");
	}
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
			("listen-port,l", value<std::string>(&_par.baseport)->default_value("2000"), "IPC Listen port")
			("connect,c", value<vector<string> >(&_par.connectstringvec), "Peer to connect to: host:port")
			("secure,s", value<bool>(&_par.secure)->implicit_value(true)->default_value(false), "Use SSL to secure communication")
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
bool Params::prepare(frame::mpipc::Configuration &_rcfg, string &_err){
	return true;
}


//------------------------------------------------------
//		FirstMessage
//------------------------------------------------------


void MessageHandler::operator()(
	frame::mpipc::ConnectionContext &_rctx,
	std::shared_ptr<FirstMessage> &_rsend_msg,
	std::shared_ptr<FirstMessage> &_rrecv_msg,
	ErrorConditionT const &_rerr
){
	if(_rrecv_msg){
		idbg(_rctx.recipientId()<<" Message received: is_on_sender: "<<_rrecv_msg->isOnSender()<<", is_on_peer: "<<_rrecv_msg->isOnPeer()<<", is_back_on_sender: "<<_rrecv_msg->isBackOnSender());
		if(_rrecv_msg->isOnPeer()){
			rsvc.sendResponse(_rctx.recipientId(), _rrecv_msg);
		}else if(_rrecv_msg->isBackOnSender()){
			cout<<"Received from "<<_rctx.recipientName()<<": "<<_rrecv_msg->str<<endl;
		}
	}
}


namespace{

void broadcast_message(frame::mpipc::Service &_rsvc, std::shared_ptr<frame::mpipc::Message> &_rmsgptr){
	
	vdbg("done stop===============================");
	
	for(Params::StringVectorT::const_iterator it(app_params.connectstringvec.begin()); it != app_params.connectstringvec.end(); ++it){
		_rsvc.sendMessage(it->c_str(), _rmsgptr, 0|frame::mpipc::MessageFlags::WaitResponse);
	}
}


}//namespace
