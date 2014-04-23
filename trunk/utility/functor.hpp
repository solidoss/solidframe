// utility/functor.hpp
//
// Copyright (c) 2010, 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_FUNCTOR_HPP
#define UTILITY_FUNCTOR_HPP

#include "system/common.hpp"

namespace solid{

//-----------------------------------------------------------------
//	FunctorReference
//-----------------------------------------------------------------


template <
	class R = void,
	class P1 = void, class P2 = void,
	class P3 = void, class P4 = void
>
class FunctorReference;

template <typename T>
struct FunctorRefStub{
	T	&ref;
	FunctorRefStub(T &_ref):ref(_ref){}
};

template <
	class R
>
class FunctorReference<R, void, void, void, void>{
	explicit FunctorReference(const FunctorReference&);
	typedef FunctorReference<R, void, void, void, void>	FunctorReferenceT;
	typedef R (*CallFncT)(void*);
	typedef void (*DelFncT)(void*);
	enum{
		DataSizeT = sizeof(FunctorRefStub<int>)
	};
	template <typename T>
	static R call(void *_pf){
		T *pf = reinterpret_cast<T*>(_pf);
		return pf->ref();
	}
	
	template <typename T>
	static void del(void *_pf){
		T *pf = reinterpret_cast<T*>(_pf);
		pf->~T();
	}
	
	CallFncT	pcallfnc;
	DelFncT		pdelfnc;
	char 		d[DataSizeT];
public:
	template <class F>
	explicit FunctorReference(F &_f){
		/*FunctorRefStub<F>	*pfr = */new(d) FunctorRefStub<F>(_f);
		pcallfnc = &call<FunctorRefStub<F> >;
		pdelfnc = &del<FunctorRefStub<F> >;
	}
	~FunctorReference(){
		(*pdelfnc)(d);
	}
	
	R operator()(){
		return (*pcallfnc)(d);
	}
};


template <
	class R, class P1
>
class FunctorReference<R, P1, void, void, void>{
	explicit FunctorReference(const FunctorReference&);
	typedef FunctorReference<R, P1, void, void, void>	FunctorReferenceT;
	typedef R (*CallFncT)(void*, P1);
	typedef void (*DelFncT)(void*);
	enum{
		DataSizeT = sizeof(FunctorRefStub<int>)
	};
	
	template <typename T>
	static R call(void *_pf, P1 _p1){
		T *pf = reinterpret_cast<T*>(_pf);
		return pf->ref(_p1);
	}
	
	template <typename T>
	static void del(void *_pf){
		T *pf = reinterpret_cast<T*>(_pf);
		pf->~T();
	}
	
	CallFncT	pcallfnc;
	DelFncT		pdelfnc;
	char 		d[DataSizeT];
public:
	template <class F>
	explicit FunctorReference(F &_f){
		/*FunctorRefStub<F>	*pfr = */new(d) FunctorRefStub<F>(_f);
		pcallfnc = &call<FunctorRefStub<F> >;
		pdelfnc = &del<FunctorRefStub<F> >;
	}
	~FunctorReference(){
		(*pdelfnc)(d);
	}
	
	R operator()(P1 _p1){
		return (*pcallfnc)(d, _p1);
	}
};

template <
	class R, class P1, class P2
>
class FunctorReference<R, P1, P2, void, void>{
	explicit FunctorReference(const FunctorReference&);
	typedef FunctorReference<R, P1, P2, void, void>	FunctorReferenceT;
	typedef R (*CallFncT)(void*, P1, P2);
	typedef void (*DelFncT)(void*);
	enum{
		DataSizeT = sizeof(FunctorRefStub<int>)
	};
	
	template <typename T>
	static R call(void *_pf, P1 _p1, P2 _p2){
		T *pf = reinterpret_cast<T*>(_pf);
		return pf->ref(_p1, _p2);
	}
	
	template <typename T>
	static void del(void *_pf){
		T *pf = reinterpret_cast<T*>(_pf);
		pf->~T();
	}
	
	CallFncT	pcallfnc;
	DelFncT		pdelfnc;
	char 		d[DataSizeT];
public:
	template <class F>
	explicit FunctorReference(F &_f){
		/*FunctorRefStub<F>	*pfr = */new(d) FunctorRefStub<F>(_f);
		pcallfnc = &call<FunctorRefStub<F> >;
		pdelfnc = &del<FunctorRefStub<F> >;
	}
	~FunctorReference(){
		(*pdelfnc)(d);
	}
	
	R operator()(P1 _p1, P2 _p2){
		return (*pcallfnc)(d, _p1, _p2);
	}
};

template <
	class R, class P1, class P2, class P3
>
class FunctorReference<R, P1, P2, P3, void>{
	explicit FunctorReference(const FunctorReference&);
	typedef FunctorReference<R, P1, P2, P3, void>	FunctorReferenceT;
	typedef R (*CallFncT)(void*, P1, P2, P3);
	typedef void (*DelFncT)(void*);
	enum{
		DataSizeT = sizeof(FunctorRefStub<int>)
	};
	
	template <typename T>
	static R call(void *_pf, P1 _p1, P2 _p2, P3 _p3){
		T *pf = reinterpret_cast<T*>(_pf);
		return pf->ref(_p1, _p2, _p3);
	}
	
	template <typename T>
	static void del(void *_pf){
		T *pf = reinterpret_cast<T*>(_pf);
		pf->~T();
	}
	
	CallFncT	pcallfnc;
	DelFncT		pdelfnc;
	char 		d[DataSizeT];
public:
	template <class F>
	explicit FunctorReference(F &_f){
		/*FunctorRefStub<F>	*pfr = */new(d) FunctorRefStub<F>(_f);
		pcallfnc = &call<FunctorRefStub<F> >;
		pdelfnc = &del<FunctorRefStub<F> >;
	}
	~FunctorReference(){
		(*pdelfnc)(d);
	}
	
	R operator()(P1 _p1, P2 _p2, P3 _p3){
		return (*pcallfnc)(d, _p1, _p2, _p3);
	}
};

template <
	class R, class P1, class P2, class P3, class P4
>
class FunctorReference{
	explicit FunctorReference(const FunctorReference&);
	typedef FunctorReference<R, P1, P2, P3, P4>	FunctorReferenceT;
	typedef R (*CallFncT)(void*, P1, P2, P3, P4);
	typedef void (*DelFncT)(void*);
	enum{
		DataSizeT = sizeof(FunctorRefStub<int>)
	};
	
	template <typename T>
	static R call(void *_pf, P1 _p1, P2 _p2, P3 _p3, P4 _p4){
		T *pf = reinterpret_cast<T*>(_pf);
		return pf->ref(_p1, _p2, _p3, _p4);
	}
	
	template <typename T>
	static void del(void *_pf){
		T *pf = reinterpret_cast<T*>(_pf);
		pf->~T();
	}
	
	CallFncT	pcallfnc;
	DelFncT		pdelfnc;
	char 		d[DataSizeT];
public:
	template <class F>
	explicit FunctorReference(F &_f){
		/*FunctorRefStub<F>	*pfr = */new(d) FunctorRefStub<F>(_f);
		pcallfnc = &call<FunctorRefStub<F> >;
		pdelfnc = &del<FunctorRefStub<F> >;
	}
	~FunctorReference(){
		(*pdelfnc)(d);
	}
	
	R operator()(P1 _p1, P2 _p2, P3 _p3, P4 _p4){
		return (*pcallfnc)(d, _p1, _p2, _p3, _p4);
	}
};

//-----------------------------------------------------------------
//	TODO: FunctorPointer
//-----------------------------------------------------------------


}//namespace solid

#endif
