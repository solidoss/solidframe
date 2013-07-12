/* Declarations file typemapperbase.hpp
	
	Copyright 2011, 2012 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef SOLID_SERIALIZATION_TYPE_MAPPER_BASE_HPP
#define SOLID_SERIALIZATION_TYPE_MAPPER_BASE_HPP

#include <typeinfo>

#define BASIC_DECL(tp) \
template <class S>\
S& operator&(tp &_t, S &_s){\
	return _s.push(_t, "basic");\
}

namespace solid{
namespace serialization{
//! A base class for mapping types to callbacks
class TypeMapperBase{
public:
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
		void *_pctx = NULL
	)const;
	virtual void prepareParsePointerId(
		void *_pdes, std::string &_rs,
		const char *_name
	)const;
protected:
	typedef bool (*FncSerT)(void *, void *, void *, const char *, void *);
	typedef bool (*FncDesT)(void *, void *, const char *, void *);
	
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
