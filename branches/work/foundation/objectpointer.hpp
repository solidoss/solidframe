/* Declarations file objectpointer.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FOUNDATION_OBJECTPTR_HPP
#define FOUNDATION_OBJECTPTR_HPP

#include "foundation/common.hpp"

struct B;
namespace foundation{

class Object;

struct ObjectPointerBase{
protected:
	void clear(Object *_pobj);
	void use(Object *_pobj);
	void destroy(Object *_pobj);
};

//! An autoptr style smartpointer for objects.
template <class SO = Object>
class ObjectPointer: protected ObjectPointerBase{
public:
	typedef SO 					ObjectT;
	typedef ObjectPointer<SO>	ThisT;
public:
	explicit ObjectPointer(ObjectT *_pobj = NULL):pobj(_pobj) {
		if(_pobj){
			use(static_cast<Object*>(_pobj));
		}
	}
	
	ObjectPointer(const ThisT &_pobj):pobj(_pobj.release()){}
	
	template <class B>
	ObjectPointer(const ObjectPointer<B> &_rop):pobj(_rop.release()){
	}
	
	~ObjectPointer(){
		if(pobj){
			ObjectPointerBase::clear(static_cast<Object*>(pobj));
		}
	}
	ObjectT* release() const{
		ObjectT *po = pobj;
		pobj = NULL;
		return po;
	}
	ThisT& operator=(const ThisT &_robj){
		clear();
		pobj = _robj.release();
		return *this;
	}
	ThisT& operator=(ObjectT *_pobj){
		clear();ptr(_pobj);	return *this;
	}
	ObjectT& operator*()const  {return *pobj;}
	ObjectT* operator->()const {return pobj;}
	ObjectT* ptr()	const	{return pobj;}
	operator bool ()const	{return pobj != NULL;}
	bool operator!()const	{return pobj == NULL;}
	void clear(){
		if(pobj){
			ObjectPointerBase::clear(static_cast<Object*>(pobj));
			pobj = NULL;
		}
	}
protected:
	void ptr(ObjectT *_pobj){
		pobj = _pobj;
		if(_pobj){
			use(static_cast<Object*>(pobj));
		}
	}
private:
	mutable ObjectT 	*pobj;
};

}//namespace foundation
#endif

