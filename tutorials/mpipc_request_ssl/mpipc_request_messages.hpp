#ifndef TUTORIAL_IPC_REQUEST_MESSAGES_HPP
#define TUTORIAL_IPC_REQUEST_MESSAGES_HPP

#include "solid/frame/mpipc/mpipcmessage.hpp"
#include "solid/frame/mpipc/mpipccontext.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v1.hpp"
#include <vector>
#include <map>

namespace ipc_request{


struct RequestKey{
	virtual ~RequestKey(){}
};

struct RequestKeyAnd: RequestKey{
	std::shared_ptr<RequestKey>		first;
	std::shared_ptr<RequestKey>		second;
	
	
	RequestKeyAnd(){}
	
	template <class T1, class T2>
	RequestKeyAnd(
		std::shared_ptr<T1> && _p1,
		std::shared_ptr<T2> &&_p2
	):	first(std::move(std::static_pointer_cast<RequestKey>(_p1))),
		second(std::move(std::static_pointer_cast<RequestKey>(_p2))){}
		
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(second, "second").push(first, "first").
	}
};

struct RequestKeyOr: RequestKey{
	std::shared_ptr<RequestKey>		first;
	std::shared_ptr<RequestKey>		second;
	
	
	template <class T1, class T2>
	RequestKeyOr(
		std::shared_ptr<T1> && _p1,
		std::shared_ptr<T2> &&_p2
	):	first(std::move(std::static_pointer_cast<RequestKey>(_p1))),
		second(std::move(std::static_pointer_cast<RequestKey>(_p2))){}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(second, "second").push(first, "first").
	}
};

struct RequestKeyAndList: RequestKey{
	std::vector<std::shared_ptr<RequestKey>> key_vec;
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.pushContainer(key_vec, "key_vec");
	}
};

struct RequestKeyOrList: RequestKey{
	std::vector<std::shared_ptr<RequestKey>> key_vec;
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.pushContainer(key_vec, "key_vec");
	}
};

struct RequestKeyUserIdRegex: RequestKey{
	std::string		regex;
	
	RequestKeyUserIdRegex(std::string && _ustr): regex(std::move(_ustr)){}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(regex, "regex");
	}
};

struct RequestKeyEmailRegex: RequestKey{
	std::string		regex;
	
	RequestKeyEmailRegex(std::string && _ustr): regex(std::move(_ustr)){}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(regex, "regex");
	}
};



struct Request: solid::frame::mpipc::Message{
	std::shared_ptr<RequestKey>	key;
	
	Request(){}
	
	Request(std::string && _ustr): userid_regex(std::move(_ustr)){}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(userid_regex, "userid_regex");
	}	
};

struct Date{
	uint8_t		day;
	uint8_t		month;
	uint16_t	year;
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(day, "day").push(month, "month").push(year, "year");
	}
};

struct UserData{
	std::string		full_name;
	std::string		email;
	std::string		country;
	std::string		city;
	Date			birth_date;
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(full_name, "full_name").push(email, "email");
		_s.push(country, "country").push(city, "city").push(birth_date, "birth_date");
	}
};

struct Response: solid::frame::mpipc::Message{
	using UserDataMapT = std::map<std::string, UserData>;
	
	UserDataMapT	user_data_map;
	
	Response(){}
	
	Response(const solid::frame::mpipc::Message &_rmsg):solid::frame::mpipc::Message(_rmsg){}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.pushContainer(user_data_map, "user_data_map");
	}
};

using ProtoSpecT = solid::frame::mpipc::serialization_v1::ProtoSpec<0, Request, Response>;

}//namespace ipc_request

#endif

