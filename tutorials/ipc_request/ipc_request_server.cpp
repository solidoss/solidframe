#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/ipc/ipcservice.hpp"
#include "solid/frame/ipc/ipcconfiguration.hpp"

#include "ipc_request_messages.hpp"
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

AccountDataDequeT	account_dq = {
	{"user1", "Super Man", "user1@email.com", "RO", "bucharest", {11, 1, 2001}},
	{"user2", "Spider Man", "user2@email.com", "RO", "bucharest", {12, 2, 2002}},
	{"user3", "Ant Man", "user3@email.com", "RO", "bucharest", {13, 3, 2003}},
	{"iron_man", "Iron Man", "gigel1@email.com", "UK", "london", {11,4,2004}},
	{"dragon_man", "Dragon Man", "gigel2@email.com", "FR", "paris", {12,5,2005}},
	{"frog_man", "Frog Man", "gigel3@email.com", "PL", "warsaw", {13,6,2006}},
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
	frame::ipc::ConnectionContext &_rctx,
	std::shared_ptr<M> &_rsent_msg_ptr,
	std::shared_ptr<M> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
);
	
template <>
void complete_message<ipc_request::Request>(
	frame::ipc::ConnectionContext &_rctx,
	std::shared_ptr<ipc_request::Request> &_rsent_msg_ptr,
	std::shared_ptr<ipc_request::Request> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	SOLID_CHECK(not _rerror);
	SOLID_CHECK(_rrecv_msg_ptr);
	SOLID_CHECK(not _rsent_msg_ptr);
	
	auto msgptr = std::make_shared<ipc_request::Response>(*_rrecv_msg_ptr);
	
	
	std::regex	userid_regex(_rrecv_msg_ptr->userid_regex);
	
	for(const auto &ad: account_dq){
		if(std::regex_match(ad.userid, userid_regex)){
			msgptr->user_data_map.insert(std::move(ipc_request::Response::UserDataMapT::value_type(ad.userid, std::move(make_user_data(ad)))));
		}
	}
	
	ErrorConditionT err = _rctx.service().sendMessage(_rctx.recipientId(), std::move(msgptr));
		
	SOLID_CHECK(not err);
}

template <>
void complete_message<ipc_request::Response>(
	frame::ipc::ConnectionContext &_rctx,
	std::shared_ptr<ipc_request::Response> &_rsent_msg_ptr,
	std::shared_ptr<ipc_request::Response> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	SOLID_CHECK(not _rerror);
	SOLID_CHECK(not _rrecv_msg_ptr);
	SOLID_CHECK(_rsent_msg_ptr);
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
		frame::ipc::Service		ipcsvc(manager);
		ErrorConditionT			err;
		
		err = scheduler.start(1);
		
		if(err){
			cout<<"Error starting aio scheduler: "<<err.message()<<endl;
			return 1;
		}
		
		{
			frame::ipc::serialization_v1::Protocol	*proto = new frame::ipc::serialization_v1::Protocol;
			frame::ipc::Configuration				cfg(scheduler, proto);
			
			ipc_request::ProtoSpecT::setup<ipc_request_server::MessageSetup>(*proto);
			
			cfg.listener_address_str = p.listener_addr;
			cfg.listener_address_str += ':';
			cfg.listener_address_str += p.listener_port;
			
			cfg.connection_start_state = frame::ipc::ConnectionState::Active;
			
			err = ipcsvc.reconfigure(std::move(cfg));
			
			if(err){
				cout<<"Error starting ipcservice: "<<err.message()<<endl;
				manager.stop();
				return 1;
			}
			{
				std::ostringstream oss;
				oss<<ipcsvc.configuration().listenerPort();
				cout<<"server listens on port: "<<oss.str()<<endl;
			}
		}
		
		cout<<"Press any char and ENTER to stop: ";
		char c;
		cin>>c;
		
		manager.stop();
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


