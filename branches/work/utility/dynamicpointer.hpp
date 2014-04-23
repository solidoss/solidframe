// utility/dynamicpointer.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_DYNAMIC_POINTER_HPP
#define UTILITY_DYNAMIC_POINTER_HPP

#include "system/common.hpp"

namespace solid{

struct DynamicBase;

class DynamicPointerBase{
protected:
	void clear(DynamicBase *_pdyn);
	void use(DynamicBase *_pdyn);
	void storeSpecific(void *_pv)const;
	static void* fetchSpecific();
};


template <class T = DynamicBase, class C = void>
class DynamicPointer;


template <class T = DynamicBase>
class DynamicSharedPointer: DynamicPointerBase{
public:
	typedef DynamicSharedPointer<T>		DynamicPointerT;
	typedef T							DynamicT;
	typedef T							element_type;
	
	DynamicSharedPointer(DynamicT *_pdyn = NULL):pdyn(_pdyn){
		if(_pdyn){
			use(static_cast<DynamicBase*>(_pdyn));
		}
	}
	template <class B>
	explicit DynamicSharedPointer(const DynamicSharedPointer<B> &_rcp):pdyn(static_cast<T*>(_rcp.release())){
		if(pdyn){
			use(static_cast<DynamicBase*>(pdyn));
		}
	}
	
	//The copy constructor must be specified - the compiler wount consider the above constructor as copy-constructor
	DynamicSharedPointer(const DynamicPointerT &_rcp):pdyn(static_cast<T*>(_rcp.release())){
		if(pdyn){
			use(static_cast<DynamicBase*>(pdyn));
		}
	}
	
	template <class B>
	explicit DynamicSharedPointer(const DynamicPointer<B> &_rcp):pdyn(static_cast<T*>(_rcp.release())){
		//we inherit the usecount - do not uncomment the lines below
// 		if(pdyn){
// 			use(static_cast<DynamicBase*>(pdyn));
// 		}
	}
	
	~DynamicSharedPointer(){
		if(pdyn){
			DynamicPointerBase::clear(static_cast<DynamicBase*>(pdyn));
		}
	}
	template <class O>
	DynamicPointerT& operator=(const DynamicSharedPointer<O> &_rcp){
		DynamicT *p(_rcp.get());
		if(p == pdyn){
			return *this;
		}
		if(pdyn) clear();
		set(p);
		return *this;
	}
	template <class O>
	DynamicPointerT& operator=(const DynamicPointer<O> &_rcp){
		DynamicT *p(_rcp.release());
		if(p == pdyn){
			return *this;
		}
		if(pdyn) clear();
		pdyn = p;//inherit the use
		return *this;
	}
	DynamicPointerT& operator=(DynamicT *_pdyn){
		if(_pdyn == pdyn){
			return *this;
		}
		clear();
		set(_pdyn);
		return *this;
	}
	DynamicT& operator*()const	{return *pdyn;}
	DynamicT* operator->()const	{return pdyn;}
	DynamicT* get() const		{return pdyn;}
	//operator bool () const	{return psig;}
	bool empty()const			{return !pdyn;}
	bool operator!()const		{return empty();}
	
	void clear(){
		if(pdyn){
			DynamicPointerBase::clear(static_cast<DynamicBase*>(pdyn));
			pdyn = NULL;
		}
	}
	DynamicT* release() const{
		DynamicT *tmp = pdyn;
		return tmp;
	}
protected:
	void set(DynamicT *_pdyn){
		pdyn = _pdyn;
		if(pdyn){
			use(static_cast<DynamicBase*>(pdyn));
		}
	}
private:
	mutable DynamicT *pdyn;
};

//! An autoptr like smartpointer for DynamicBase objects
template <class T>
class DynamicPointer<T, void>: DynamicPointerBase{
public:
	typedef DynamicPointer<T>	DynamicPointerT;
	typedef T					DynamicT;
	typedef T					element_type;
	
	DynamicPointer(DynamicT *_pdyn = NULL):pdyn(_pdyn){
		if(_pdyn){
			use(static_cast<DynamicBase*>(_pdyn));
		}
	}
	//!Use this constructor if you want to pass a pointer without incrementing use count - use it with caution
	DynamicPointer(DynamicT *_pdyn, bool _b):pdyn(_pdyn){
	}
	template <class B>
	explicit DynamicPointer(const DynamicPointer<B> &_rcp):pdyn(static_cast<T*>(_rcp.release())){
		//we inherit the usecount - do not uncomment the lines below
// 		if(pdyn){
// 			use(static_cast<DynamicBase*>(pdyn));
// 		}
	}
	
	template <class B, class Ct>
	DynamicPointer(const DynamicPointer<B, Ct> &_rcp):pdyn(static_cast<T*>(_rcp.release())){
		//we inherit the usecount - do not uncomment the lines below
// 		if(pdyn){
// 			use(static_cast<DynamicBase*>(pdyn));
// 		}
	}
	//The copy constructor must be specified - the compiler wount consider the above constructor as copy-constructor
	DynamicPointer(const DynamicPointerT &_rcp):pdyn(_rcp.release()){
		//we inherit the usecount - do not uncomment the lines below
// 		if(pdyn){
// 			use(static_cast<DynamicBase*>(pdyn));
// 		}
	}
	
	template <class B>
	DynamicPointer(const DynamicSharedPointer<B> &_rcp):pdyn(static_cast<T*>(_rcp.get())){
		if(pdyn){
			use(static_cast<DynamicBase*>(pdyn));
		}
	}
	
	~DynamicPointer(){
		if(pdyn){
			DynamicPointerBase::clear(static_cast<DynamicBase*>(pdyn));
		}
	}
	DynamicT* release() const{
		DynamicT *tmp = pdyn;
		pdyn = NULL;return tmp;
	}
	DynamicPointerT& operator=(const DynamicPointerT &_rcp){
		DynamicT *p(_rcp.release());
		if(this == &_rcp){
			return *this;
		}
		if(pdyn) clear();
		pdyn = p;//we inherit the usecount
		return *this;
	}
	template <class O>
	DynamicPointerT& operator=(const DynamicPointer<O> &_rcp){
		DynamicT *p(_rcp.release());
		if(p == pdyn){
			return *this;
		}
		if(pdyn) clear();
		pdyn = p;//we inherit the usecount
		return *this;
	}
	template <class O>
	DynamicPointerT& operator=(const DynamicSharedPointer<O> &_rcp){
		DynamicT *p(_rcp.get());
		if(p == pdyn){
			return *this;
		}
		if(pdyn) clear();
		ptr(p);
		return *this;
	}
	DynamicPointerT& operator=(DynamicT *_pdyn){
		if(_pdyn == pdyn){
			return *this;
		}
		clear();
		ptr(_pdyn);
		return *this;
	}
	DynamicT& operator*()const	{return *pdyn;}
	DynamicT* operator->()const	{return pdyn;}
	DynamicT* get() const		{return pdyn;}
	//operator bool () const	{return psig;}
	bool empty() const			{return !pdyn;}
	bool operator!()const		{return empty();}
	void clear(){
		if(pdyn){
			DynamicPointerBase::clear(static_cast<DynamicBase*>(pdyn));
			pdyn = NULL;
		}
	}
protected:
	void ptr(DynamicT *_pdyn){
		pdyn = _pdyn;
		if(pdyn){
			use(static_cast<DynamicBase*>(pdyn));
		}
	}
private:
	mutable DynamicT *pdyn;
	
};
#if 0
//! An autoptr like smartpointer for DynamicBase objects
template <class T, class C>
class DynamicPointer: DynamicPointerBase{
public:
	typedef DynamicPointer<T,C>	DynamicPointerT;
	typedef T					DynamicT;
	typedef C					ContextT;
	
	DynamicPointer(DynamicT *_pdyn = NULL):pdyn(_pdyn){
		if(_pdyn){
			use(static_cast<DynamicBase*>(_pdyn));
		}
	}
	//!Use this constructor if you want to pass a pointer without incrementing use count - use it with caution
	DynamicPointer(DynamicT *_pdyn, bool _b):pdyn(_pdyn){
	}
	DynamicPointer(DynamicT *_pdyn, const ContextT &_rctx):pdyn(_pdyn), ctx(_rctx){
	}
	template <class B>
	explicit DynamicPointer(const DynamicPointer<B, void> &_rcp):pdyn(static_cast<T*>(_rcp.release())){
		//we inherit the usecount - do not uncomment the lines below
// 		if(pdyn){
// 			use(static_cast<DynamicBase*>(pdyn));
// 		}
	}
	template <class B, class Ct>
	explicit DynamicPointer(const DynamicPointer<B, Ct> &_rcp):pdyn(static_cast<T*>(_rcp.release())), ctx(_rcp.ctx){
		//we inherit the usecount - do not uncomment the lines below
// 		if(pdyn){
// 			use(static_cast<DynamicBase*>(pdyn));
// 		}
	}
	
	//The copy constructor must be specified - the compiler wount consider the above constructor as copy-constructor
	DynamicPointer(const DynamicPointerT &_rcp):pdyn(_rcp.release()), ctx(_rcp.ctx){
		//we inherit the usecount - do not uncomment the lines below
// 		if(pdyn){
// 			use(static_cast<DynamicBase*>(pdyn));
// 		}
	}
	
	template <class B>
	explicit DynamicPointer(const DynamicSharedPointer<B> &_rcp):pdyn(static_cast<T*>(_rcp.release())){
		if(pdyn){
			use(static_cast<DynamicBase*>(pdyn));
		}
	}
	
	~DynamicPointer(){
		if(pdyn){
			DynamicPointerBase::clear(static_cast<DynamicBase*>(pdyn));
		}
	}
	DynamicT* release() const{
		DynamicT *tmp = pdyn;
		pdyn = NULL;return tmp;
	}
	DynamicPointerT& operator=(const DynamicPointerT &_rcp){
		DynamicT *p(_rcp.release());
		if(this == &_rcp){
			return *this;
		}
		if(pdyn) clear();
		pdyn = p;//we inherit the usecount
		ctx = _rcp.ctx;
		return *this;
	}
	template <class O>
	DynamicPointerT& operator=(const DynamicPointer<O> &_rcp){
		DynamicT *p(_rcp.release());
		
		if(pdyn) clear();
		pdyn = p;//we inherit the usecount
		return *this;
	}
	template <class O, class Ct>
	DynamicPointerT& operator=(const DynamicPointer<O, Ct> &_rcp){
		DynamicT *p(_rcp.release());
		if(this == &_rcp){
			return *this;
		}
		if(pdyn) clear();
		pdyn = p;//we inherit the usecount
		ctx = _rcp.ctx;
		return *this;
	}
	template <class O>
	DynamicPointerT& operator=(const DynamicSharedPointer<O> &_rcp){
		DynamicT *p(_rcp.release());
		if(p == pdyn){
			return *this;
		}
		if(pdyn) clear();
		set(p);
		return *this;
	}
	DynamicPointerT& operator=(DynamicT *_pdyn){
		if(_pdyn == pdyn){
			return *this;
		}
		clear();
		set(_pdyn);
		return *this;
	}
	DynamicT& operator*()const	{
		storeSpecific();
		return *pdyn;
	}
	DynamicT* operator->()const	{
		storeSpecific();
		return pdyn;
	}
	DynamicT* get() const{
		storeSpecific();
		return pdyn;
	}
	//operator bool () const	{return psig;}
	bool operator!()const		{return !pdyn;}
	void clear(){
		if(pdyn){
			DynamicPointerBase::clear(static_cast<DynamicBase*>(pdyn));
			pdyn = NULL;
		}
	}
	ContextT& context(){
		return ctx;
	}
	const ContextT& context()const{
		return ctx;
	}
	static ContextT& specificContext(){
		return *reinterpret_cast<ContextT*>(fetchSpecific());
	}
	void storeSpecific()const{
		DynamicPointerBase::storeSpecific(&ctx);
	}
protected:
	void set(DynamicT *_pdyn){
		pdyn = _pdyn;
		if(pdyn){
			use(static_cast<DynamicBase*>(pdyn));
		}
	}
private:
	mutable DynamicT	*pdyn;
	mutable ContextT	ctx;
};

#endif

}//namespace solid

#endif
