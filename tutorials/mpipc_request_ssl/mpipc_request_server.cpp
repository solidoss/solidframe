#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/mpipc/mpipcservice.hpp"
#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipcsocketstub_openssl.hpp"

#include "mpipc_request_messages.hpp"
#include <string>
#include <deque>
#include <iostream>
#include <regex>

using namespace solid;
using namespace std;

namespace{

using AioSchedulerT = frame::Scheduler<frame::aio::Reactor>;

//-----------------------------------------------------------------------------
//		Parameters
//-----------------------------------------------------------------------------
struct Parameters{
	Parameters():listener_port("0"), listener_addr("0.0.0.0"){}
	
	string			listener_port;
	string			listener_addr;
};
//-----------------------------------------------------------------------------

struct Date{
	uint8_t		day;
	uint8_t		month;
	uint16_t	year;
};

struct AccountData{
	string		userid;
	string		full_name;
	string		email;
	string		country;
	string		city;
	Date		birth_date;
};


using AccountDataDequeT = std::deque<AccountData>;

const AccountDataDequeT	account_dq = {
	{"user1", "Super Man", "user1@email.com", "US", "Washington", {11, 1, 2001}},
	{"user2", "Spider Man", "user2@email.com", "RO", "Bucharest", {12, 2, 2002}},
	{"user3", "Ant Man", "user3@email.com", "IE", "Dublin", {13, 3, 2003}},
	{"iron_man", "Iron Man", "man.iron@email.com", "UK", "London", {11,4,2004}},
	{"dragon_man", "Dragon Man", "man.dragon@email.com", "FR", "paris", {12,5,2005}},
	{"frog_man", "Frog Man", "man.frog@email.com", "PL", "Warsaw", {13,6,2006}},
};

ipc_request::UserData make_user_data(const AccountData &_rad){
	ipc_request::UserData	ud;
	ud.full_name = _rad.full_name;
	ud.email = _rad.email;
	ud.country = _rad.country;
	ud.city = _rad.city;
	ud.birth_date.day = _rad.birth_date.day;
	ud.birth_date.month = _rad.birth_date.month;
	ud.birth_date.year = _rad.birth_date.year;
	return ud;
}

}//namespace

namespace ipc_request_server{

template <class M>
void complete_message(
	frame::mpipc::ConnectionContext &_rctx,
	std::shared_ptr<M> &_rsent_msg_ptr,
	std::shared_ptr<M> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
);

using namespace ipc_request;

struct AccountDataKeyVisitor: RequestKeyVisitor{
	const AccountData	&racc;
	bool				retval;
	
	AccountDataKeyVisitor(const AccountData &_racc):racc(_racc), retval(false){}
	
	void visit(RequestKeyAnd& _k) override{
		
		if(_k.first){
			_k.first->visit(*this);
			if(!retval) return;
		}
		if(_k.second){
			_k.second->visit(*this);
			if(!retval) return;
		}
		retval = true;
	}
	
	void visit(RequestKeyOr& _k) override{
		if(_k.first){
			_k.first->visit(*this);
			if(retval) return;
		}
		if(_k.second){
			_k.second->visit(*this);
			if(retval) return;
		}
		retval = false;
	}
	
	void visit(RequestKeyAndList& _k) override{
		for(auto &k: _k.key_vec){
			if(k){
				k->visit(*this);
				if(!retval) return;
			}
		}
		retval = true;
	}
	void visit(RequestKeyOrList& _k) override{
		for(auto &k: _k.key_vec){
			if(k){
				k->visit(*this);
				if(retval) return;
			}
		}
		retval = false;
	}
	
	void visit(RequestKeyUserIdRegex& _k) override{
		std::regex	userid_regex(_k.regex);//can be optimized by keeping it on the visitor
		retval = std::regex_match(racc.userid, userid_regex);
	}
	void visit(RequestKeyEmailRegex& _k) override{
		std::regex	userid_regex(_k.regex);//can be optimized by keeping it on the visitor
		retval = std::regex_match(racc.email, userid_regex);
	}
	
	void visit(RequestKeyYearLess& _k) override{
		retval = racc.birth_date.year < _k.year;
	}
};


template <>
void complete_message<ipc_request::Request>(
	frame::mpipc::ConnectionContext &_rctx,
	std::shared_ptr<ipc_request::Request> &_rsent_msg_ptr,
	std::shared_ptr<ipc_request::Request> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	SOLID_CHECK(not _rerror);
	SOLID_CHECK(_rrecv_msg_ptr);
	SOLID_CHECK(not _rsent_msg_ptr);
	
	cout<<"Received request: ";
	if(_rrecv_msg_ptr->key){
		_rrecv_msg_ptr->key->print(cout);
	}
	cout<<endl;
	
	
	auto msgptr = std::make_shared<ipc_request::Response>(*_rrecv_msg_ptr);
	
	
// 	std::regex	userid_regex(_rrecv_msg_ptr->userid_regex);
// 	
	for(const auto &ad: account_dq){
		AccountDataKeyVisitor v(ad);
		
		if(_rrecv_msg_ptr->key){
			_rrecv_msg_ptr->key->visit(v);
		}
		
		if(v.retval){
			msgptr->user_data_map.insert(std::move(ipc_request::Response::UserDataMapT::value_type(ad.userid, make_user_data(ad))));
		}
	}
	
	SOLID_CHECK_ERROR(_rctx.service().sendResponse(_rctx.recipientId(), std::move(msgptr)));
}


template <>
void complete_message<ipc_request::Response>(
	frame::mpipc::ConnectionContext &_rctx,
	std::shared_ptr<ipc_request::Response> &_rsent_msg_ptr,
	std::shared_ptr<ipc_request::Response> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	SOLID_CHECK(not _rerror);
	SOLID_CHECK(not _rrecv_msg_ptr);
	SOLID_CHECK(_rsent_msg_ptr);
}



template <typename T>
struct MessageSetup;

template <>
struct MessageSetup<RequestKeyAnd>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyAnd>(_protocol_idx);
		_rprotocol.registerCast<RequestKeyAnd, RequestKey>();
	}
};

template <>
struct MessageSetup<RequestKeyOr>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyOr>(_protocol_idx);
		_rprotocol.registerCast<RequestKeyOr, RequestKey>();
	}
};

template <>
struct MessageSetup<RequestKeyAndList>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyAndList>(_protocol_idx);
		_rprotocol.registerCast<RequestKeyAndList, RequestKey>();
	}
};

template <>
struct MessageSetup<RequestKeyOrList>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyOrList>(_protocol_idx);
		_rprotocol.registerCast<RequestKeyOrList, RequestKey>();
	}
};

template <>
struct MessageSetup<RequestKeyUserIdRegex>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyUserIdRegex>(_protocol_idx);
		_rprotocol.registerCast<RequestKeyUserIdRegex, RequestKey>();
	}
};

template <>
struct MessageSetup<RequestKeyEmailRegex>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyEmailRegex>(_protocol_idx);
		_rprotocol.registerCast<RequestKeyEmailRegex, RequestKey>();
	}
};

template <>
struct MessageSetup<RequestKeyYearLess>{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
		_rprotocol.registerType<RequestKeyYearLess>(_protocol_idx);
		_rprotocol.registerCast<RequestKeyYearLess, RequestKey>();
	}
};

template <typename T>
struct MessageSetup{
	void operator()(frame::mpipc::serialization_v1::Protocol &_rprotocol, const size_t _protocol_idx, const size_t _message_idx){
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
		frame::mpipc::ServiceT	ipcservice(manager);
		ErrorConditionT			err;
		
		err = scheduler.start(1);
		
		if(err){
			cout<<"Error starting aio scheduler: "<<err.message()<<endl;
			return 1;
		}
		
		{
			auto 						proto = frame::mpipc::serialization_v1::Protocol::create();
			frame::mpipc::Configuration	cfg(scheduler, proto);
			
			ipc_request::ProtoSpecT::setup<ipc_request_server::MessageSetup>(*proto);
			
			cfg.server.listener_address_str = p.listener_addr;
			cfg.server.listener_address_str += ':';
			cfg.server.listener_address_str += p.listener_port;
			
			cfg.server.connection_start_state = frame::mpipc::ConnectionState::Active;
			
			frame::mpipc::openssl::setup_server(
				cfg,
				[](frame::aio::openssl::Context &_rctx) -> ErrorCodeT{
					_rctx.loadVerifyFile("echo-ca-cert.pem");
					_rctx.loadCertificateFile("echo-server-cert.pem");
					_rctx.loadPrivateKeyFile("echo-server-key.pem");
					return ErrorCodeT();
				},
				frame::mpipc::openssl::NameCheckSecureStart{"fasdfas"}
			);
			
			err = ipcservice.reconfigure(std::move(cfg));
			
			if(err){
				cout<<"Error starting ipcservice: "<<err.message()<<endl;
				manager.stop();
				return 1;
			}
			{
				std::ostringstream oss;
				oss<<ipcservice.configuration().server.listenerPort();
				cout<<"server listens on port: "<<oss.str()<<endl;
			}
		}
		
		cout<<"Press any char and ENTER to stop:"<<endl;
		char c;
		cin>>c;
	}
	return 0;
}

//-----------------------------------------------------------------------------

bool parseArguments(Parameters &_par, int argc, char *argv[]){
	if(argc == 2){
		size_t			pos;
		
		_par.listener_addr = argv[1];
		
		pos = _par.listener_addr.rfind(':');
		
		if(pos != string::npos){
			_par.listener_addr[pos] = '\0';
			
			_par.listener_port.assign(_par.listener_addr.c_str() + pos + 1);
			
			_par.listener_addr.resize(pos);
		}
	}
	return true;
}


