// utility/dynamicpointer.hpp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com) 
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
};


template <class T = DynamicBase>
class DynamicPointer;


template <class T>
class DynamicPointer: DynamicPointerBase{
public:
	typedef DynamicPointer<T>			ThisT;
	typedef T							DynamicT;
	typedef T							element_type;
	
	DynamicPointer(DynamicT *_pdyn = NULL):pdyn(_pdyn){
		if(_pdyn){
			use(static_cast<DynamicBase*>(_pdyn));
		}
	}
	template <class B>
	explicit DynamicPointer(const DynamicPointer<B> &_rcp):pdyn(static_cast<T*>(_rcp.get())){
		if(pdyn){
			use(static_cast<DynamicBase*>(pdyn));
		}
	}
	
	//The copy constructor must be specified - the compiler wount consider the above constructor as copy-constructor
	DynamicPointer(const ThisT &_rcp):pdyn(static_cast<T*>(_rcp.get())){
		if(pdyn){
			use(static_cast<DynamicBase*>(pdyn));
		}
	}
	
	~DynamicPointer(){
		if(pdyn){
			DynamicPointerBase::clear(static_cast<DynamicBase*>(pdyn));
		}
	}
	template <class O>
	ThisT& operator=(const DynamicPointer<O> &_rcp){
		DynamicT *p(_rcp.get());
		if(p == pdyn){
			return *this;
		}
		if(pdyn) clear();
		set(p);
		return *this;
	}

	ThisT& operator=(DynamicT *_pdyn){
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
	bool empty()const			{return !pdyn;}
	bool operator!()const		{return empty();}
	
	void clear(){
		if(pdyn){
			DynamicPointerBase::clear(static_cast<DynamicBase*>(pdyn));
			pdyn = NULL;
		}
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

}//namespace solid

#endif
