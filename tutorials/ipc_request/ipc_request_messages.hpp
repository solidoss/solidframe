#ifndef TUTORIAL_IPC_REQUEST_MESSAGES_HPP
#define TUTORIAL_IPC_REQUEST_MESSAGES_HPP

#include "solid/frame/ipc/ipcmessage.hpp"
#include "solid/frame/ipc/ipccontext.hpp"
#include "solid/frame/ipc/ipcprotocol_serialization_v1.hpp"
#include <vector>
#include <map>

namespace ipc_request{

struct Request: solid::frame::ipc::Message{
	std::string			userid_regex;
	
	Request(){}
	
	Request(std::string && _ustr): userid_regex(std::move(_ustr)){}
	
	template <class S>
	void serialize(S &_s, solid::frame::ipc::ConnectionContext &_rctx){
		_s.push(userid_regex, "userid_regex");
	}	
};

// struct Date{
// 	
// };

struct Date{
	uint8_t		day;
	uint8_t		month;
	uint16_t	year;
	
	template <class S>
	void serialize(S &_s, solid::frame::ipc::ConnectionContext &_rctx){
		_s.push(day, "day").push(month, "month").push(year, "year");
	}
};

struct UserData{
	std::string		user_id;
	std::string		full_name;
	std::string		email;
	std::string		country;
	std::string		city;
	Date			birth_date;
	
	template <class S>
	void serialize(S &_s, solid::frame::ipc::ConnectionContext &_rctx){
		_s.push(user_id, "user_id");
		_s.push(full_name, "full_name").push(email, "email");
		_s.push(country, "country").push(city, "city").push(birth_date, "birth_date");
	}
};

struct Response: solid::frame::ipc::Message{
	using UserDataMapT = std::map<std::string, UserData>;
	
	UserDataMapT	user_data_map;
	
	Response(){}
	
	Response(const solid::frame::ipc::Message &_rmsg):solid::frame::ipc::Message(_rmsg){}
	
	template <class S>
	void serialize(S &_s, solid::frame::ipc::ConnectionContext &_rctx){
		_s.pushContainer(user_data_map, "user_data_map");
	}
};

using ProtoSpecT = solid::frame::ipc::serialization_v1::ProtoSpec<0, Request, Response>;

}//namespace ipc_request

#endif

