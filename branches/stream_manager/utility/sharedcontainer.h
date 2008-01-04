/* Declarations file sharedcontainer.h
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SHAREDCONTAINER_H
#define SHAREDCONTAINER_H

#include "common.h"
#include "system/convertors.h"

template <class Obj>
class SharedContainer{
public:
	typedef Obj ObjectTp;
	SharedContainer(unsigned _objpermutbts = 6, 
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
					objmat(new ObjectTp*[mutrowscnt]){
		for(uint i = 0; i < mutrowscnt; ++i) objmat[i] = NULL;
	}
	~SharedContainer(){
		for(uint i(0); i < mutrowscnt; ++i){
			delete []objmat[i];
		}
		delete []objmat;
	}
	inline ObjectTp& object(unsigned i){
		return doGetObject(i >> objpermutbts);
	}
	ObjectTp& safeObject(unsigned i){
		int mrow = getObjectRow(i);
		if(!objmat[mrow]){
			objmat[mrow] = new ObjectTp[mutcolscnt];
		}
		return object(i);
	}
	inline ObjectTp& operator[](unsigned i){
		return doGetObject(i);
	}
	inline int isRangeEnd(unsigned i){
		return !(i & objpermutmsk);
	}
private:
	inline ObjectTp& doGetObject(unsigned i){
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
	ObjectTp			**objmat;
};

#endif
