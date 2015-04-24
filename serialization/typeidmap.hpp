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
protected:
	typedef FUNCTION<void*()>							FactoryFunctionT;
	
	typedef void(*LoadFunctionT)(void*, void*);
	typedef void(*StoreFunctionT)(void*, void*, const char*);
	
	struct Stub{
		FactoryFunctionT	factoryfnc;
		LoadFunctionT		loadfnc;
		StoreFunctionT		storefnc;
	};
	
	typedef std::vector<Stub>							StubVectorT;
	
	typedef std::unordered_map<std::type_index, uint32>	TypeIndexMapT;
	
	template <class F>
	struct FactoryStub{
		F	f;
		FactoryStub(F _f):f(_f){}
		void* operator()(){
			return nullptr;
		}
	};
	
	template <class T, class Ser>
	static void store_pointer(void *_pser, void *_pt, const char *_name){
		Ser &rs = *(reinterpret_cast<Ser*>(_pser));
		T	&rt = *(reinterpret_cast<T*>(_pt));
		
		rs.push(rt, _name);
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
	bool store(Ser &_rs, T* _pt, const char *_name) const {
		if(_pt == nullptr){
			return storeNullPointer(_rs, _name);
		}else{
			return storePointer(_rs, _pt, std::type_index(typeid(_pt)), _name);
		}
	}
private:
	virtual bool storeNullPointer(Ser &_rs, const char *_name) const = 0;
	virtual bool storePointer(Ser &_rs, void *_p, std::type_index const&, const char *_name) const = 0;

	TypeIdMapSer(TypeIdMapSer&&);
	TypeIdMapSer& operator=(TypeIdMapSer&&);
};

template <class Des>
class TypeIdMapDes: virtual public TypeIdMapBase{
public:
	TypeIdMapDes(){}
	
	template <class T>
	bool load(
		Des &_rd,
		void* &_rptr,				//store destination pointer, the real type must be static_cast-ed to this pointer
		void* & _rrealptr,			//store pointer to real type
		const uint64 &_riv,			//integer value that may store the typeid
		std::string const &_rsv,	//string value that may store the typeid
		const char *_name
	) const {
		return loadPointer(_rd, _rptr, _rrealptr, std::type_index(typeid(T)), _riv, _rsv, _name);
	}
	
	virtual void loadTypeId(Des &_rd, uint64 &_rv, std::string &_rstr, const char* _name)const = 0;
private:
	
	virtual bool loadPointer(
		Des &_rd, void* &_rptr, void* & _rrealptr,
		std::type_index const& _rtidx,		//type_index of the destination pointer
		const uint64 &_riv, std::string const &_rsv, const char *_name
	) const = 0;
	
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
	/*virtual*/ bool storeNullPointer(Ser &_rs, const char *_name) const {
		static const uint32 nulltypeid = 0;
		_rs.pushCross(nulltypeid, _name);
		return true;
	}
	/*virtual*/ bool storePointer(Ser &_rs, void *_p, std::type_index const& _tid, const char *_name) const {
		TypeIdMapBase::TypeIndexMapT::const_iterator it = TypeIdMapBase::typemap.find(_tid);
		if(it != TypeIdMapBase::typemap.end()){
			TypeIdMapBase::Stub const & rstub = TypeIdMapBase::stubvec[it->second];
			(*rstub.storefnc)(&_rs, _p, _name);
			_rs.pushCrossValue(it->second, _name); 
			return true;
		}
		return false;
	}
	
	/*virtual*/ void loadTypeId(Des &_rd, uint64 &_rv, std::string &/*_rstr*/, const char *_name)const{
		_rd.pushCross(_rv, _name);
	}
	// Returns true, if the type identified by _riv exists
	// and a cast from that type to the type identified by _rtidx, exists
	/*virtual*/ bool loadPointer(
		Des &_rd, void* &_rptr, void* & _rrealptr,
		std::type_index const& _rtidx,		//type_index of the destination pointer
		const uint64 &_riv, std::string const &/*_rsv*/, const char *_name
	) const {
		//TODO:
		return false;
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
