// utility/dynamic.hpp
//
// Copyright (c) 2010 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_DYNAMIC_TYPE_HPP
#define UTILITY_DYNAMIC_TYPE_HPP

#include <vector>

#include "system/common.hpp"
#include "system/cassert.hpp"

#include "utility/dynamicpointer.hpp"
#include "system/atomic.hpp"

namespace solid{

//----------------------------------------------------------------
//		DynamicMapperBase
//----------------------------------------------------------------


struct DynamicMapperBase{
	bool check(const size_t _tid)const;
protected:
	DynamicMapperBase();
	~DynamicMapperBase();
	typedef void (*GenericFncT)(const DynamicPointer<> &, void *, void*);
	GenericFncT callback(const size_t _tid)const;
	void callback(const size_t _tid, GenericFncT _pf);
private:
	struct Data;
	Data	&d;
};

//----------------------------------------------------------------
//		DynamicBase
//----------------------------------------------------------------

//struct DynamicPointerBase;
//! A base for all types that needs dynamic typeid.
struct DynamicBase{
	static bool isType(const size_t _id){
		return false;
	}
	//! Get the type id for a Dynamic object.
	virtual size_t dynamicTypeId()const = 0;
	
	virtual bool isTypeDynamic(const size_t _id)const;
	
	virtual size_t callback(const DynamicMapperBase &_rdm)const;

protected:
	static size_t generateId();
	
	friend class DynamicPointerBase;
	virtual ~DynamicBase();
	
	//! Used by DynamicPointer - smartpointers
	size_t use();
	//! Used by DynamicPointer to know if the object must be deleted
	/*!
	 * For the return value, think of use count. Returning zero means the object should
	 * be deleted. Returning non zero means the object should not be deleted.
	 */
	size_t release();
private:
	typedef ATOMIC_NS::atomic<size_t>			AtomicSizeT;
	
	AtomicSizeT		usecount;
};

//----------------------------------------------------------------
//		Dynamic
//----------------------------------------------------------------


//! Template class to provide dynamic type functionality
/*!
	If you need to have dynamic type functionality for your objects,
	you need to inherit them through Dynamic. Here's an example:<br>
	if you need to have:<br>
		C: public B <br>
		B: public A <br>
	you will actually have:<br> 
		C: public Dynamic\<C,B><br>
		B: public Dynamic\<B,A><br>
		A: public Dynamic\<A>
*/
template <class X, class T = DynamicBase>
struct Dynamic: T{
	typedef Dynamic<X,T>	BaseT;
	
	Dynamic(){}
	//! One parameter constructor to forward to base
	template<class G1>
	explicit Dynamic(G1 &_g1):T(_g1){}
	
	template<class G1>
	explicit Dynamic(const G1 &_g1):T(_g1){}
	
	//! Two parameters constructor to forward to base
	template<class G1, class G2>
	explicit Dynamic(G1 &_g1, G2 &_g2):T(_g1, _g2){}
	
	template<class G1, class G2>
	explicit Dynamic(const G1 &_g1, const G2 &_g2):T(_g1, _g2){}
	
	//! Three parameters constructor to forward to base
	template<class G1, class G2, class G3>
	explicit Dynamic(G1 &_g1, G2 &_g2, G3 &_g3):T(_g1, _g2, _g3){}
	
	template<class G1, class G2, class G3>
	explicit Dynamic(const G1 &_g1, const G2 &_g2, const G3 &_g3):T(_g1, _g2, _g3){}
	
	template<class G1, class G2, class G3, class G4>
	explicit Dynamic(G1 &_g1, G2 &_g2, G3 &_g3, G4 &_g4):T(_g1, _g2, _g3, _g4){}
	
	template<class G1, class G2, class G3, class G4>
	explicit Dynamic(const G1 &_g1, const G2 &_g2, const G3 &_g3, const G4 &_g4):T(_g1, _g2, _g3, _g4){}
	
	template<class G1, class G2, class G3, class G4, class G5>
	explicit Dynamic(G1 &_g1, G2 &_g2, G3 &_g3, G4 &_g4, G5 &_g5):T(_g1, _g2, _g3, _g4, _g5){}
	
	template<class G1, class G2, class G3, class G4, class G5>
	explicit Dynamic(const G1 &_g1, const G2 &_g2, const G3 &_g3, const G4 &_g4, const G5 &_g5):T(_g1, _g2, _g3, _g4, _g5){}
	
	template<class G1, class G2, class G3, class G4, class G5, class G6>
	explicit Dynamic(const G1 &_g1, const G2 &_g2, const G3 &_g3, const G4 &_g4, const G5 &_g5, const G6 &_g6):T(_g1, _g2, _g3, _g4, _g5, _g6){}
	
	template<class G1, class G2, class G3, class G4, class G5, class G6, class G7>
	explicit Dynamic(G1 &_g1, G2 &_g2, G3 &_g3, G4 &_g4, G5 &_g5, G6 &_g6, G7 &_g7):T(_g1, _g2, _g3, _g4, _g5, _g6, _g7){}
	
	template<class G1, class G2, class G3, class G4, class G5, class G6>
	explicit Dynamic(G1 &_g1, G2 &_g2, G3 &_g3, G4 &_g4, G5 &_g5, G6 &_g6):T(_g1, _g2, _g3, _g4, _g5, _g6){}
	
	template<class G1, class G2, class G3, class G4, class G5, class G6, class G7>
	explicit Dynamic(const G1 &_g1, const G2 &_g2, const G3 &_g3, const G4 &_g4, const G5 &_g5, const G6 &_g6, const G7 &_g7):T(_g1, _g2, _g3, _g4, _g5, _g6, _g7){}
	
	//!The static type id
#ifdef HAS_SAFE_STATIC
	static size_t staticTypeId(){
		static const size_t id(DynamicBase::generateId());
		return id;
	}
#else
private:
	static size_t staticTypeIdStub(){
		static const size_t id(DynamicBase::generateId());
		return id;
	}
	static void once_cbk(){
		staticTypeIdStub();
	}
public:
	static size_t staticTypeId(){
		static boost::once_flag once = BOOST_ONCE_INIT;
		boost::call_once(&once_cbk, once);
		return staticTypeIdStub();
	}
#endif
	//TODO: add:
	//static bool isTypeExplicit(const DynamicBase*);
	static bool isType(const size_t _id){
		if(_id == staticTypeId()) return true;
		return T::isType(_id);
	}
	//! The dynamic typeid
	virtual size_t dynamicTypeId()const{
		return staticTypeId();
	}
	virtual bool isTypeDynamic(const size_t _id)const{
		if(_id == staticTypeId()) return true;
		return T::isTypeDynamic(_id);
	}
	//! Returns the associated callback from the given DynamicMap
	/*virtual*/ size_t callback(const DynamicMapperBase &_rdm)const{
		if(_rdm.check(staticTypeId())){
			return staticTypeId();
		}
		return T::callback(_rdm);
	}
	X* cast(DynamicBase *_pdb){
		if(isTypeDynamic(_pdb->dynamicTypeId())){
			return static_cast<X*>(_pdb);
		}
		return NULL;
	}
};

template <class Ret, class Obj, class Ctx = void>
struct DynamicMapper;


template <class Ret, class Obj>
struct DynamicMapper<Ret, Obj, void>: DynamicMapperBase{
	typedef Ret ReturnT;
	typedef Obj ObjectT;
	typedef Ret (*FncT)(const DynamicPointer<> &, Obj *, void*);
	
	template < class Dyn, class ObjX>
	static Ret static_handle(const DynamicPointer<> &_rdp, Obj *_po, void*){
		ObjX				&ro = static_cast<ObjX&>(*_po);
		DynamicPointer<Dyn>	dp(_rdp);
		return ro.dynamicHandle(dp);
	}
	
	DynamicMapper(){}
	~DynamicMapper(){
		
	}
	
	template <class Dyn, class ObjX>
	void insert(){
		FncT pf = &static_handle<Dyn, ObjX>;
		DynamicMapperBase::callback(Dyn::staticTypeId(), reinterpret_cast<GenericFncT>(pf));
	}
	
	FncT callback(const size_t _id)const{
		return reinterpret_cast<FncT>(DynamicMapperBase::callback(_id));
	}
};


template <class Ret, class Obj, class Ctx>
struct DynamicMapper: DynamicMapperBase{
	typedef Ret ReturnT;
	typedef Obj ObjectT;
	typedef Ret (*FncT)(const DynamicPointer<> &, Obj *, Ctx*);
	
	template < class Dyn, class ObjX>
	static Ret static_handle(const DynamicPointer<> &_rdp, Obj *_po, Ctx*_pctx){
		ObjX				&ro = static_cast<ObjX&>(*_po);
		DynamicPointer<Dyn>	dp(_rdp);
		return ro.dynamicHandle(dp, *_pctx);
	}
	
	DynamicMapper(){}
	~DynamicMapper(){
		
	}
	
	template <class Dyn, class ObjX>
	void insert(){
		FncT pf = &static_handle<Dyn, ObjX>;
		DynamicMapperBase::callback(Dyn::staticTypeId(), reinterpret_cast<GenericFncT>(pf));
	}
	
	FncT callback(const size_t _id)const{
		return reinterpret_cast<FncT>(DynamicMapperBase::callback(_id));
	}
};




//----------------------------------------------------------------
//		DynamicHandler
//----------------------------------------------------------------

//! A templated dynamic handler
/*!
*/
template <class Map, size_t Cp = 128>
class DynamicHandler{
public:
	DynamicHandler(Map &_rm):rm(_rm), sz(0){}
	
	template <class It>
	DynamicHandler(Map &_rm, It _first, It _last):rm(_rm), sz(0){
		init(_first, _last);
	}
	
	template <class DynPtr>
	DynamicHandler(Map &_rm, DynPtr &_dp):rm(_rm), sz(0){
		init(_dp);
	}
	
	~DynamicHandler(){
		clear();
	}
	template <typename It>
	void init(It _first, It _last){
		clear();
		sz = _last - _first;
		if(sz <= Cp){
			size_t off = 0;
			while(_first != _last){
				t[off] = *_first;
				++_first;
				++off;
			}
		}else{
			v.assign(_first, _last);
		}
	}
	template <typename DynPtr>
	void init(DynPtr &_dp){
		clear();
		sz = 1;
		if(sz <= Cp){
			t[0] = _dp;
		}else{
			DynamicPointer<> dp(_dp);
			v.push_back(dp);
		}
	}
	void clear(){
		v.clear();
		for(size_t i = 0; i < sz; ++i){
			t[i].clear();
		}
		sz = 0;
	}
	size_t size()const{
		return sz;
	}
	
	template <typename Obj, typename Ctx>
	typename Map::ReturnT handle(Obj &_robj, const size_t _idx, Ctx &_rctx){
		DynamicPointer<>	*pdp = NULL;
		if(sz <= Cp){
			pdp = &t[_idx];
		}else{
			pdp = &v[_idx];
		}
		const size_t		idx = (*pdp)->callback(rm);
		if(idx != static_cast<size_t>(-1)){
			typename Map::FncT pf = static_cast<typename Map::FncT>(rm.callback(idx));
			return (*pf)(*pdp, &_robj, &_rctx);
		}else{
			return static_cast<typename Map::ObjectT&>(_robj).dynamicHandle(*pdp, _rctx);
		}
	}
	
	template <typename Obj>
	typename Map::ReturnT handle(Obj &_robj, const size_t _idx){
		DynamicPointer<>	*pdp = NULL;
		if(sz <= Cp){
			pdp = &t[_idx];
		}else{
			pdp = &v[_idx];
		}
		const size_t		idx = (*pdp)->callback(rm);
		if(idx != static_cast<size_t>(-1)){
			typename Map::FncT pf = static_cast<typename Map::FncT>(rm.callback(idx));
			return (*pf)(*pdp, &_robj, NULL);
		}else{
			return static_cast<typename Map::ObjectT&>(_robj).dynamicHandle(*pdp);
		}
	}
	
private:
	typedef std::vector<DynamicPointer<DynamicBase> > DynamicPointerVectorT;
	Map								&rm;
	size_t							sz;
	DynamicPointer<DynamicBase>		t[Cp];
	DynamicPointerVectorT			v;
};

}//namespace solid

#endif
