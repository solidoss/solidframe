/* Declarations file typemapper.hpp
	
	Copyright 2007, 2008 Valentin Palade 
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
#ifndef ALGORITHM_SERIALIZATION_TYPE_MAPPER_HPP
#define ALGORITHM_SERIALIZATION_TYPE_MAPPER_HPP

#include <string>
#include <typeinfo>
#include "utility/common.hpp"
#include "algorithm/serialization_new/typemapperbase.hpp"
namespace serialization_new{

template <class Ser, class Des, typename Int>
class IdTypeMapper: public TypeMapperBase{
	typedef IdTypeMapper<Ser, Des, Int>	ThisT;
public:
	IdTypeMapper(){}
	template <class T>
	uint32 insert(uint32 _idx = -1){
		return this->insertFunction(&doMap<T>, _idx, typeid(T).name());
	}
	template <class T, typename CT>
	uint32 insert(uint32 _idx = -1){
		return this->insertFunction(&doMap<T, CT>, _idx, typeid(T).name());
	}
	uint32 realIdentifier(uint32 _idx){
		return _idx;
	}
private:
	template <class T>
	static void doMap(void *_p, void *_ps, void *_pd, void *_pid, const char *_name){
		if(_ps){
			Ser &rs(*reinterpret_cast<Ser*>(_ps));
			T	&rt(*reinterpret_cast<T*>(_p));
			Int &rid(*reinterpret_cast<Int*>(_pid));
			
			rt & rs;
			rs.push(rid, _name);
		}else{
			Des &rd(*reinterpret_cast<Des*>(_pd));
			T*	&rpt(*reinterpret_cast<T**>(_p));
			rpt = new T;
			*rpt & rd;
		}
	}
	template <class T, class CT>
	static void doMap(void *_p, void *_ps, void *_pd, void *_pid, const char *_name){
		if(_ps){
			Ser &rs(*reinterpret_cast<Ser*>(_ps));
			T   &rt(*reinterpret_cast<T*>(_p));
			Int &rid(*reinterpret_cast<Int*>(_pid));
			
			rt & rs;
			rs.push(rid, _name);
		}else{
			Des &rd(*reinterpret_cast<Des*>(_pd));
			T*  &rpt(*reinterpret_cast<T**>(_p));
			rpt = new T(CT());
			*rpt & rd;
		}
	}
	/*virtual*/ void prepareStorePointer(void *_pser, void *_pt, uint32 _id, const char *_name){
		uint32	*pid;
		(*this->function(_id, pid))(_pt, _pser, NULL, pid, _name);
		
	}
	/*virtual*/ void prepareStorePointer(void *_pser, void *_pt, const char *_pid, const char *_name){
		uint32	*pid;
		(*this->function(_pid, pid))(_pt, _pser, NULL, pid, _name);
	}
	
	/*virtual*/ void prepareParsePointer(
		void *_pdes, std::string &_rs,
		void *_p, const char *_name
	){
		const Int	&rid(*reinterpret_cast<const Int*>(_rs.data()));
		uint32		id(rid);
		(*this->function(id))(_p, NULL, _pdes, NULL, _name);
	}
	/*virtual*/ void prepareParsePointerId(
		void *_pdes, std::string &_rs,
		const char *_name
	){
		Des		&rd(*reinterpret_cast<Des*>(_pdes));
		Int		&rid(*reinterpret_cast<Int*>(const_cast<char*>(_rs.data())));
		rd.push(rid, _name);
	}
};


}

#endif
