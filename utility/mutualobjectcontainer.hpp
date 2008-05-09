/* Declarations file mutualobjectcontainer.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MUTUALOBJECTCONTAINER_HPP
#define MUTUALOBJECTCONTAINER_HPP

#include "common.hpp"
#include "system/convertors.hpp"

//! A container of shared objects
/*!
	Here's the concrete situation for which i've designed this container.
	
	Suppose you have lots of objects in a vector. Also suppose that every
	this object needs an associated mutex. More over such a mutex can be
	shared by multiple objects, and you dont want all mutexez created at once.
	
	MutualObjectContainer does:<br>
	- associate M indexes (a range of indexes) to a mutex/an object;
	- allocates the mutexes by rows of N mutexes/objects
	- allocates no more than R rows
	
	So what happens when the matrix is full but the ids keep increasing?<br>
	- Then it will start allover with the mutex(0,0)
	
	In the end Ive decided that instead of a mutext I should have any type I want,
	so the template\<Obj\> MutualObjectContainer appeared.
*/
template <class Obj>
class MutualObjectContainer{
public:
	typedef Obj MutualObjectTp;
	//!Constructor
	/*!
		\param _objpermutbts The number of objects associated to a mutex in bitcount 
		(real count 1<<bitcount)
		\param _mutrowsbts The number of mutex rows in bitcount (real count 1<<bitcount)
		\param _mutcolssbts The number of mutexes in a row in bitcount (real count 1<<bitcount)
	*/
	MutualObjectContainer(
		unsigned _objpermutbts = 6, 
		unsigned _mutrowsbts = 8, 
		unsigned _mutcolsbts = 8
	 ):
		objpermutbts(_objpermutbts),
		objpermutmsk(bitsToMsk(_objpermutbts)),
		mutrowsbts(_mutrowsbts),
		mutrowsmsk(bitsToMsk(_mutrowsbts)),
		mutrowscnt(bitsToCnt(_mutrowsbts)),
		mutcolsmsk(bitsToMsk(_mutcolsbts)),
		mutcolscnt(bitsToCnt(_mutcolsbts)),
		objmat(new MutualObjectTp*[mutrowscnt])
	{
		for(uint i = 0; i < mutrowscnt; ++i) objmat[i] = NULL;
	}
	~MutualObjectContainer(){
		for(uint i(0); i < mutrowscnt; ++i){
			delete []objmat[i];
		}
		delete []objmat;
	}
	//! Fast but unsafe get the mutex for a position
	/*!
		Use this after calling safeObject once for a certain position.
	*/
	inline MutualObjectTp& object(unsigned i){
		return doGetObject(i >> objpermutbts);
	}
	//! Slower but safe get the mutex for a position
	/*!
		It will reallocate new mutexes if needed
	*/
	MutualObjectTp& safeObject(unsigned i){
		int mrow = getObjectRow(i);
		if(!objmat[mrow]){
			objmat[mrow] = new MutualObjectTp[mutcolscnt];
		}
		return object(i);
	}
	//! Gets the mutex at pos i (the matrix is seen as a vector)
	inline MutualObjectTp& operator[](unsigned i){
		return doGetObject(i);
	}
	//! Will return true if i is the first index of a range sharing the same mutex
	inline int isRangeBegin(unsigned i){
		return !(i & objpermutmsk);
	}
private:
	inline MutualObjectTp& doGetObject(unsigned i){
		return objmat[(i >> mutrowsbts) & mutrowsmsk][i & mutcolsmsk];
	}
	inline unsigned getObjectRow(unsigned i){
		return ((i >> objpermutbts) >> mutrowsbts) & mutrowsmsk;
	}
private:
	const unsigned		objpermutbts;//objects per mutex mask
	const unsigned		objpermutmsk;//objects per mutex count
	const unsigned 		mutrowsbts;//mutex rows bits
	const unsigned		mutrowsmsk;//mutex rows mask
	const unsigned		mutrowscnt;//mutex rows count
	const unsigned 		mutcolsmsk;//mutex columns mask
	const unsigned 		mutcolscnt;//mutex columns count
	MutualObjectTp		**objmat;
};

#endif
