/* Declarations file objptr.hpp
	
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

#ifndef CS_OBJECTPTR_HPP
#define CS_OBJECTPTR_HPP

#include "common.hpp"

namespace clientserver{

class Object;

struct ObjPtrBase{
protected:
	void clear(Object *_pobj);
	void use(Object *_pobj);
	void destroy(Object *_pobj);
};

//! An autoptr style smartpointer for objects.
template <class SO>
class ObjPtr: protected ObjPtrBase{
public:
	typedef SO 			ObjectTp;
	typedef ObjPtr<SO>	ThisTp;
public:
	ObjPtr():pobj(NULL){}
	
	explicit ObjPtr(ObjectTp *_pobj):pobj(_pobj) {
		if(_pobj) use(static_cast<Object*>(_pobj));
	}
	
	ObjPtr(const ThisTp &_pobj):pobj(_pobj.release()){}
	
	~ObjPtr(){
		if(pobj){ObjPtrBase::clear(static_cast<Object*>(pobj));}
	}
	ObjectTp* release() const{
		ObjectTp *po = pobj;
		pobj = NULL;
		return po;
	}
	ThisTp& operator=(const ThisTp &_robj){
		clear();
		pobj = _robj.release();
		return *this;
	}
	ThisTp& operator=(ObjectTp *_pobj){
		clear();ptr(_pobj);	return *this;
	}
	ObjectTp& operator*()const  {return *pobj;}
	ObjectTp* operator->()const {return pobj;}
	ObjectTp* ptr() const  {return pobj;}
	operator bool () const {return pobj;}
	bool operator!()const  {return !pobj;}
	void clear(){if(pobj){ObjPtrBase::clear(static_cast<Object*>(pobj));pobj = NULL;}}
protected:
	void ptr(ObjectTp *_pobj){
		pobj = _pobj;
		use(static_cast<Object*>(pobj));
	}
private:
	mutable ObjectTp 	*pobj;
};

}//namespace clientserver
#endif

