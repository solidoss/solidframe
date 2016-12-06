#ifndef TUTORIAL_IPC_REQUEST_MESSAGES_HPP
#define TUTORIAL_IPC_REQUEST_MESSAGES_HPP

#include "solid/frame/mpipc/mpipcmessage.hpp"
#include "solid/frame/mpipc/mpipccontext.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v1.hpp"
#include <vector>
#include <ostream>
#include <map>

namespace ipc_request{

struct RequestKeyVisitor;

struct RequestKey{
	virtual ~RequestKey(){}
	
	virtual void print(std::ostream &_ros) const = 0;
	
	virtual void visit(RequestKeyVisitor &) = 0;
};

struct RequestKeyAnd;
struct RequestKeyOr;
struct RequestKeyAndList;
struct RequestKeyOrList;
struct RequestKeyUserIdRegex;
struct RequestKeyEmailRegex;
struct RequestKeyYearLess;

struct RequestKeyVisitor{
	virtual ~RequestKeyVisitor(){}
	
	virtual void visit(RequestKeyAnd&) = 0;
	virtual void visit(RequestKeyOr&) = 0;
	virtual void visit(RequestKeyAndList&) = 0;
	virtual void visit(RequestKeyOrList&) = 0;
	virtual void visit(RequestKeyUserIdRegex&) = 0;
	virtual void visit(RequestKeyEmailRegex&) = 0;
	virtual void visit(RequestKeyYearLess&) = 0;
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
		_s.push(second, "second").push(first, "first");
	}
	
	void print(std::ostream &_ros) const override{
		_ros<<'{';
		if(first) first->print(_ros);
		_ros<<',';
		if(second) second->print(_ros);
		_ros<<'}';
	}
	void visit(RequestKeyVisitor &_v) override{
		_v.visit(*this);
	}
};

struct RequestKeyOr: RequestKey{
	std::shared_ptr<RequestKey>		first;
	std::shared_ptr<RequestKey>		second;
	
	RequestKeyOr(){}
	
	template <class T1, class T2>
	RequestKeyOr(
		std::shared_ptr<T1> && _p1,
		std::shared_ptr<T2> &&_p2
	):	first(std::move(std::static_pointer_cast<RequestKey>(_p1))),
		second(std::move(std::static_pointer_cast<RequestKey>(_p2))){}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(second, "second").push(first, "first");
	}
	
	void print(std::ostream &_ros) const override{
		_ros<<'(';
		if(first) first->print(_ros);
		_ros<<',';
		if(second) second->print(_ros);
		_ros<<')';
	}
	void visit(RequestKeyVisitor &_v) override{
		_v.visit(*this);
	}
};

struct RequestKeyAndList: RequestKey{
	std::vector<std::shared_ptr<RequestKey>> key_vec;
	
	RequestKeyAndList(){}
	
	template <class ...Args>
	RequestKeyAndList(std::shared_ptr<Args>&& ..._args):key_vec{std::move(_args)...}{}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.pushContainer(key_vec, "key_vec");
	}
	
	void print(std::ostream &_ros) const override{
		_ros<<'{';
		for(auto const &key: key_vec){
			if(key) key->print(_ros);
			_ros<<',';
		}
		_ros<<'}';
	}
	void visit(RequestKeyVisitor &_v) override{
		_v.visit(*this);
	}
};

struct RequestKeyOrList: RequestKey{
	std::vector<std::shared_ptr<RequestKey>> key_vec;
	
	
	RequestKeyOrList(){}
	
	template <class ...Args>
	RequestKeyOrList(std::shared_ptr<Args>&& ..._args):key_vec{std::move(_args)...}{}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.pushContainer(key_vec, "key_vec");
	}
	
	void print(std::ostream &_ros) const override{
		_ros<<'(';
		for(auto const &key: key_vec){
			if(key) key->print(_ros);
			_ros<<',';
		}
		_ros<<')';
	}
	
	void visit(RequestKeyVisitor &_v) override{
		_v.visit(*this);
	}
};

struct RequestKeyUserIdRegex: RequestKey{
	std::string		regex;
	
	RequestKeyUserIdRegex(){}
	
	RequestKeyUserIdRegex(std::string && _ustr): regex(std::move(_ustr)){}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(regex, "regex");
	}
	
	void print(std::ostream &_ros) const override{
		_ros<<"userid matches \""<<regex<<"\"";
	}
	
	void visit(RequestKeyVisitor &_v) override{
		_v.visit(*this);
	}
};

struct RequestKeyEmailRegex: RequestKey{
	std::string		regex;
	
	RequestKeyEmailRegex(){}
	
	RequestKeyEmailRegex(std::string && _ustr): regex(std::move(_ustr)){}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(regex, "regex");
	}
	
	
	void print(std::ostream &_ros) const override{
		_ros<<"email matches \""<<regex<<"\"";
	}
	
	void visit(RequestKeyVisitor &_v) override{
		_v.visit(*this);
	}
};


struct RequestKeyYearLess: RequestKey{
	uint16_t	year;
	
	RequestKeyYearLess(uint16_t _year = 0xffff):year(_year){}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(year, "year");
	}
	
	void print(std::ostream &_ros) const override{
		_ros<<"year < "<<year;
	}
	
	void visit(RequestKeyVisitor &_v) override{
		_v.visit(*this);
	}
};



struct Request: solid::frame::mpipc::Message{
	std::shared_ptr<RequestKey>	key;
	
	Request(){}
	
	Request(std::shared_ptr<RequestKey> && _key): key(std::move(_key)){}
	
	template <class S>
	void serialize(S &_s, solid::frame::mpipc::ConnectionContext &_rctx){
		_s.push(key, "key");
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

using ProtoSpecT = solid::frame::mpipc::serialization_v1::ProtoSpec<
	0,
	Request,
	Response,
	RequestKeyAnd,
	RequestKeyOr,
	RequestKeyAndList,
	RequestKeyOrList,
	RequestKeyUserIdRegex,
	RequestKeyEmailRegex,
	RequestKeyYearLess
>;

}//namespace ipc_request

#endif

