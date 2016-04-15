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

#include "utility/common.hpp"
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


bool joinTypeId(uint64 &_rtype_id, const uint32 _protocol_id, const uint64 _message_id);

void splitTypeId(const uint64 _type_id, uint32 &_protocol_id, uint64 &_message_id);

class TypeIdMapBase{
protected:
	typedef FUNCTION<void*()>										FactoryFunctionT;
	
	//typedef void(*LoadFunctionT)(void*, void*, const char*);
	//typedef void(*StoreFunctionT)(void*, void*, const char*);
	typedef FUNCTION<void(void*, void*, const char*)>				LoadFunctionT;
	typedef FUNCTION<void(void*, void*, const char*)>				StoreFunctionT;
	
	typedef void(*CastFunctionT)(void*, void*);
	
	typedef std::pair<std::type_index, size_t>						CastIdT;
	
	struct CastHash{
		size_t operator()(CastIdT const &_id)const{
			return _id.first.hash_code() ^ _id.second;
		}
	};
	
	struct ProtocolStub{
		ProtocolStub():current_message_index(0){}
		
		size_t		current_message_index;
	};

	typedef std::unordered_map<CastIdT, CastFunctionT, CastHash>	CastMapT;
	
	struct Stub{
		FactoryFunctionT	factoryfnc;
		LoadFunctionT		loadfnc;
		StoreFunctionT		storefnc;
		uint64				id;
	};
	
	static ErrorConditionT error_no_cast();
	static ErrorConditionT error_no_type();
	
	typedef std::vector<Stub>										StubVectorT;
	
	typedef std::unordered_map<std::type_index, size_t>				TypeIndexMapT;
	typedef std::unordered_map<uint64, size_t>						MessageTypeIdMapT;
	typedef std::unordered_map<size_t, ProtocolStub>				ProtocolMapT;
	
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
	
	
	static void store_nullptr(void *_pser, void *_pt, const char *_name){
	}
	
	template <class T, class Des>
	static void load_pointer(void *_pser, void *_pt, const char *_name){
		Des &rs = *(reinterpret_cast<Des*>(_pser));
		T	&rt = *(reinterpret_cast<T*>(_pt));
		
		rs.push(rt, _name);
	}
	
	
	static void load_nullptr(void *_pser, void *_pt, const char *_name){
	}
	
	template <class F, class T, class Ser>
	struct StoreFunctor{
		F	f;
		StoreFunctor(F _f):f(_f){}
		
		void operator()(void *_pser, void *_pt, const char *_name){
			Ser &rs = *(reinterpret_cast<Ser*>(_pser));
			T	&rt = *(reinterpret_cast<T*>(_pt));
			
			f(rs, rt, _name);
		}
	};
	
	template <class F, class T, class Des>
	struct LoadFunctor{
		F	f;
		LoadFunctor(F _f):f(_f){}
		
		void operator()(void *_pser, void *_pt, const char *_name){
			Des &rs = *(reinterpret_cast<Des*>(_pser));
			T	&rt = *(reinterpret_cast<T*>(_pt));
			
			f(rs, rt, _name);
		}
	};
	
	template <class Base, class Derived>
	static void cast_pointer(void *_pderived, void* _pbase){
		Derived *pd = reinterpret_cast<Derived*>(_pderived);
		Base 	*&rpb = *reinterpret_cast<Base**>(_pbase);
		rpb = static_cast<Base*>(pd);
	}
	
	template <class T>
	static void cast_void_pointer(void *_pderived, void* _pbase){
		T 	*&rpb = *reinterpret_cast<T**>(_pbase);
		rpb = reinterpret_cast<T*>(_pderived);
	}
protected:
	TypeIdMapBase(){
		//register nullptr
		stubvec.push_back(Stub());
		stubvec.back().factoryfnc = [](){return nullptr;};
		stubvec.back().loadfnc = load_nullptr;
		stubvec.back().storefnc = store_nullptr;
		stubvec.back().id = 0;
		
		msgidmap[0] = 0;
	}
	
	bool findTypeIndex(const uint64 &_rid, size_t &_rindex) const;
	
	size_t doAllocateNewIndex(const size_t _protocol_id, uint64 &_rid);
	bool doFindTypeIndex(const size_t _protocol_id,  size_t _idx, uint64 &_rid) const ;
	
	template <class T, class StoreF, class LoadF, class FactoryF>
	size_t doRegisterType(StoreF _sf, LoadF _lf, FactoryF _ff, const size_t _protocol_id,  size_t _idx){
		uint64	id;
		
		if(_idx == 0){
			_idx = doAllocateNewIndex(_protocol_id, id);
		}else if(doFindTypeIndex(_protocol_id, _idx, id)){
			return InvalidIndex();
		}
		
		size_t		vecidx = stubvec.size();
		
		typemap[std::type_index(typeid(T))] = vecidx;
		msgidmap[id] = vecidx;
		
		stubvec.push_back(Stub());
		stubvec.back().factoryfnc = _ff;
		stubvec.back().loadfnc = _lf;
		stubvec.back().storefnc = _sf;
		stubvec.back().id = id;
		
		
		doRegisterCast<T, T>();
		doRegisterCast<T>();
		
		return vecidx;
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
			castmap[CastIdT(std::type_index(typeid(Derived)), 0)] = &cast_void_pointer<Derived>;
			castmap[CastIdT(std::type_index(typeid(Base)), 0)] = &cast_void_pointer<Base>;
			return true;
		}else{
			THROW_EXCEPTION("Derived type not registered");
			return false;
		}
	}
	
	template <class Derived>
	bool doRegisterCast(){
		TypeIdMapBase::TypeIndexMapT::const_iterator it = TypeIdMapBase::typemap.find(std::type_index(typeid(Derived)));
		if(it != TypeIdMapBase::typemap.end()){
			castmap[CastIdT(std::type_index(typeid(Derived)), 0)] = &cast_void_pointer<Derived>;
			return true;
		}else{
			THROW_EXCEPTION("Derived type not registered");
			return false;
		}
	}
	
protected:
	TypeIndexMapT		typemap;
	CastMapT			castmap;
	StubVectorT			stubvec;
	MessageTypeIdMapT	msgidmap;
	ProtocolMapT		protomap;
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
	
	template <class T>
	ErrorConditionT store(Ser &_rs, T* _pt, const size_t _type_id, const char *_name) const {
		if(_pt == nullptr){
			return storeNullPointer(_rs, _name);
		}else{
			return storePointer(_rs, _pt, _type_id, _name);
		}
	}
	
private:
	virtual ErrorConditionT storeNullPointer(Ser &_rs, const char *_name) const = 0;
	virtual ErrorConditionT storePointer(Ser &_rs, void *_p, std::type_index const&, const char *_name) const = 0;
	virtual ErrorConditionT storePointer(Ser &_rs, void *_p, const size_t _type_id, const char *_name) const = 0;

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
	
	ErrorConditionT findTypeIndex(const uint64 _type_id, size_t &_rstub_index)const{
		if(!TypeIdMapBase::findTypeIndex(_type_id, _rstub_index)){
			return TypeIdMapBase::error_no_cast();
		}
		return ErrorConditionT();
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
	size_t registerType(FactoryF _ff, const size_t _protocol_id, const size_t _idx = 0){
		return TypeIdMapBase::doRegisterType<T>(
			TypeIdMapBase::store_pointer<T, Ser>, TypeIdMapBase::load_pointer<T, Des>,
			_ff, _protocol_id, _idx
		);
	}
	
	template <class T, class StoreF, class LoadF, class FactoryF>
	size_t registerType(StoreF _sf, LoadF _lf, FactoryF _ff, const size_t _protocol_id, const size_t _idx = 0){
		TypeIdMapBase::StoreFunctor<StoreF, T, Ser>		sf(_sf);
		TypeIdMapBase::LoadFunctor<LoadF, T, Des>		lf(_lf);
		return TypeIdMapBase::doRegisterType<T>(sf, lf, _ff, _protocol_id, _idx);
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
			rstub.storefnc(&_rs, _p, _name);
			_rs.pushCross(rstub.id, _name); 
			return ErrorConditionT();
		}
		return TypeIdMapBase::error_no_type();
	}
	
	/*virtual*/ ErrorConditionT storePointer(Ser &_rs, void *_p, const size_t _type_id, const char *_name) const{
		if(_type_id < TypeIdMapBase::stubvec.size()){
			TypeIdMapBase::Stub const & rstub = TypeIdMapBase::stubvec[_type_id];
			rstub.storefnc(&_rs, _p, _name);
			_rs.pushCross(rstub.id, _name); 
			return ErrorConditionT();
		}else{
			return TypeIdMapBase::error_no_type();
		}
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
		
		size_t									stubindex;
		
		if(!TypeIdMapBase::findTypeIndex(_riv, stubindex)){
			return TypeIdMapBase::error_no_cast();
		}
		
		TypeIdMapBase::CastMapT::const_iterator	it = TypeIdMapBase::castmap.find(TypeIdMapBase::CastIdT(_rtidx, stubindex));
		
		if(it != TypeIdMapBase::castmap.end()){
			TypeIdMapBase::Stub	const	&rstub = TypeIdMapBase::stubvec[stubindex];
			void 						*realptr = rstub.factoryfnc();
			
			(*it->second)(realptr, _rptr);//store the pointer
			
			rstub.loadfnc(&_rd, realptr, _name);
			return ErrorConditionT();
		}
		return TypeIdMapBase::error_no_cast();
	}
	void clear(){
		TypeIdMapBase::typemap.clear();
		TypeIdMapBase::castmap.clear();
		TypeIdMapBase::stubvec.clear();
		TypeIdMapBase::msgidmap.clear();
		TypeIdMapBase::protomap.clear();
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
	size_t registerType(Data const &_rd, FactoryF _ff, const size_t _protocol_id = 0, const size_t _idx = 0){
		
		const size_t rv = TypeIdMapBase::doRegisterType<T>(
			TypeIdMapBase::store_pointer<T, Ser>,
			TypeIdMapBase::load_pointer<T, Des>,
			_ff, _protocol_id, _idx
		);
		
		if(rv == InvalidIndex()){
			return rv;
		}
		
		datavec.push_back(_rd);
		return rv;
	}
	
	template <class T, class StoreF, class LoadF, class FactoryF>
	size_t registerType(Data const &_rd, StoreF _sf, LoadF _lf, FactoryF _ff, const size_t _protocol_id = 0, const size_t _idx = 0){
		TypeIdMapBase::StoreFunctor<StoreF, T, Ser>		sf(_sf);
		TypeIdMapBase::LoadFunctor<LoadF, T, Des>		lf(_lf);
		
		const size_t rv = TypeIdMapBase::doRegisterType<T>(sf, lf, _ff, _protocol_id, _idx);
		
		if(rv == InvalidIndex()){
			return rv;
		}
		
		datavec.push_back(_rd);
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
	
	template <class T>
	size_t index(const T *_pt)const{
		TypeIdMapBase::TypeIndexMapT::const_iterator it = TypeIdMapBase::typemap.find(std::type_index(typeid(*_pt)));
		if(it != TypeIdMapBase::typemap.end()){
			return it->second;
		}else{
			return 0;
		}
	}
	
	Data& operator[](const size_t _idx){
		return datavec[_idx];
	}
	
	Data const& operator[](const size_t _idx)const{
		return datavec[_idx];
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
			rstub.storefnc(&_rs, _p, _name);
			_rs.pushCross(rstub.id, _name);
			return ErrorConditionT();
		}else{
			return TypeIdMapBase::error_no_type();
		}
	}
	
	/*virtual*/ ErrorConditionT storePointer(Ser &_rs, void *_p, const size_t _type_id, const char *_name) const{
		if(_type_id < TypeIdMapBase::stubvec.size()){
			TypeIdMapBase::Stub const & rstub = TypeIdMapBase::stubvec[_type_id];
			rstub.storefnc(&_rs, _p, _name);
			_rs.pushCross(rstub.id, _name);
			return ErrorConditionT();
		}else{
			return TypeIdMapBase::error_no_type();
		}
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
		size_t									stubindex;
		
		if(!TypeIdMapBase::findTypeIndex(_riv, stubindex)){
			return TypeIdMapBase::error_no_cast();
		}
		
		TypeIdMapBase::CastMapT::const_iterator	it = TypeIdMapBase::castmap.find(TypeIdMapBase::CastIdT(_rtidx, stubindex));
		
		if(it != TypeIdMapBase::castmap.end()){
			TypeIdMapBase::Stub	const	&rstub = TypeIdMapBase::stubvec[stubindex];
			void 						*realptr = rstub.factoryfnc();
			
			(*it->second)(realptr, _rptr);//store the pointer
			
			rstub.loadfnc(&_rd, realptr, _name);
			return ErrorConditionT();
		}
		return TypeIdMapBase::error_no_cast();
	}
	
	void clear(){
		TypeIdMapBase::typemap.clear();
		TypeIdMapBase::castmap.clear();
		TypeIdMapBase::stubvec.clear();
		TypeIdMapBase::msgidmap.clear();
		TypeIdMapBase::protomap.clear();
		
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
