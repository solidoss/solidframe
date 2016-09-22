#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/ipc/ipcservice.hpp"
#include "solid/frame/ipc/ipcconfiguration.hpp"

#include "ipc_echo_messages.hpp"

#include <iostream>

using namespace solid;
using namespace std;

using AioSchedulerT = frame::Scheduler<frame::aio::Reactor>;

//-----------------------------------------------------------------------------
//		Parameters
//-----------------------------------------------------------------------------
struct Parameters{
	Parameters():port("3333"){}
	
	string			port;
};

//-----------------------------------------------------------------------------
namespace ipc_echo_client{

template <class M>
void complete_message(
	frame::ipc::ConnectionContext &_rctx,
	std::shared_ptr<M> &_rsent_msg_ptr,
	std::shared_ptr<M> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	if(_rerror){
		cout<<"Error sending message to "<<_rctx.recipientName()<<". Error: "<<_rerror.message()<<endl;
		return;
	}
	
	SOLID_CHECK(_rrecv_msg_ptr and _rsent_msg_ptr);
	
	cout<<"Received from "<<_rctx.recipientName()<<": "<<_rrecv_msg_ptr->str<<endl;
}

template <typename T>
struct MessageSetup{
	void operator()(frame::ipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<T>(complete_message<T>, _protocol_idx, _message_idx);
	}
};


}//namespace

//-----------------------------------------------------------------------------

bool parseArguments(Parameters &_par, int argc, char *argv[]);

//-----------------------------------------------------------------------------
//		main
//-----------------------------------------------------------------------------

int main(int argc, char *argv[]){
	Parameters p;
	
	if(!parseArguments(p, argc, argv)) return 0;
	
	{
		
		AioSchedulerT			scheduler;
		
		
		frame::Manager			manager;
		frame::ipc::ServiceT	ipcservice(manager);
		
		frame::aio::Resolver	resolver;
		
		ErrorConditionT			err;
		
		err = scheduler.start(1);
		
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
			auto 						proto = frame::ipc::serialization_v1::Protocol::create();
			frame::ipc::Configuration	cfg(scheduler, proto);
			
			ipc_echo::ProtoSpecT::setup<ipc_echo_client::MessageSetup>(*proto);
			
			cfg.name_resolve_fnc = frame::ipc::InternetResolverF(resolver, p.port.c_str());
			
			cfg.connection_start_state = frame::ipc::ConnectionState::Active;
			
			err = ipcservice.reconfigure(std::move(cfg));
			
			if(err){
				cout<<"Error starting ipcservice: "<<err.message()<<endl;
				return 1;
			}
		}
		
		
		while(true){
			string	line;
			getline(cin, line);
			
			if(line == "q" or line == "Q" or line == "quit"){
				break;
			}
			{
				string		recipient;
				size_t		offset = line.find(' ');
				if(offset != string::npos){
					recipient = line.substr(0, offset);
					ipcservice.sendMessage(recipient.c_str(), make_shared<ipc_echo::Message>(line.substr(offset + 1)), 0|frame::ipc::MessageFlags::WaitResponse);
				}else{
					cout<<"No recipient specified. E.g:"<<endl<<"localhost:4444 Some text to send"<<endl;
				}
			}
		}
		
		manager.stop();
	}
	return 0;
}

//-----------------------------------------------------------------------------

bool parseArguments(Parameters &_par, int argc, char *argv[]){
	if(argc == 2){
		_par.port = argv[1];
	}
	return true;
}


