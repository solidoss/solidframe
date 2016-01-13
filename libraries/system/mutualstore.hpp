// mutualstore.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_MUTUALSTORE_HPP
#define SYSTEM_MUTUALSTORE_HPP

#include "system/common.hpp"
#include "system/convertors.hpp"


namespace solid{

//! A container of shared objects
/*!
	Here's the concrete situation for which this object store was designed for:<br><br>
	
	Suppose you have lots of objects in a vector. Also suppose that every
	this object needs an associated mutex. More over such a mutex can be
	shared by multiple objects, and you dont want all mutexez created at once.
	
	MutualSStore does:<br>
	- associate M indexes (a range of indexes) to a mutex/an object;
	- allocates the mutexes by rows of N mutexes/objects
	- allocates no more than R rows
	
	So what happens when the matrix is full but the ids keep increasing?<br>
	- Then it will start allover with the mutex(0,0)
	
	In the end Ive decided that instead of a mutext I should have any type I want,
	so the template\<Obj\> MutualStore appeared.
*/
template <class Obj>
class MutualStore{
public:
	typedef Obj MutualObjectT;
	//!Constructor
	/*!
		\param _objpermutbts The number of objects associated to a mutex as bitcount 
		(real count 1<<bitcount)
		\param _mutrowsbts The number of mutex rows as bitcount (real count 1<<bitcount)
		\param _mutcolsbts The number of mutexes in a row as bitcount (real count 1<<bitcount)
	*/
	MutualStore(
		const bool _preload = false,
		unsigned _objpermutbts = 6,
		unsigned _mutrowsbts = 3,
		unsigned _mutcolsbts = 3
	 ):
		objpermutbts(_objpermutbts),
		objpermutmsk(bitsToMask(_objpermutbts)),
		mutrowsbts(_mutrowsbts),
		mutrowsmsk(bitsToMask(_mutrowsbts)),
		mutrowscnt(bitsToCount(_mutrowsbts)),
		mutcolsmsk(bitsToMask(_mutcolsbts)),
		mutcolscnt(bitsToCount(_mutcolsbts)),
		objmat(new MutualObjectT*[mutrowscnt])
	{
		for(uint i = 0; i < mutrowscnt; ++i){
			objmat[i] = _preload ? new MutualObjectT[mutcolscnt] : NULL;
		}
	}
	
	~MutualStore(){
		for(uint i(0); i < mutrowscnt; ++i){
			delete []objmat[i];
		}
		delete []objmat;
	}
	//! Fast but unsafe get the mutex for a position
	/*!
		Use this after calling safeObject once for a certain position.
	*/
	inline MutualObjectT& at(const size_t i){
		return doGetObject(i >> objpermutbts);
	}
	inline MutualObjectT& at(const size_t i, const unsigned _objpermutbts){
		return doGetObject(i >> objpermutbts);
	}
	inline MutualObjectT const& at(const size_t i)const{
		return doGetObject(i >> objpermutbts);
	}
	inline MutualObjectT const& at(const size_t i, const unsigned _objpermutbts)const{
		return doGetObject(i >> _objpermutbts);
	}
	//! Slower but safe get the mutex for a position
	/*!
		It will reallocate new mutexes if needed
	*/
	MutualObjectT& safeAt(const size_t i){
		const size_t mrow = getObjectRow(i);
		if(!objmat[mrow]){
			objmat[mrow] = new MutualObjectT[mutcolscnt];
		}
		return at(i);
	}
	MutualObjectT& safeAt(const size_t i, const unsigned _objpermutbts){
		const size_t mrow = getObjectRow(i, _objpermutbts);
		if(!objmat[mrow]){
			objmat[mrow] = new MutualObjectT[mutcolscnt];
		}
		return at(i);
	}
	//! Gets the mutex at pos i (the matrix is seen as a vector)
	inline MutualObjectT& operator[](const size_t i){
		return doGetObject(i);
	}
	inline MutualObjectT const& operator[](const size_t i)const{
		return doGetObject(i);
	}
	//! Will return true if i is the first index of a range sharing the same mutex
	inline bool isRangeBegin(const size_t i)const{
		return !(i & objpermutmsk);
	}
	inline bool isRangeBegin(const size_t i, const size_t _objpermutmsk)const{
		return !(i & _objpermutmsk);
	}
	template <typename V>
	void visit(const size_t _upto, V &_rv){
		const size_t mutcnt = mutrowscnt * mutcolscnt;
		const size_t cnt = _upto >> objpermutbts/* ? (_upto >> objpermutbts) + 1 : 0*/;
		const size_t end = cnt > mutcnt ? mutcnt : cnt;
		for(size_t i(0); i < end; ++i){
			_rv(doGetObject(i));
		}
	}
	template <typename V>
	void visit(const size_t _upto, V &_rv)const{
		const size_t mutcnt = mutrowscnt * mutcolscnt;
		const size_t cnt = _upto >> objpermutbts/* ? (_upto >> objpermutbts) + 1 : 0*/;
		const size_t end = cnt > mutcnt ? mutcnt : cnt;
		for(size_t i(0); i < end; ++i){
			_rv(doGetObject(i));
		}
	}
	template <typename V>
	void visit(const size_t _upto, V &_rv, const unsigned _objpermutbts){
		const size_t mutcnt = mutrowscnt * mutcolscnt;
		const size_t cnt = _upto >> _objpermutbts/* ? (_upto >> _objpermutbts) + 1 : 0*/;
		const size_t end = cnt > mutcnt ? mutcnt : cnt;
		for(size_t i(0); i < end; ++i){
			_rv(doGetObject(i/*, _objpermutbts*/));
		}
	}
	template <typename V>
	void visit(const size_t _upto, V &_rv, const unsigned _objpermutbts)const{
		const size_t mutcnt = mutrowscnt * mutcolscnt;
		const size_t cnt = _upto >> _objpermutbts/* ? (_upto >> _objpermutbts) + 1 : 0*/;
		const size_t end = cnt > mutcnt ? mutcnt : cnt;
		for(size_t i(0); i < end; ++i){
			_rv(doGetObject(i/*, _objpermutbts*/));
		}
	}
	size_t rowRangeSize()const{
		return mutcolscnt * bitsToCount(objpermutbts);
	}
private:
	inline MutualObjectT& doGetObject(const size_t i){
		return objmat[(i >> mutrowsbts) & mutrowsmsk][i & mutcolsmsk];
	}
	inline const MutualObjectT& doGetObject(const size_t i)const{
		return objmat[(i >> mutrowsbts) & mutrowsmsk][i & mutcolsmsk];
	}
	inline size_t getObjectRow(const size_t i){
		return ((i >> objpermutbts) >> mutrowsbts) & mutrowsmsk;
	}
	inline size_t getObjectRow(const size_t i)const{
		return ((i >> objpermutbts) >> mutrowsbts) & mutrowsmsk;
	}
	
	inline size_t getObjectRow(const size_t i, const unsigned _objpermutbts){
		return ((i >> _objpermutbts) >> mutrowsbts) & mutrowsmsk;
	}
	inline size_t getObjectRow(const size_t i, const unsigned _objpermutbts)const{
		return ((i >> _objpermutbts) >> mutrowsbts) & mutrowsmsk;
	}
private:
	const unsigned		objpermutbts;//objects per mutex mask
	const size_t		objpermutmsk;//objects per mutex count
	const unsigned 		mutrowsbts;//mutex rows bits
	const size_t		mutrowsmsk;//mutex rows mask
	const size_t		mutrowscnt;//mutex rows count
	const size_t 		mutcolsmsk;//mutex columns mask
	const size_t 		mutcolscnt;//mutex columns count
	MutualObjectT		**objmat;
};

}//namespace solid

#endif
