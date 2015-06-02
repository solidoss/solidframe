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
    uint64	minmsec;
    uint64	maxmsec;
	uint64	sz;
};


struct MessageStub{
	MessageStub():count(0){}
	uint32 count;
};

typedef std::vector<ServerStub>     ServerVectorT;
typedef std::vector<MessageStub>    MessageVectorT;

struct FirstMessage;

namespace{
	Mutex					mtx;
	Condition				cnd;
	//bool					run = true;
	//uint32					wait_count = 0;
	Params					app_params;
	void broadcast_message(frame::ipc::Service &_rsvc, DynamicPointer<frame::ipc::Message> &_rmsgptr);
	void on_receive(FirstMessage const &_rmsg);
}

struct InitMessage: Dynamic<FirstMessage, frame::ipc::Message>{
	std::string		port;
	
	InitMessage(std::string const &_port):port(_port){}
	InitMessage(){}
	
	template <class S>
	void serialize(S &_s, frame::ipc::ConnectionContext &_rctx){
		_s.push(port, "port");
	}
};


struct FirstMessage: Dynamic<FirstMessage, frame::ipc::Message>{
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

// 	/*virtual*/ void ipcOnReceive(frame::ipc::ConnectionContext const &_rctx, MessagePointerT &_rmsgptr);
// 	/*virtual*/ UInt32PairT ipcOnPrepare(frame::ipc::ConnectionContext const &_rctx);
// 	/*virtual*/ void ipcOnComplete(frame::ipc::ConnectionContext const &_rctx, int _err);
	
	template <class S>
	void serialize(S &_s, frame::ipc::ConnectionContext &_rctx){
		_s.push(str, "data");
	}
	
};

struct MessageHandler{
	frame::ipc::Service &rsvc;
	MessageHandler(frame::ipc::Service &_rsvc): rsvc(_rsvc){}
	
	//Called on message receive
	void operator()(frame::ipc::ConnectionContext &_rctx, DynamicPointer<FirstMessage> &_rmsg);
	
	//Called when message is confirmed that:
	// * was successfully sent on peer-side - for requests, a message is considered successfully sent when the response was received
	// * was not successfuly sent - i.e. the connection was closed before message ACK
	
	void operator()(frame::ipc::ConnectionContext &_rctx, DynamicPointer<FirstMessage> &_rmsg, ErrorConditionT const &_rerr);
	uint32 operator()(frame::ipc::ConnectionContext &_rctx, FirstMessage const &_rmsg);
	
	
	void operator()(frame::ipc::ConnectionContext &_rctx, DynamicPointer<InitMessage> &_rmsg);
	
	//Called when message is confirmed that:
	// * was successfully sent on peer-side - for requests, a message is considered successfully sent when the response was received
	// * was not successfuly sent - i.e. the connection was closed before message ACK
	
	void operator()(frame::ipc::ConnectionContext &_rctx, DynamicPointer<InitMessage> &_rmsg, ErrorConditionT const &_rerr){
		
	}
	
	uint32 operator()(frame::ipc::ConnectionContext &_rctx, InitMessage const &_rmsg){
		return _rctx.messageFlags();
	}
};

//------------------------------------------------------------------

bool parseArguments(Params &_par, int argc, char *argv[]);

int main(int argc, char *argv[]){
	
	if(parseArguments(app_params, argc, argv)) return 0;
	
	Thread::init();
	
#ifdef UDEBUG
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
			frame::ipc::Configuration	cfg(sch);
			
			cfg.protocolCallback(
				[&ipcsvc](frame::ipc::ServiceProxy& _rsp){
					_rsp.registerType<InitMessage>(
						serialization::basic_factory<FirstMessage>,
						MessageHandler(ipcsvc), MessageHandler(ipcsvc), MessageHandler(ipcsvc)
					);
					_rsp.registerType<FirstMessage>(
						serialization::basic_factory<FirstMessage>,
						MessageHandler(ipcsvc), MessageHandler(ipcsvc), MessageHandler(ipcsvc)
					);
				}
			);
			
			cfg.listen_address_str = "0.0.0.0:"; cfg.listen_address_str += app_params.baseport;
			cfg.default_listen_port_str = app_params.baseport;
			cfg.name_resolve_fnc = frame::ipc::ResolverF(resolver, app_params.baseport.c_str());
			cfg.incoming_connection_start_fnc = [](frame::ipc::ConnectionContext &_rctx){
				if(_rctx.service().configuration().isServerOnly()){
					_rctx.service().activateConnection(_rctx.connectionId());
				}else{
					//wait to receive InitMessage, so we know on which connection pool, the incomming connection should be part of
				}
			};
			cfg.outgoing_connection_start_fnc = [](frame::ipc::ConnectionContext &_rctx){
				if(_rctx.service().configuration().isClientOnly()){
					_rctx.service().activateConnection(_rctx.connectionId());
				}else{
					//peer2peer mode: send InitMessage
					frame::ipc::MessagePointerT msgptr(new InitMessage(app_params.baseport));
					_rctx.service().sendMessage(_rctx.connectionId(), msgptr);
				}
			};
			
			err = ipcsvc.reconfigure(cfg);
			
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
					DynamicPointer<frame::ipc::Message> msgptr(new FirstMessage(s));
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
			("listen-port,l", value<std::string>(&_par.baseport)->default_value("2000"), "IPC Listen port")
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

// /*virtual*/ void FirstMessage::ipcOnReceive(frame::ipc::ConnectionContext const &_rctx, MessagePointerT &_rmsgptr){
// 	idbg("EXECUTE ----------------  size = "<<str.size());
// 	
// 	if(ipcIsOnReceiver()){
// 		//TODO:
// 		//_rctx.service().sendMessage(_rmsgptr, _rctx.connectionuid, (uint32)0/*fdt::ipc::Service::SynchronousSendFlag*/);
// 	}else{
// 		on_receive(*this);
// 	}
// }
// /*virtual*/ FirstMessage::UInt32PairT FirstMessage::ipcOnPrepare(frame::ipc::ConnectionContext const &_rctx){
// // 	if(isOnSender()){
// // 		return frame::ipc::WaitResponseFlag;
// // 	}else{
// // 		return 0;
// // 	}
// 	return UInt32PairT();
// }
// /*virtual*/ void FirstMessage::ipcOnComplete(frame::ipc::ConnectionContext const &_rctx, int _err){
// 	if(!_err){
//         idbg("SUCCESS ----------------");
//     }else{
//         idbg("ERROR ------------------");
//     }
// }

//Called on message receive
void MessageHandler::operator()(frame::ipc::ConnectionContext &_rctx, DynamicPointer<FirstMessage> &_rmsg){
	idbg("Message received: is_on_sender: "<<_rmsg->isOnSender()<<", is_on_peer: "<<_rmsg->isOnPeer()<<", is_back_on_sender: "<<_rmsg->isBackOnSender());
	if(_rmsg->isOnPeer()){
		//rsvc.sendResponse(_rctx.connectionId(), _rmsg);
	}
}
	
//Called when message is confirmed that:
// * was successfully sent on peer-side - for requests, a message is considered successfully sent when the response was received
// * was not successfuly sent - i.e. the connection was closed before message ACK

void MessageHandler::operator()(frame::ipc::ConnectionContext &_rctx, DynamicPointer<FirstMessage> &_rmsg, ErrorConditionT const &_rerr){
	if(!_rerr){
		idbg("Message successfully sent");
	}else{
		idbg("Message not confirmed: "<<_rerr.message());
	}
}

uint32 MessageHandler::operator()(frame::ipc::ConnectionContext &_rctx, FirstMessage const &_rmsg){
	return _rctx.messageFlags();
}


void MessageHandler::operator()(frame::ipc::ConnectionContext &_rctx, DynamicPointer<InitMessage> &_rmsg){
	
	SocketAddress				localaddr;
	SocketAddress				remoteaddr;
	
	
	_rctx.device().localAddress(localaddr);
	_rctx.device().remoteAddress(remoteaddr);
	
	
	std::ostringstream			tmposs;
		
	tmposs<<remoteaddr<<':'<<_rmsg->port;
	
	idbg("Received init from: "<<tmposs.str());
	
	frame::ipc::MessagePointerT msgptr(new InitMessage(app_params.baseport));
	ErrorConditionT				err = _rctx.service().activateConnection(
		_rctx.connectionId(), tmposs.str().c_str(),
		[](ErrorConditionT const &_rerr){
			if(not _rerr){
				return std::pair<frame::ipc::MessagePointerT, uint32>(new InitMessage(app_params.baseport), 0);
			}else{
				return std::pair<frame::ipc::MessagePointerT, uint32>(new InitMessage(), 0);
			}
		},
		localaddr < remoteaddr
	);
	
	if(err){
		edbg("Activating connection: "<<err.message());
	}
}

namespace{

void broadcast_message(frame::ipc::Service &_rsvc, DynamicPointer<frame::ipc::Message> &_rmsgptr){
	for(Params::StringVectorT::const_iterator it(app_params.connectstringvec.begin()); it != app_params.connectstringvec.end(); ++it){
		_rsvc.sendMessage(it->c_str(), _rmsgptr);
	}
}

void on_receive(FirstMessage const &_rmsg){
	Locker<Mutex> lock(mtx);
	cout<<"Received: "<<_rmsg.str<<endl;
}

}//namespace
