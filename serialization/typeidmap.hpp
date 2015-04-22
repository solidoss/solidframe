// serialization/typeidmap.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_SERIALIZATION_TYPEIDMAP_HPP
#define SOLID_SERIALIZATION_TYPEIDMAP_HPP

#include "utility/dynamicpointer.hpp"
#include "system/function.hpp"
#include <unordered_map>
#include <typeindex>

namespace solid{
namespace serialization{


class TypeIdMapBase{
protected:
	typedef FUNCTION<void*()>							FactoryFunctionT;
	
	typedef void(*LoadFunctionT)(void*, void*);
	typedef void(*StoreFunctionT)(void*, void*);
	
	struct Stub{
		FactoryFunctionT	factoryfnc;
		LoadFunctionT		loadfnc;
		StoreFunctionT		storefnc;
	};
	
	typedef std::vector<Stub>							StubVectorT;
	
	typedef std::unordered_map<std::type_index, size_t>	TypeIndexMapT;
protected:
	TypeIndexMapT	typemap;
	StubVectorT		stubvec;	
};


template <class Ser>
class TypeIdMapSer: virtual protected TypeIdMapBase{
public:
	TypeIdMapSer(){}
	
	template <class T>
	bool store(Ser &_rs, T* _pt) const {
		if(_pt == nullptr){
			_rs.pushCrossValue(0, "type_id");
		}else{
			auto it = typemap.find(std::type_index(typeid(_pt)));
			if(it != typemap.end()){
				
				return true;
			}
		}
		return false;
	}
private:
	TypeIdMapSer(TypeIdMapSer&&);
	TypeIdMapSer& operator=(TypeIdMapSer&&);
};

template <class Des>
class TypeIdMapDes: virtual public TypeIdMapBase{
public:
	TypeIdMapDes(){}
	
	template <class T>
	void load(Des &_rs, T* & _pt) const {
		
	}
	
	template <class T>
	void load(Des &_rs, DynamicPointer<T> &_rptr) const {
		
	}
private:
	TypeIdMapDes(TypeIdMapDes&&);
	TypeIdMapDes& operator=(TypeIdMapDes&&);
};


template <class Ser, class Des, class Data = void>
class TypeIdMap;

template <class Ser, class Des>
class TypeIdMap<Ser, Des, void>: public TypeIdMapSer<Ser>, public TypeIdMapDes<Des>{
public:
	
	TypeIdMap(){
	}
	~TypeIdMap(){
		
	}
	
	template <class T>
	size_t registerType(size_t _idx = SOLID_INVALID_SIZE){
		return 0;
	}
	
	template <class T, class FactoryF>
	size_t registerType(FactoryF _f, size_t _idx = SOLID_INVALID_SIZE){
		return 0;
	}
	
	template <class Base, class Derived>
	size_t registerCast(){
		return 0;
	}
	
	template <class Derived>
	size_t registerCast(size_t _idx){
		return 0;
	}
	
private:
	TypeIdMap(TypeIdMap const &);
	TypeIdMap(TypeIdMap &&);
	TypeIdMap& operator=(TypeIdMap const &);
	TypeIdMap& operator=(TypeIdMap &&);
private:
};


}//namespace serialization
}//namespace solid
#endif
