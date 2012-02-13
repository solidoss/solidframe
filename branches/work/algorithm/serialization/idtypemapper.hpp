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
#include "system/specific.hpp"
#include "utility/common.hpp"
#include "algorithm/serialization/typemapperbase.hpp"

namespace serialization{

template <class Ser, class Des, typename Int>
class IdTypeMapper: public TypeMapperBase{
	typedef IdTypeMapper<Ser, Des, Int>	ThisT;
	template <class T>
	static void doMap(void *_p, void *_ps, void *_pd, void *_pid, const char *_name){
		if(_ps){
			Ser &rs = *reinterpret_cast<Ser*>(_ps);
			T	&rt = *reinterpret_cast<T*>(_p);
			Int &rid = *reinterpret_cast<Int*>(_pid);
			
			rt & rs;
			rs.push(rid, _name);
		}else{
			Des &rd	= *reinterpret_cast<Des*>(_pd);
			T*	&rpt = *reinterpret_cast<T**>(_p);
			rpt = new T;
			*rpt & rd;
		}
	}
	template <class T, class CT>
	static void doMap(void *_p, void *_ps, void *_pd, void *_pid, const char *_name){
		if(_ps){
			Ser &rs = *reinterpret_cast<Ser*>(_ps);
			T   &rt = *reinterpret_cast<T*>(_p);
			Int &rid = *reinterpret_cast<Int*>(_pid);
			
			rt & rs;
			rs.push(rid, _name);
		}else{
			Des &rd = *reinterpret_cast<Des*>(_pd);
			T*  &rpt = *reinterpret_cast<T**>(_p);
			rpt = new T(CT());
			*rpt & rd;
		}
	}
	template <class T>
	static void doMapSpecific(void *_p, void *_ps, void *_pd, void *_pid, const char *_name){
		if(_ps){
			Ser &rs = *reinterpret_cast<Ser*>(_ps);
			T	&rt = *reinterpret_cast<T*>(_p);
			Int &rid = *reinterpret_cast<Int*>(_pid);
			
			rt & rs;
			rs.push(rid, _name);
		}else{
			Des &rd	= *reinterpret_cast<Des*>(_pd);
			T*	&rpt = *reinterpret_cast<T**>(_p);
			rpt = Specific::template uncache<T>();
			*rpt & rd;
		}
	}
	template <class T, class CT>
	static void doMapSpecific(void *_p, void *_ps, void *_pd, void *_pid, const char *_name){
		if(_ps){
			Ser &rs = *reinterpret_cast<Ser*>(_ps);
			T   &rt = *reinterpret_cast<T*>(_p);
			Int &rid = *reinterpret_cast<Int*>(_pid);
			
			rt & rs;
			rs.push(rid, _name);
		}else{
			Des &rd = *reinterpret_cast<Des*>(_pd);
			T*  &rpt = *reinterpret_cast<T**>(_p);
			rpt = Specific::template uncache<T>(CT());
			*rpt & rd;
		}
	}
public:
	IdTypeMapper(){}
	template <class T>
	uint32 insert(uint32 _idx = 0){
		typename UnsignedType<Int>::Type idx(_idx);
		return this->insertFunction(&ThisT::template doMap<T>, idx, typeid(T).name());
	}
	template <class T, typename CT>
	uint32 insert(uint32 _idx = 0){
		typename UnsignedType<Int>::Type idx(_idx);
		return this->insertFunction(&ThisT::template doMap<T, CT>, idx, typeid(T).name());
	}
	template <class T>
	uint32 insertSpecific(uint32 _idx = 0){
		typename UnsignedType<Int>::Type idx(_idx);
		return this->insertFunction(&ThisT::template doMapSpecific<T>, idx, typeid(T).name());
	}
	template <class T, typename CT>
	uint32 insertSpecific(uint32 _idx = 0){
		typename UnsignedType<Int>::Type idx(_idx);
		return this->insertFunction(&ThisT::template doMapSpecific<T, CT>, idx, typeid(T).name());
	}
	uint32 realIdentifier(uint32 _idx)const{
		return _idx;
	}
private:
	/*virtual*/ void prepareStorePointer(
		void *_pser, void *_pt,
		uint32 _id, const char *_name
	)const{
		uint32	*pid(NULL);
		(*this->function(_id, pid))(_pt, _pser, NULL, pid, _name);
	}
	/*virtual*/ void prepareStorePointer(
		void *_pser, void *_pt,
		const char *_pid, const char *_name
	)const{
		uint32	*pid(NULL);
		(*this->function(_pid, pid))(_pt, _pser, NULL, pid, _name);
	}
	
	/*virtual*/ bool prepareParsePointer(
		void *_pdes, std::string &_rs,
		void *_p, const char *_name
	)const{
		const Int							&rid(*reinterpret_cast<const Int*>(_rs.data()));
		typename UnsignedType<Int>::Type	idx(rid);
		FncT								pf(this->function(idx));
		
		if(pf){
			(*pf)(_p, NULL, _pdes, NULL, _name);
			return true;
		}else{
			return false;
		}
	}
	/*virtual*/ void prepareParsePointerId(
		void *_pdes, std::string &_rs,
		const char *_name
	)const{
		Des		&rd(*reinterpret_cast<Des*>(_pdes));
		Int		&rid(*reinterpret_cast<Int*>(const_cast<char*>(_rs.data())));
		rd.push(rid, _name);
	}
};


}

#endif
