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
#ifndef ALGORITHM_SERIALIZATION_TYPE_MAPPER_BASE_HPP
#define ALGORITHM_SERIALIZATION_TYPE_MAPPER_BASE_HPP

#include <typeinfo>

#define BASIC_DECL(tp) \
template <class S>\
S& operator&(tp &_t, S &_s){\
	return _s.push(_t, "basic");\
}


namespace serialization{
//! A base class for mapping types to callbacks
class TypeMapperBase{
public:
	template <class T>
	static const char* typeName(T *_p){
		return typeid(*_p).name();
	}
	virtual void prepareStorePointer(
		void *_pser, void *_p,
		uint32 _rid, const char *_name
	)const;
	virtual void prepareStorePointer(
		void *_pser, void *_p,
		const char *_pid, const char *_name
	)const;
	virtual bool prepareParsePointer(
		void *_pdes, std::string &_rs,
		void *_p, const char *_name
	)const;
	virtual void prepareParsePointerId(
		void *_pdes, std::string &_rs,
		const char *_name
	)const;
protected:
	typedef void (*FncT)(void *, void *, void *, void *, const char *);
	typedef void (*PrepareIdFncT)(void *, void *, void *, uint32, const char *);
	typedef void (*PrepareNameFncT)(void *, void *, void *, const char *, const char *);
	
	TypeMapperBase();
	virtual ~TypeMapperBase();
	//! Insert a function callback
	uint32 insertFunction(FncT, uint32 _pos, const char *_name);
	uint32 insertFunction(FncT, uint16 _pos, const char *_name);
	uint32 insertFunction(FncT, uint8  _pos, const char *_name);
	FncT function(const uint32 _id, uint32* &_rpid)const;
	FncT function(const char *_pid, uint32* &_rpid)const;
	FncT function(const uint32 _id)const;
	FncT function(const uint16 _id)const;
	FncT function(const uint8  _id)const;
private:
	struct Data;
	Data &d;
};

}

#endif
