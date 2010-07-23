/* Declarations file dynamicpointer.hpp
	
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

#ifndef SYSTEM_DYNAMIC_POINTER_HPP
#define SYSTEM_DYNAMIC_POINTER_HPP

#include "system/common.hpp"

struct DynamicBase;

class DynamicPointerBase{
protected:
	void clear(DynamicBase *_pdyn);
	void use(DynamicBase *_pdyn);

};
//! An autoptr like smartpointer for DynamicBase objects
template <class T = DynamicBase>
class DynamicPointer: DynamicPointerBase{
public:
	typedef DynamicPointer<T>	DynamicPointerT;
	typedef T					DynamicT;
	
	explicit DynamicPointer(DynamicT *_pdyn = NULL):pdyn(_pdyn){
		if(_pdyn) use(static_cast<DynamicBase*>(_pdyn));
	}
	template <class B>
	explicit DynamicPointer(const DynamicPointer<B> &_rcp):pdyn(static_cast<T*>(_rcp.release())){}
	
	DynamicPointer(const DynamicPointerT &_rcp):pdyn(_rcp.release()){}
	~DynamicPointer(){
		if(pdyn){DynamicPointerBase::clear(static_cast<DynamicBase*>(pdyn));}
	}
	DynamicT* release() const{
		DynamicT *tmp = pdyn;
		pdyn = NULL;return tmp;
	}
	DynamicPointerT& operator=(const DynamicPointerT &_rcp){
		if(pdyn) clear();
		pdyn = _rcp.release();
		return *this;
	}
	DynamicPointerT& operator=(DynamicT *_pdyn){
		clear();ptr(_pdyn);	return *this;
	}
	DynamicT& operator*()const {return *pdyn;}
	DynamicT* operator->()const{return pdyn;}
	DynamicT* ptr() const		{return pdyn;}
	//operator bool () const	{return psig;}
	bool operator!()const	{return !pdyn;}
	void clear(){if(pdyn) DynamicPointerBase::clear(static_cast<DynamicBase*>(pdyn));pdyn = NULL;}
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


#endif
