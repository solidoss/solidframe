#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/mpipc/mpipcservice.hpp"
#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipcsocketstub_openssl.hpp"
#include "solid/frame/mpipc/mpipccompression_snappy.hpp"

#include "mpipc_request_messages.hpp"
#include <fstream>

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

std::string loadFile(const char *_path){
	ifstream		ifs(_path);
	ostringstream	oss;
	oss<<ifs.rdbuf();
	return oss.str();
}

//-----------------------------------------------------------------------------
namespace ipc_request_client{

using namespace ipc_request;

template <class M>
void complete_message(
	frame::mpipc::ConnectionContext &_rctx,
	std::shared_ptr<M> &_rsent_msg_ptr,
	std::shared_ptr<M> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	SOLID_CHECK(false);//this method should not be called
}

template <typename T>
struct MessageSetup;

template <>
struct MessageSetup<RequestKeyAnd>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyAnd>(_protocol_idx);
	}
};

template <>
struct MessageSetup<RequestKeyOr>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyOr>(_protocol_idx);
	}
};

template <>
struct MessageSetup<RequestKeyAndList>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyAndList>(_protocol_idx);
	}
};

template <>
struct MessageSetup<RequestKeyOrList>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyOrList>(_protocol_idx);
	}
};

template <>
struct MessageSetup<RequestKeyUserIdRegex>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyUserIdRegex>(_protocol_idx);
	}
};

template <>
struct MessageSetup<RequestKeyEmailRegex>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyEmailRegex>(_protocol_idx);
	}
};

template <>
struct MessageSetup<RequestKeyYearLess>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyYearLess>(_protocol_idx);
	}
};


template <typename T>
struct MessageSetup{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<T>(complete_message<T>, _protocol_idx, _message_idx);
	}
};


}//namespace

namespace{
	ostream& operator<<(ostream &_ros, const ipc_request::Date &_rd){
		_ros<<_rd.year<<'.'<<(int)_rd.month<<'.'<<(int)_rd.day;
		return _ros;
	}
	ostream& operator<<(ostream &_ros, const ipc_request::UserData &_rud){
		_ros<<_rud.full_name<<", "<<_rud.email<<", "<<_rud.country<<", "<<_rud.city<<", "<<_rud.birth_date;
		return _ros;
	}
}
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
		frame::mpipc::ServiceT	ipcservice(manager);
		
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
			auto 						proto = frame::mpipc::serialization_v1::Protocol::create();
			frame::mpipc::Configuration	cfg(scheduler, proto);
			
			ipc_request::ProtoSpecT::setup<ipc_request_client::MessageSetup>(*proto);
			
			cfg.client.name_resolve_fnc = frame::mpipc::InternetResolverF(resolver, p.port.c_str());
			
			cfg.client.connection_start_state = frame::mpipc::ConnectionState::Active;
			
			frame::mpipc::openssl::setup_client(
				cfg,
				[](frame::aio::openssl::Context &_rctx) -> ErrorCodeT{
					_rctx.addVerifyAuthority(loadFile("echo-ca-cert.pem"));
					_rctx.loadCertificate(loadFile("echo-client-cert.pem"));
					_rctx.loadPrivateKey(loadFile("echo-client-key.pem"));
					return ErrorCodeT();
				},
				frame::mpipc::openssl::NameCheckSecureStart{"echo-server"}
			);
			
			frame::mpipc::snappy::setup(cfg);
			
			err = ipcservice.reconfigure(std::move(cfg));
			
			if(err){
				cout<<"Error starting ipcservice: "<<err.message()<<endl;
				return 1;
			}
		}
		
		cout<<"Expect lines like:"<<endl;
		cout<<"quit"<<endl;
		cout<<"q"<<endl;
		cout<<"localhost user\\d*"<<endl;
		cout<<"127.0.0.1 [a-z]+_man"<<endl;
		
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
					
					
					
					auto  lambda = [](
						frame::mpipc::ConnectionContext &_rctx,
						std::shared_ptr<ipc_request::Request> &_rsent_msg_ptr,
						std::shared_ptr<ipc_request::Response> &_rrecv_msg_ptr,
						ErrorConditionT const &_rerror
					){
						if(_rerror){
							cout<<"Error sending message to "<<_rctx.recipientName()<<". Error: "<<_rerror.message()<<endl;
							return;
						}
						
						SOLID_CHECK(not _rerror and _rsent_msg_ptr and _rrecv_msg_ptr);
						
						cout<<"Received "<<_rrecv_msg_ptr->user_data_map.size()<<" users:"<<endl;
						
						for(const auto& user_data: _rrecv_msg_ptr->user_data_map){
							cout<<'{'<<user_data.first<<"}: "<<user_data.second<<endl;
						}
					};
					
					auto req_ptr = make_shared<ipc_request::Request>(
						make_shared<ipc_request::RequestKeyAndList>(
							make_shared<ipc_request::RequestKeyOr>(
								make_shared<ipc_request::RequestKeyUserIdRegex>(line.substr(offset + 1)),
								make_shared<ipc_request::RequestKeyEmailRegex>(line.substr(offset + 1))
							),
							make_shared<ipc_request::RequestKeyOr>(
								make_shared<ipc_request::RequestKeyYearLess>(2000),
								make_shared<ipc_request::RequestKeyYearLess>(2003)
							)
						)
					);
					
					cout<<"Request key: ";
					if(req_ptr->key) req_ptr->key->print(cout);
					cout<<endl;
					
					ipcservice.sendRequest(
						recipient.c_str(), //make_shared<ipc_request::Request>(line.substr(offset + 1)),
						req_ptr, lambda, 0
					);
				}else{
					cout<<"No recipient specified. E.g:"<<endl<<"localhost:4444 Some text to send"<<endl;
				}
			}
		}
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



