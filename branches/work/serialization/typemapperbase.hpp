// serialization/typemapperbase.hpp
//
// Copyright (c) 2011, 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_SERIALIZATION_TYPE_MAPPER_BASE_HPP
#define SOLID_SERIALIZATION_TYPE_MAPPER_BASE_HPP

#include <typeinfo>

#define BASIC_DECL(tp) \
template <class S>\
void serialize(S &_s, tp &_t){\
	_s.push(_t, "basic");\
}\
template <class S, class Ctx>\
void serialize(S &_s, tp &_t, Ctx &){\
	_s.push(_t, "basic");\
}

namespace solid{
namespace serialization{
//! A base class for mapping types to callbacks
class TypeMapperBase{
public:
	typedef void (*FncInitPointerT)(void*, void*);
	
	template <class T>
	static const char* typeName(T *_p){
		return typeid(*_p).name();
	}
	virtual bool prepareStorePointer(
		void *_pser, void *_p,
		uint32 _rid, const char *_name,
		void *_pctx = NULL
	)const;
	virtual bool prepareStorePointer(
		void *_pser, void *_p,
		const char *_pid, const char *_name,
		void *_pctx = NULL
	)const;
	virtual bool prepareParsePointer(
		void *_pdes, std::string &_rs,
		void *_p, const char *_name,
		void *_pctx = NULL,
		FncInitPointerT _pinicbk = NULL
		
	)const;
	virtual void prepareParsePointerId(
		void *_pdes, std::string &_rs,
		const char *_name
	)const;
	
protected:
	typedef bool (*FncSerT)(void *, void *, void *, const char *, void *);
	typedef bool (*FncDesT)(void *, void *, const char *, void *, FncInitPointerT);
	
	TypeMapperBase();
	virtual ~TypeMapperBase();
	//! Insert a function callback
	uint32 insertFunction(FncSerT, FncDesT, uint32 _pos, const char *_name);
	uint32 insertFunction(FncSerT, FncDesT, uint16 _pos, const char *_name);
	uint32 insertFunction(FncSerT, FncDesT, uint8  _pos, const char *_name);
	
	FncSerT function(const uint32 _id, uint32* &_rpid)const;
	FncSerT function(const char *_pid, uint32* &_rpid)const;
	
	FncDesT function(const uint32 _id)const;
	FncDesT function(const uint16 _id)const;
	FncDesT function(const uint8  _id)const;
private:
	struct Data;
	Data &d;
};

}//namespace serialization
}//namespace solid

#endif
