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

template <class T>
T* basic_factory(){
	return new T;
}

class TypeIdMapBase{
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
	
	template <class F>
	struct FactoryStub{
		F	f;
		FactoryStub(F _f):f(_f){}
		void* operator()(){
			return nullptr;
		}
	};
	
	template <class T, class Ser>
	static void store_pointer(void *_pser, void *_pt){
		Ser &rs = *(reinterpret_cast<Ser*>(_pser));
		T	&rt = *(reinterpret_cast<T*>(_pt));
		
		rs.push(rt, "pointer_data");
	}
	
	template <class T, class Des>
	static void load_pointer(void *_pser, void *_pt){
		Des &rs = *(reinterpret_cast<Des*>(_pser));
		T	&rt = *(reinterpret_cast<T*>(_pt));
		
		rs.push(rt, "pointer_data");
	}
	
protected:
	TypeIdMapBase():crtidx(1){
		stubvec.push_back(Stub());
	}
	
	template <class T, class Ser, class Des, class Factory>
	size_t doRegisterType(Factory _f, size_t _idx){
		
		if(_idx == SOLID_INVALID_SIZE){
			_idx = crtidx;
			++crtidx;
		}
		if(_idx >= stubvec.size()){
			stubvec.resize(_idx + 1);
		}
		stubvec[_idx].factoryfnc = _f;
		stubvec[_idx].loadfnc = load_pointer<T, Des>;
		stubvec[_idx].storefnc = store_pointer<T, Ser>;
		return _idx;
	}
	
	template <class Base, class Derived>
	size_t doRegisterCast(){
		return 0;
	}
	
	template <class Derived>
	size_t registerCast(size_t _idx){
		return 0;
	}
	
protected:
	TypeIndexMapT	typemap;
	StubVectorT		stubvec;
	size_t			crtidx;
};


template <class Ser>
class TypeIdMapSer: virtual protected TypeIdMapBase{
public:
	TypeIdMapSer(){}
	
	template <class T>
	bool store(Ser &_rs, T* _pt) const {
		if(_pt == nullptr){
			storeNullPointer(_rs);
		}else{
			storePointer(_rs, _pt, std::type_index(typeid(_pt)));
		}
		return false;
	}
	
	virtual void storeNullPointer(Ser &_rs) = 0;
	virtual void storePointer(Ser &_rs, void *_p, std::type_index const&) = 0;
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
		return TypeIdMapBase::doRegisterType<T, Ser, Des>(basic_factory<T>, _idx);
	}
	
	template <class T, class FactoryF>
	size_t registerType(FactoryF _f, size_t _idx = SOLID_INVALID_SIZE){
		return TypeIdMapBase::doRegisterType<T, Ser, Des>(_f, _idx);
	}
	
	template <class Base, class Derived>
	size_t registerCast(){
		return TypeIdMapBase::doRegisterCast<Base, Derived>();
	}
	
	template <class Derived>
	size_t registerCast(size_t _idx){
		return TypeIdMapBase::doRegisterCast<Derived>(_idx);
	}
private:
	/*virtual*/ void storeNullPointer(Ser &_rs){
		
	}
	/*virtual*/ void storePointer(Ser &_rs, void *_p, std::type_index const&){
		
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
