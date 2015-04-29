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
#include "system/exception.hpp"
#include "system/error.hpp"
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
	typedef FUNCTION<void*()>										FactoryFunctionT;
	
	typedef void(*LoadFunctionT)(void*, void*, const char*);
	typedef void(*StoreFunctionT)(void*, void*, const char*);
	
	typedef void(*CastFunctionT)(void*, void*);
	
	typedef std::pair<std::type_index, size_t>						CastIdT;
	struct CastHash{
		size_t operator()(CastIdT const &_id)const{
			return _id.first.hash_code() ^ _id.second;
		}
	};

	typedef std::unordered_map<CastIdT, CastFunctionT, CastHash>	CastMapT;
	
	struct Stub{
		FactoryFunctionT	factoryfnc;
		LoadFunctionT		loadfnc;
		StoreFunctionT		storefnc;
	};
	
	static ErrorConditionT error_no_cast();
	static ErrorConditionT error_no_type();
	
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
	static void load_pointer(void *_pser, void *_pt, const char *_name){
		Des &rs = *(reinterpret_cast<Des*>(_pser));
		T	&rt = *(reinterpret_cast<T*>(_pt));
		
		rs.push(rt, _name);
	}
	
	template <class Base, class Derived>
	static void cast_pointer(void *_pderived, void* _pbase){
		Derived *pd = reinterpret_cast<Derived*>(_pderived);
		Base 	*&rpb = *reinterpret_cast<Base**>(_pbase);
		rpb = static_cast<Base*>(pd);
	}
protected:
	TypeIdMapBase():crtidx(1){
		stubvec.push_back(Stub());
	}
	
	template <class T, class Ser, class Des, class Factory>
	size_t doRegisterType(Factory _f, size_t _idx){
		
		if(_idx == 0){
			_idx = crtidx;
			++crtidx;
		}
		if(_idx >= stubvec.size()){
			stubvec.resize(_idx + 1);
		}
		stubvec[_idx].factoryfnc = _f;
		stubvec[_idx].loadfnc = load_pointer<T, Des>;
		stubvec[_idx].storefnc = store_pointer<T, Ser>;
		typemap[std::type_index(typeid(T))] = _idx;
		doRegisterCast<T, T>();
		return _idx;
	}
	
	template <class Derived, class Base>
	bool doRegisterDownCast(){
		TypeIdMapBase::TypeIndexMapT::const_iterator it = TypeIdMapBase::typemap.find(std::type_index(typeid(Base)));
		if(it != TypeIdMapBase::typemap.end()){
			return doRegisterDownCast<Derived>(it->second);
		}else{
			THROW_EXCEPTION("Base type not registered");
			return false;
		}
	}
	
	template <class Derived>
	bool doRegisterDownCast(const size_t _idx){
		if(_idx < stubvec.size()){
			typemap[std::type_index(typeid(Derived))] = _idx;
			return true;
		}else{
			THROW_EXCEPTION_EX("Invalid type index ", _idx);
			return false;
		}
	}
	
	template <class Derived, class Base>
	bool doRegisterCast(){
		TypeIdMapBase::TypeIndexMapT::const_iterator it = TypeIdMapBase::typemap.find(std::type_index(typeid(Derived)));
		if(it != TypeIdMapBase::typemap.end()){
			castmap[CastIdT(std::type_index(typeid(Base)), it->second)] = &cast_pointer<Base, Derived>;
			return true;
		}else{
			THROW_EXCEPTION("Derived type not registered");
			return false;
		}
	}
	
protected:
	TypeIndexMapT	typemap;
	CastMapT		castmap;
	StubVectorT		stubvec;
	size_t			crtidx;
};


template <class Ser>
class TypeIdMapSer: virtual protected TypeIdMapBase{
public:
	TypeIdMapSer(){}
	
	template <class T>
	ErrorConditionT store(Ser &_rs, T* _pt, const char *_name) const {
		if(_pt == nullptr){
			return storeNullPointer(_rs, _name);
		}else{
			return storePointer(_rs, _pt, std::type_index(typeid(*_pt)), _name);
		}
	}
private:
	virtual ErrorConditionT storeNullPointer(Ser &_rs, const char *_name) const = 0;
	virtual ErrorConditionT storePointer(Ser &_rs, void *_p, std::type_index const&, const char *_name) const = 0;

	TypeIdMapSer(TypeIdMapSer&&);
	TypeIdMapSer& operator=(TypeIdMapSer&&);
};

template <class Des>
class TypeIdMapDes: virtual public TypeIdMapBase{
public:
	TypeIdMapDes(){}
	
	template <class T>
	ErrorConditionT load(
		Des &_rd,
		void* _rptr,				//store destination pointer, the real type must be static_cast-ed to this pointer
		const uint64 &_riv,			//integer value that may store the typeid
		std::string const &_rsv,	//string value that may store the typeid
		const char *_name
	) const {
		return loadPointer(_rd, _rptr, std::type_index(typeid(T)), _riv, _rsv, _name);
	}
	
	virtual void loadTypeId(Des &_rd, uint64 &_rv, std::string &_rstr, const char* _name)const = 0;
private:
	
	virtual ErrorConditionT loadPointer(
		Des &_rd, void* _rptr,
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
	
	
	template <class T, class FactoryF>
	size_t registerType(FactoryF _f, size_t _idx = 0){
		return TypeIdMapBase::doRegisterType<T, Ser, Des>(_f, _idx);
	}
	
	template <class Derived, class Base>
	bool registerDownCast(){
		return TypeIdMapBase::doRegisterDownCast<Derived, Base>();
	}
	
	
	template <class Derived, class Base>
	bool registerCast(){
		return TypeIdMapBase::doRegisterCast<Derived, Base>();
	}
	
private:
	/*virtual*/ ErrorConditionT storeNullPointer(Ser &_rs, const char *_name) const {
		static const uint32 nulltypeid = 0;
		_rs.pushCross(nulltypeid, _name);
		return ErrorConditionT();
	}
	
	/*virtual*/ ErrorConditionT storePointer(Ser &_rs, void *_p, std::type_index const& _tid, const char *_name) const {
		TypeIdMapBase::TypeIndexMapT::const_iterator it = TypeIdMapBase::typemap.find(_tid);
		if(it != TypeIdMapBase::typemap.end()){
			TypeIdMapBase::Stub const & rstub = TypeIdMapBase::stubvec[it->second];
			(*rstub.storefnc)(&_rs, _p, _name);
			_rs.pushCrossValue(it->second, _name); 
			return ErrorConditionT();
		}
		return TypeIdMapBase::error_no_type();
	}
	
	/*virtual*/ void loadTypeId(Des &_rd, uint64 &_rv, std::string &/*_rstr*/, const char *_name)const{
		_rd.pushCross(_rv, _name);
	}
	
	
	// Returns true, if the type identified by _riv exists
	// and a cast from that type to the type identified by _rtidx, exists
	/*virtual*/ ErrorConditionT loadPointer(
		Des &_rd, void* _rptr,
		std::type_index const& _rtidx,		//type_index of the destination pointer
		const uint64 &_riv, std::string const &/*_rsv*/, const char *_name
	) const {
		const size_t							typeindex = static_cast<size_t>(_riv);
		TypeIdMapBase::CastMapT::const_iterator	it = TypeIdMapBase::castmap.find(TypeIdMapBase::CastIdT(_rtidx, typeindex));
		
		if(it != TypeIdMapBase::castmap.end()){
			TypeIdMapBase::Stub	const	&rstub = TypeIdMapBase::stubvec[typeindex];
			void 						*realptr = rstub.factoryfnc();
			
			(*it->second)(realptr, _rptr);//store the pointer
			
			(*rstub.loadfnc)(&_rd, realptr, _name);
			return ErrorConditionT();
		}
		return TypeIdMapBase::error_no_cast();
	}
	void clear(){
		TypeIdMapBase::typemap.clear();
		TypeIdMapBase::castmap.clear();
		TypeIdMapBase::stubvec.clear();
		TypeIdMapBase::crtidx = 1;
	}
private:
	TypeIdMap(TypeIdMap const &);
	TypeIdMap(TypeIdMap &&);
	TypeIdMap& operator=(TypeIdMap const &);
	TypeIdMap& operator=(TypeIdMap &&);
private:
};


template <class Ser, class Des, class Data>
class TypeIdMap: public TypeIdMapSer<Ser>, public TypeIdMapDes<Des>{
	typedef std::vector<Data>	DataVectorT;
public:
	TypeIdMap(){
		datavec.resize(1);
	}
	
	~TypeIdMap(){
		
	}
	
	template <class T, class FactoryF>
	size_t registerType(Data const &_rd, FactoryF _f, size_t _idx = 0){
		const size_t rv = TypeIdMapBase::doRegisterType<T, Ser, Des>(_f, _idx);
		if(datavec.size() <= rv){
			datavec.resize(rv + 1);
		}
		datavec[rv] = _rd;
		return rv;
	}
	
	template <class Derived, class Base>
	bool registerDownCast(){
		return TypeIdMapBase::doRegisterDownCast<Derived, Base>();
	}
	
	
	template <class Derived, class Base>
	bool registerCast(){
		return TypeIdMapBase::doRegisterCast<Derived, Base>();
	}
	template <class T>
	Data& operator[](const T *_pt){
		TypeIdMapBase::TypeIndexMapT::const_iterator it = TypeIdMapBase::typemap.find(std::type_index(typeid(*_pt)));
		if(it != TypeIdMapBase::typemap.end()){
			return datavec[it->second];
		}else{
			return datavec[0];
		}
	}
	template <class T>
	Data const& operator[](const T *_pt)const{
		TypeIdMapBase::TypeIndexMapT::const_iterator it = TypeIdMapBase::typemap.find(std::type_index(typeid(*_pt)));
		if(it != TypeIdMapBase::typemap.end()){
			return datavec[it->second];
		}else{
			return datavec[0];
		}
	}
private:
	/*virtual*/ ErrorConditionT storeNullPointer(Ser &_rs, const char *_name) const {
		static const uint32 nulltypeid = 0;
		_rs.pushCross(nulltypeid, _name);
		return ErrorConditionT();
	}
	
	/*virtual*/ ErrorConditionT storePointer(Ser &_rs, void *_p, std::type_index const& _tid, const char *_name) const {
		TypeIdMapBase::TypeIndexMapT::const_iterator it = TypeIdMapBase::typemap.find(_tid);
		if(it != TypeIdMapBase::typemap.end()){
			TypeIdMapBase::Stub const & rstub = TypeIdMapBase::stubvec[it->second];
			(*rstub.storefnc)(&_rs, _p, _name);
			_rs.pushCrossValue(it->second, _name); 
			return ErrorConditionT();
		}
		return TypeIdMapBase::error_no_type();
	}
	
	/*virtual*/ void loadTypeId(Des &_rd, uint64 &_rv, std::string &/*_rstr*/, const char *_name)const{
		_rd.pushCross(_rv, _name);
	}
	
	
	// Returns true, if the type identified by _riv exists
	// and a cast from that type to the type identified by _rtidx, exists
	/*virtual*/ ErrorConditionT loadPointer(
		Des &_rd, void* _rptr,
		std::type_index const& _rtidx,		//type_index of the destination pointer
		const uint64 &_riv, std::string const &/*_rsv*/, const char *_name
	) const {
		const size_t							typeindex = static_cast<size_t>(_riv);
		TypeIdMapBase::CastMapT::const_iterator	it = TypeIdMapBase::castmap.find(TypeIdMapBase::CastIdT(_rtidx, typeindex));
		
		if(it != TypeIdMapBase::castmap.end()){
			TypeIdMapBase::Stub	const	&rstub = TypeIdMapBase::stubvec[typeindex];
			void 						*realptr = rstub.factoryfnc();
			
			(*it->second)(realptr, _rptr);//store the pointer
			
			(*rstub.loadfnc)(&_rd, realptr, _name);
			return ErrorConditionT();
		}
		return TypeIdMapBase::error_no_cast();
	}
	
	void clear(){
		TypeIdMapBase::typemap.clear();
		TypeIdMapBase::castmap.clear();
		TypeIdMapBase::stubvec.clear();
		TypeIdMapBase::crtidx = 1;
		datavec.clear();
	}
	
private:
	TypeIdMap(TypeIdMap const &);
	TypeIdMap(TypeIdMap &&);
	TypeIdMap& operator=(TypeIdMap const &);
	TypeIdMap& operator=(TypeIdMap &&);
private:
	DataVectorT	datavec;
};

}//namespace serialization
}//namespace solid
#endif
