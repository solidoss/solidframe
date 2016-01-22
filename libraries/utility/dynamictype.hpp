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

#include "utility/common.hpp"
#include "utility/dynamicpointer.hpp"
#include "system/atomic.hpp"

namespace solid{

//----------------------------------------------------------------
//		DynamicBase
//----------------------------------------------------------------
typedef std::vector<size_t>	DynamicIdVectorT;

//struct DynamicPointerBase;
//! A base for all types that needs dynamic typeid.
struct DynamicBase{
	static bool isType(const size_t _id){
		return false;
	}
	//! Get the type id for a Dynamic object.
	virtual size_t dynamicTypeId()const = 0;
	
	static bool isTypeDynamic(const size_t _id);
	
	static void staticTypeIds(DynamicIdVectorT &_rv){
	}
	virtual void dynamicTypeIds(DynamicIdVectorT &_rv)const{
	}
protected:
	static size_t generateId();
	DynamicBase():usecount(0){}
	
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
#ifdef SOLID_USE_SAFE_STATIC
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
	static bool isTypeDynamic(const size_t _id){
		if(_id == staticTypeId()) return true;
		return T::isTypeDynamic(_id);
	}
	
	static X* cast(DynamicBase *_pdb){
		if(_pdb and isTypeDynamic(_pdb->dynamicTypeId())){
			return static_cast<X*>(_pdb);
		}
		return nullptr;
	}
	
	static const X* cast(const DynamicBase *_pdb){
		if(isTypeDynamic(_pdb->dynamicTypeId())){
			return static_cast<const X*>(_pdb);
		}
		return nullptr;
	}
	
	static void staticTypeIds(DynamicIdVectorT &_rv){
		_rv.push_back(BaseT::staticTypeId());
		T::staticTypeIds(_rv);
	}
	
	/*virtual*/ void dynamicTypeIds(DynamicIdVectorT &_rv)const{
		staticTypeIds(_rv);
	}
};


}//namespace solid

#endif
