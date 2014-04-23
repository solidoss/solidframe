// utility/polycontainer.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef UTILITY_POLYCONTAINER_HPP
#define UTILITY_POLYCONTAINER_HPP

#include "system/common.hpp"

namespace solid{

class MultiContainer{	
	typedef void (*FncT) (void*);
public:
#ifdef HAS_SAFE_STATIC
	template <typename T>
	T* get(T *_p = NULL){
		static unsigned id(stackid(&MultiContainer::cleaner<T>));
		if(_p){
			if(push(_p, id)){ delete _p; _p = NULL;}
		}else{
			_p = reinterpret_cast<T*>(get(id));
		}
		return _p;
	}
#else
private:
	template <typename T>
	T* getStub(T *_p = NULL){
		static unsigned id(stackid(&MultiContainer::cleaner<T>));
		if(_p){
			if(push(_p, id)){ delete _p; _p = NULL;}
		}else{
			_p = reinterpret_cast<T*>(get(id));
		}
		return _p;
	}
	template <typename T>
	void once_cbk(){
		T *p = getStub<T>();
		getStub(p);
	}
public:
	template <typename T>
	T* get(T *_p = NULL){
		static boost::once_flag once = BOOST_ONCE_INIT;
		boost::call_once(&once_cbk<T>, once);
		return getStub<T>(_p);
	}
		
#endif
private:
	static unsigned objectid(FncT _pf);
	template<class T>
	static void cleaner(void *_p){
		delete reinterpret_cast<T*>(_p);
	}
	bool push(void *_p, unsigned _id);
	void* get(unsigned _id);
};


template <class T>
struct PolyKeeper;

template <class T>
struct PolyKeeper<T*>{
	PolyKeeper():val(NULL){}
	~PolyKeeper(){delete val;}
	T	*val;
};

template <class T>
struct PolyKeeper{
	T	val;
};
template <class H, class T>
struct PolyLayer;

template <class H, class T>
struct PolyLayer: PolyKeeper<H>, T{};

template <class L>
struct PolyContainer: L{
	template <class T>
	T &get(){
		return static_cast<PolyKeeper<T>* >(this)->val;
	}
};

template <class L>
struct ConstPolyContainer: L{
	template <class T>
	T const &get(){
		return static_cast<PolyKeeper<T>* >(this)->val;
	}
protected:
	template <class T>
	T &set(){
		return static_cast<PolyKeeper<T>* >(this)->val;
	}
};

}//namespace solid

#define POLY1(a1)\
	PolyContainer<PolyLayer<a1, NullType> >
#define POLY2(a1,a2)\
	PolyContainer<PolyLayer<a1, PolyLayer<a2, NullType> > >
#define POLY3(a1,a2,a3)\
	PolyContainer<PolyLayer<a1, PolyLayer<a2, PolyLayer<a3, NullType> > > >
#define POLY4(a1,a2,a3,a4)\
	PolyContainer<PolyLayer<a1, PolyLayer<a2, PolyLayer<a3, PolyLayer<a4, NullType> > > > >
#define POLY5(a1,a2,a3,a4,a5)\
	PolyContainer<PolyLayer<a1, PolyLayer<a2, PolyLayer<a3, PolyLayer<a4, PolyLayer<a5, NullType> > > > > >
#define POLY6(a1,a2,a3,a4,a5,a6)\
	PolyContainer<PolyLayer<a1, PolyLayer<a2, PolyLayer<a3, PolyLayer<a4, PolyLayer<a5, PolyLayer<a6, NullType> > > > > > >

#define CONSTPOLY1(a1)\
	ConstPolyContainer<PolyLayer<a1, NullType> >
#define CONSTPOLY2(a1,a2)\
	ConstPolyContainer<PolyLayer<a1, PolyLayer<a2, NullType> > >
#define CONSTPOLY3(a1,a2,a3)\
	ConstPolyContainer<PolyLayer<a1, PolyLayer<a2, PolyLayer<a3, NullType> > > >
#define CONSTPOLY4(a1,a2,a3,a4)\
	ConstPolyContainer<PolyLayer<a1, PolyLayer<a2, PolyLayer<a3, PolyLayer<a4, NullType> > > > >
#define CONSTPOLY5(a1,a2,a3,a4,a5)\
	ConstPolyContainer<PolyLayer<a1, PolyLayer<a2, PolyLayer<a3, PolyLayer<a4, PolyLayer<a5, NullType> > > > > >
#define CONSTPOLY6(a1,a2,a3,a4,a5,a6)\
	ConstPolyContainer<PolyLayer<a1, PolyLayer<a2, PolyLayer<a3, PolyLayer<a4, PolyLayer<a5, PolyLayer<a6, NullType> > > > > > >


#endif
